#include "common.h"

string server_id = "";
string curr_id = "";
string server_type;
unique_ptr<CCService::Stub> stub_coor;
unique_ptr<SNSService::Stub> stub_slave;
string slave_host;
string slave_port;

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

    if (server_type == "master"){
      Request slave_request;
      slave_request.set_username(username);
      slave_request.add_arguments(args);
      ClientContext slave_context;
      Reply slave_reply;

      Status status = stub_slave -> Follow(&slave_context, slave_request, &slave_reply);
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
  
  Status Login(ServerContext* context, const Request* request, Reply* reply) override {
    // ------------------------------------------------------------
    // In this function, you are to write code that handles 
    // a new user and verify if the username is available
    // or already taken
    // ------------------------------------------------------------
    string username = request->username();

    if (server_type == "master"){
      Request slave_request;
      slave_request.set_username(username);
      ClientContext slave_context;
      Reply slave_reply;

      Status status = stub_slave -> Login(&slave_context, slave_request, &slave_reply);
    }
    

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

  Status TimeLineSlave(ServerContext* context, const RequestSlave* request, ReplySlave* reply) override {
    

    string username = request -> username();
    string clean_msg = request -> msg();
    string follower = request -> follower();
    string time = request -> time();

    vector<string> file_msg;
    ifstream ifs(server_id + "/" + follower + "/" + follower + "_timeline.txt");
    if (ifs.is_open()){
      string line_msg;
      while(getline(ifs, line_msg)){
        file_msg.push_back(line_msg);
      }
    }
    ifs.close();

    ofstream ofs(server_id + "/" + follower + "/" + follower + "_timeline.txt");
    ofs << username << " *break* " << clean_msg << " *break* " << time;
    for (auto k: file_msg){
      ofs << '\n' << k;
    }
    
    ofs.close();

    reply -> set_msg("SUCCESS");
    return Status::OK;
  }
  
  Status Timeline(ServerContext* context, ServerReaderWriter<Message, Message>* stream) override {
    // ------------------------------------------------------------
    // In this function, you are to write code that handles 
    // receiving a message/post from a user, recording it in a file
    // and then making it available on his/her follower's streams
    // ------------------------------------------------------------
    // ifstream ifs;
    cout << "Timeline is called" << endl;

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
        ifs.open(server_id + "/" + username + "/" + username + "_followers.csv");
        vector<string> list_current_followers;
        vector<string> list_other_followers;
        string follower;
        int i = 0;
        while(getline(ifs, follower, ',')){
          if (((stoi(follower) % 3) + 1) == stoi(curr_id)){
            list_current_followers.push_back(follower);
          }
          else {
            list_other_followers.push_back(follower);
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

        for (auto x: list_current_followers){

          vector<string> file_msg;
          ifs.open(server_id + "/" + x + "/" + x + "_timeline.txt");
          if (ifs.is_open()){
            string line_msg;
            while(getline(ifs, line_msg)){
              file_msg.push_back(line_msg);
            }
          }
          ifs.close();

          if (server_type == "master"){
            RequestSlave slave_request;
            ClientContext slave_context;
            slave_request.set_username(username);
            slave_request.set_msg(clean_msg);
            slave_request.set_follower(x);
            slave_request.set_time(current_time);
            ReplySlave slave_reply;

            stub_slave -> TimeLineSlave(&slave_context, slave_request, &slave_reply);
          }
          
          ofstream ofs(server_id + "/" + x + "/" + x + "_timeline.txt");
          ofs << username << " *break* " << clean_msg << " *break* " << current_time;
          for (auto k: file_msg){
            ofs << '\n' << k;
          }
          
          ofs.close();
        }

        for (auto x: list_other_followers){

          string dir = server_id + "/other_followers";

          string message = username + " *break* " + clean_msg + " *break* " + current_time;

          ofstream ofs(dir + "/" + x + ".txt");
          ofs << message << '\n';
        }
      }
    });

    thread send_msg_thread([stream](string username, int past_file_size) {
      Message send_msg;
      int new_file_size;
      string path = server_id + "/" + username + "/" + username + "_timeline.txt";
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
            if (!first_run){
              if (list_messages[i][0] == username){
                continue;
              }
            }
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

  string dir = server_id + "/other_followers";

  if (mkdir(dir.c_str(), 0777) == -1){
    cerr << "Directory for this server exists!" << endl;
  }
  else {
    cout << "Directory " << dir << " already exists!" << endl;
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

  stub_coor = CCService::NewStub(channel);

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

void GetSlave(string type){
  if (type == "master"){
    while(true){
      RequestHost request;
      request.set_username(curr_id);
      request.set_type("master");
      ClientContext context;
      ReplyHost reply;

      Status status = stub_coor -> getServer(&context, request, &reply);

      if (status.ok() && reply.msg() == "SUCCESS"){
        slave_host = reply.hostname();
        slave_port = reply.port_num();
        auto channel = CreateChannel(slave_host + ":" + slave_port, InsecureChannelCredentials());
        stub_slave = SNSService::NewStub(channel);
        break;
      }
    }
  }
}

void Heartbeat(string type){
  while(true){
    RequestHeartBeat request;
    request.set_id(curr_id);
    request.set_type(type);
    ClientContext context;
    ReplyHeartBeat reply;

    Status status = stub_coor->HeartBeat(&context, request, &reply);

    if (!status.ok() || reply.msg() != "SUCCESS"){
      cout << "Server Down! Coordinator decided so!\n";
      exit(0);
    }

    this_thread::sleep_for(chrono::milliseconds(9000));
  }
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
  server_type = type;
  SNSServiceImpl service;

  int coor = connectToCoordinator(host_coor, port_coor, type, id, port_server);
  if (coor > 0) {

    thread th1(RunServer, port_server);
    thread th2(GetSlave, type);
    thread th3(Heartbeat, type);

    th1.join();
    th2.join();
    th3.join();
  }
  return 0;
}
