#include "common.h"

string server_id = "";
string curr_id = "";

class SNSServiceImpl final : public SNSService::Service {
  
  Status List(ServerContext* context, const Request* request, Reply* reply) override {
    // ------------------------------------------------------------
    // In this function, you are to write code that handles 
    // LIST request from the user. Ensure that both the fields
    // all_users & following_users are populated
    // ------------------------------------------------------------
    string username = request->username();
    ifstream ifs("list_" + curr_id + ".csv");
    if (!ifs.is_open()){
      return Status::OK;
    }

    string user;
    while(getline(ifs, user, ',')){
      reply -> add_all_users(user);
    }
    ifs.close();

    string followers;
    ifs.open(server_id + "/" + username + "/" + username + "_followers.csv");
    int i = 0;
    while(getline(ifs, followers, ',')){
      reply->add_following_users(followers);
    }
    ifs.close();

    reply->set_msg("SUCCESS");
    return Status::OK;
  }

  Status Follow(ServerContext* context, const Request* request, Reply* reply) override {
    // ------------------------------------------------------------
    // In this function, you are to write code that handles 
    // request from a user to follow one of the existing
    // users
    // ------------------------------------------------------------
    string username = request->username();
    string args;

    for (auto x: request->arguments()){
      args += x;
    }
    if (username == args){
      reply->set_msg("FAILURE_INVALID_USERNAME");
      return Status::OK;
    }

    // Determine is the username is in the same cluster
    ofstream ofs;
    if (stoi(curr_id)-1 == stoi(args) % 3){

      ifstream ifs(server_id + "/" + args + "/" + args + "_followers.csv");
      if (!ifs.is_open()){
        reply->set_msg("FAILURE_NOT_EXISTS");
        return Status::OK; 
      }

      string followers;

      while(getline(ifs, followers, ',')){
        if (followers == username){
          reply -> set_msg("FAILURE_ALREADY_EXISTS");
          return Status::OK;
        }
      }
      ifs.close();

      ofs.open(server_id + "/" + args + "/" + args + "_followers.csv", ios_base::app);
      ofs << username << ',';
      ofs.close();

      ofs.open(server_id + "/" + username + "/" + username + "_following.csv", ios_base::app);
      ofs << args << ',';
      ofs.close();

    }
    else {

      string following;
      ifstream ifs(server_id + "/" + username + "/" + username + "_requests.csv");
      while(getline(ifs, following, ',')){
        if (following == args){
          reply -> set_msg("FAILURE_ALREADY_EXISTS");
          return Status::OK;
        }
      }
      ifs.close();

      ifs.open(server_id + "/" + username + "/" + username + "_following.csv");
      while(getline(ifs, following, ',')){
        if (following == args){
          reply -> set_msg("FAILURE_ALREADY_EXISTS");
          return Status::OK;
        }
      }

      ofs.open(server_id + "/" + username + "/" + username + "_requests.csv", ios_base::app);
      ofs << args << ',';
      ofs.close();

    }
    
    return Status::OK;
  }

  Status UnFollow(ServerContext* context, const Request* request, Reply* reply) override {
    // ------------------------------------------------------------
    // In this function, you are to write code that handles 
    // request from a user to unfollow one of his/her existing
    // followers
    // ------------------------------------------------------------
    string username = request->username();
    string args;
    for (auto x: request->arguments()){
      args += x;
    }
    if (username == args){
      reply->set_msg("FAILURE_INVALID_USERNAME");
      return Status::OK;
    }

    ifstream ifs(server_id + "/" + args + ".txt");
    if (!ifs.is_open()){
      reply->set_msg("FAILURE_NOT_EXISTS");
      return Status::OK; 
    }

    string args_username;
    getline(ifs, args_username);

    string args_followers;
    getline(ifs, args_followers);
    bool exists = false;
    int pos = args_followers.find(username);
    if (pos != string::npos){
      args_followers.erase(pos, args.length()+2);
      exists = true;
    }

    string args_following;
    getline(ifs, args_following);

    ofstream ofs(server_id + "/" + args + ".txt");
    ofs << args_username << "\n" << args_followers << "\n" << args_following << "\n";

    ofs.close();

    if (exists) {
      reply->set_msg("SUCCESS");
    }
    else {
      reply->set_msg("FAILURE_INVALID_USERNAME");
    }

    return Status::OK;
  }
  
  Status Login(ServerContext* context, const Request* request, Reply* reply) override {
    // ------------------------------------------------------------
    // In this function, you are to write code that handles 
    // a new user and verify if the username is available
    // or already taken
    // ------------------------------------------------------------
    string username = request->username();
    bool exists = false;

    DIR *dr;
    struct dirent *en;
    string path = server_id;
    dr = opendir(path.c_str());
    if (dr){
      while((en = readdir(dr)) != NULL){
        if (((string)(en->d_name)) == (username)){
          exists = true;
        }
      }
      closedir(dr);
    }

    if (!exists){
      string directory = path + "/" + username;
      mkdir(directory.c_str(), 0777);
      ofstream ofs;
      ofs.open(path + "/" + username + "/" + username + "_followers.csv", ios_base::app);
      ofs << username << ",";
      ofs.close();

      ofs.open(path + "/" + username + "/" + username + "_following.csv", ios_base::app);
      ofs << username << ",";
      ofs.close();
    }

    reply->set_msg("SUCCESS");
    return Status::OK;
  }

  Status Timeline(ServerContext* context, ServerReaderWriter<Message, Message>* stream) override {
    // ------------------------------------------------------------
    // In this function, you are to write code that handles 
    // receiving a message/post from a user, recording it in a file
    // and then making it available on his/her follower's streams
    // ------------------------------------------------------------
    // ifstream ifs;
    Message msg;
    stream->Read(&msg);
    string username = msg.username();
    int past_file_size = 0;

    thread recv_msg_thread([stream]{
      Message recv_msg;
      ifstream ifs;
      string username;
      while(stream->Read(&recv_msg)){

        time_t time_now = time(0);
        string current_time = ctime(&time_now);

        username = recv_msg.username();
        ifs.open("/" + server_id + "/" + username + ".txt");
        string followers;
        int i = 0;
        while(getline(ifs, followers)){
          if (i == 1){
            followers.erase(0, 11 + username.size() + 2);
            break;
          }
          ++i;
        }
        ifs.close();

        if (followers.size() > 0){
          vector<string> list_followers;
          size_t pos = 0;

          while((pos = followers.find(',')) != string::npos){
            list_followers.push_back(followers.substr(0, pos));
            followers.erase(0, pos + 2); // need to delete comma and a space
          }

          for (auto x: list_followers){
            vector<string> file_msg;
            ifs.open("/" + server_id + "/" + x + "_timeline.txt");
            if (ifs.is_open()){
              string line_msg;
              while(getline(ifs, line_msg)){
                file_msg.push_back(line_msg);
              }
            }
            ifs.close();

            string clean_msg = recv_msg.msg();
            size_t pos = clean_msg.find('\n');
            if (pos != string::npos){
              clean_msg.erase(clean_msg.begin()+pos);
            }
            
            pos = current_time.find('\n');
            if (pos != string::npos){
              current_time.erase(current_time.begin()+pos);
            }

            ofstream ofs("/" + server_id + "/" + x + "_timeline.txt");
            ofs << username << " *break* " << clean_msg << " *break* " << current_time;
            for (auto k: file_msg){
              ofs << '\n' << k;
            }
            
            ofs.close();
          }
        }
      }
    });

    thread send_msg_thread([stream](string username, int past_file_size) {
      Message send_msg;
      int new_file_size;
      string path = "/" + server_id + "/" + username + "_timeline.txt";
      ifstream ifs;
      int counter = 0;
      bool first_run = true;
      vector<vector<string>> list_messages;

      while(1){
        ifs.open(path);
        if (!ifs.is_open()){
          continue;
        }

        ifs.seekg(0, ios::end);
        new_file_size = ifs.tellg();
        if (new_file_size > past_file_size){
          ifs.clear();
          ifs.seekg (0, ios::beg);

          string timeline_msg;
          while (getline(ifs, timeline_msg)){
            timeline_msg += " *break* ";
            int pos;
            vector<string> user_msg;
            while((pos = timeline_msg.find(" *break* ")) != string::npos){
              user_msg.push_back(timeline_msg.substr(0, pos));
              timeline_msg.erase(0, pos+9);
            }
            list_messages.push_back(user_msg);
            
            ++counter;
            if (counter == 20){
              break;
            }
          }

          int start;
          int finish;

          if (first_run){
            start = 0;
            finish = list_messages.size();
          }
          else{
            start = list_messages.size()-1;
            finish = list_messages.size();
          }

          for (int i = start; i < finish; ++i){

            send_msg.set_username(list_messages[i][0]);
            send_msg.set_msg(list_messages[i][1]);
            struct tm tm;
            strptime((list_messages[i][2]).c_str(), "%A %B %d %H:%M:%S %Y", &tm);
            time_t get_time = mktime(&tm);

            Timestamp time_write_stamp;
            time_write_stamp = TimeUtil::TimeTToTimestamp(get_time);
            send_msg.set_allocated_timestamp(&time_write_stamp);

            stream->Write(send_msg);
            send_msg.release_timestamp();
          }

          counter = 19;
          past_file_size = new_file_size;
          first_run = false;
        }
        ifs.close();
      }
    }, username, past_file_size);

    send_msg_thread.join();
    recv_msg_thread.join();

    return Status::OK;

  }

};

void RunServer(string port_no) {
  if (mkdir(server_id.c_str(), 0777) == -1){
    cerr << "Directory for this server exists!" << endl;
  }
  else {
    cout << "Directory " << server_id << " created!" << endl;
  }

  string server_address("localhost:" + port_no); 
  SNSServiceImpl service;
  
  ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl;
  server->Wait();
}

int connectToCoordinator(string hostname_coor, string port_coor, string type, string id, string port_server){

  auto channel = CreateChannel(hostname_coor + ":" + port_coor, InsecureChannelCredentials());

  unique_ptr<CCService::Stub> stub_coor = CCService::NewStub(channel);

  RequestServer request_server;
  request_server.set_id(id);
  request_server.set_port_num(port_server);
  request_server.set_type(type);
  ClientContext context;
  ReplyServer reply_server;

  Status status = stub_coor->addServer(&context, request_server, &reply_server);

  if (status.ok() && reply_server.msg() == "SUCCESS"){
    curr_id = reply_server.name();
    server_id = type + "_" + reply_server.name();
    return 1;
  }

  return -1;
}

int main(int argc, char** argv) {

  string host_coor = "localhost";
  string port_coor = "9090";
  string port_server = "3010";
  srand(time(0));
  string id = to_string(rand());
  string type = "master";
  int opt = 0;
  while ((opt = getopt(argc, argv, "h:c:p:i:t:")) != -1){
    switch(opt) {
      case 'h':
        host_coor = optarg;
        break;
      case 'c':
        port_coor = optarg;
        break;
      case 'p':
        port_server = optarg;
        break;
      case 'i':
        id = optarg;
        break;
      case 't':
        type = optarg;
        break;
      default:
	      cerr << "Invalid Command Line Argument\n";
    }
  }
  SNSServiceImpl service;

  int coor = connectToCoordinator(host_coor, port_coor, type, id, port_server);
  if (coor > 0) {
    RunServer(port_server);
  }
  return 0;
}
