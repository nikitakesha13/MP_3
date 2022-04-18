#include "common.h"
#include "synch.h"

string synch_id = "";
string curr_id = "";
vector<string> hostnames;
vector<string> port_nums;
vector<string> ids;
vector<string> list_users;
vector<string> current_dir_users;
unique_ptr<CCService::Stub> stub_coor;
vector<shared_ptr<Channel>> synch_chans;
vector< pair<string, unique_ptr<SYNCService::Stub>> > synch_stubs;
mutex mtx;
vector<string> type_dir{"master_", "slave_"};

class Synchronizer final : public SYNCService::Service {

    Status List(ServerContext* context, const RequestOtherSynch* request, ReplyOtherSynch* reply) override {

        if (list_users.size() > 0){
            reply -> set_msg("SUCCESS");
            for (auto user: list_users){
                reply -> add_list_users(user);
            }
        }
        else {
            reply -> set_msg("EMPTY");
        }

        return Status::OK;
    }

    Status Follow(ServerContext* context, const RequestFollow* request, ReplyFollow* reply) override {
        string username = request -> username();
        string user_to_follow = request -> user_to_follow();

        for (string dir: type_dir){
            ofstream ofs(dir + synch_id + "/" + user_to_follow + "/" + user_to_follow + "_followers.csv", ios_base::app);
            ofs << username << ',';
            ofs.close();
        }

        reply -> set_msg("SUCCESS");
        return Status::OK;
    }
    
};

int connectToCoordinator(string hostname_coor, string port_coor, string port_server){

    string type = "synch";

    auto channel = CreateChannel(hostname_coor + ":" + port_coor, InsecureChannelCredentials());

    stub_coor = CCService::NewStub(channel);

    RequestServer request_server;
    request_server.set_id(curr_id);
    request_server.set_port_num(port_server);
    request_server.set_type(type);
    ClientContext context;
    ReplyServer reply_server;

    Status status = stub_coor->addServer(&context, request_server, &reply_server);

    if (status.ok() && reply_server.msg() == "SUCCESS"){
        synch_id = reply_server.name();
        return 1;
    }

    return -1;
}

void HandleRequest(){

    ifstream ifs;
    ofstream ofs;

    while(true){
        for (string user: current_dir_users){
            for (string dir: type_dir){
                ifs.open(dir + synch_id + "/" + user + "/" + user + "_requests.csv");
                if (!ifs.is_open()){
                    continue;
                }
                
                vector<string> req_users;
                string request;

                while(getline(ifs, request, ',')){
                    req_users.push_back(request);
                }

                ifs.close();

                while (req_users.size() > 0){

                    int user_loc = stoi(req_users[0]) % 3;
                    int index;
                    if (synch_stubs[0].first == to_string(user_loc)){
                        index = 0;
                    }
                    else {
                        index = 1;
                    }

                    RequestFollow request;
                    request.set_username(user);
                    request.set_user_to_follow(req_users[0]);

                    ClientContext context;
                    ReplyFollow reply;

                    Status status = synch_stubs[index].second -> Follow(&context, request, &reply);

                    if (status.ok() && reply.msg() == "SUCCESS"){

                        req_users.erase(req_users.begin());

                        ofs.open(dir + synch_id + "/" + user + "/" + user + "_following.csv", ios_base::app);
                        ofs << req_users[0] << ',';
                        ofs.close();

                    }
                }

                remove((dir + synch_id + "/" + user + "/" + user + "_requests.csv").c_str());
            }
        }
        this_thread::sleep_for(chrono::milliseconds(3000));
    }
}

void WriteToList(){
    int prev_size = 0;

    while(true){
        if (list_users.size() > prev_size){
            ofstream ofs("list_" + synch_id + ".csv", ios_base::app);
            for (int i = prev_size; i < list_users.size(); ++i){
                ofs << list_users[i] << ",";
            }
            ofs.close();
            prev_size = list_users.size();
        }
    }
}

void ListOther(){
    while(true){
        for (int i = 0; i < synch_stubs.size(); ++i){
            RequestOtherSynch request;
            request.set_msg("LIST");
            ClientContext context;
            ReplyOtherSynch reply;

            Status status = synch_stubs[i].second -> List(&context, request, &reply);

            if (status.ok() && reply.msg() == "SUCCESS"){
                for (auto user: reply.list_users()){
                    if (find(list_users.begin(), list_users.end(), user) == list_users.end()){
                        list_users.push_back(user);
                    }
                }
            }

        }
        this_thread::sleep_for(chrono::milliseconds(3000));
    }
}

void List(){

    while(true) {

        DIR *dr;
        struct dirent *en;
        for (string p: type_dir){
            string path = p + synch_id;
            dr = opendir(path.c_str());
            if (dr) {
                while((en = readdir(dr)) != NULL){
                    string temp = en->d_name;
                    if (find(list_users.begin(), list_users.end(), temp) == list_users.end()){
                        list_users.push_back(temp);
                        current_dir_users.push_back(temp);
                    }
                }
                closedir(dr);
            }
        }
        this_thread::sleep_for(chrono::milliseconds(3000));
    }
}

void RunSynch(string port_num){

    string server_address("localhost:" + port_num); 
    Synchronizer service;
    
    ServerBuilder builder;
    builder.AddListeningPort(server_address, InsecureServerCredentials());
    builder.RegisterService(&service);
    unique_ptr<Server> server(builder.BuildAndStart());
    cout << "Synchronizer listening on " << server_address << endl;
    server->Wait();
}

void GetOtherInfo(){

    while(true){
        RequestSynchData request_synch;
        request_synch.set_id(synch_id);
        ClientContext context;
        ReplySynchData reply_synch;

        Status status = stub_coor -> getFollowerSynch(&context, request_synch, &reply_synch);

        if (status.ok()){
            for (int i = 0; i < reply_synch.id().size(); ++i){
                bool exists = false;
                for (auto x: ids){
                    if (reply_synch.id()[i] == x){
                        exists = true;
                    }
                }
                if (!exists){
                    ids.push_back(reply_synch.id()[i]);
                    hostnames.push_back(reply_synch.hostname()[i]);
                    port_nums.push_back(reply_synch.port_num()[i]);
                    string chan_name = hostnames[hostnames.size()-1] + ":" + port_nums[port_nums.size()-1];
                    synch_chans.push_back(CreateChannel(chan_name, InsecureChannelCredentials()));
                    synch_stubs.push_back(make_pair(reply_synch.id()[i], SYNCService::NewStub(synch_chans[synch_chans.size()-1])));
                    cout << "ID: " << ids[ids.size()-1] << ", Hostname: " << hostnames[hostnames.size()-1] << ", Port:" << port_nums[port_nums.size()-1] << endl;
                }
            }      
        }

        if (hostnames.size() == 2){
            break;
        }

        this_thread::sleep_for(chrono::milliseconds(3000));
    }
    

}

int main(int argc, char** argv){

    string host_coor = "localhost";
    string port_coor = "9090";
    string port_synch = "9000";
    srand(time(0));
    curr_id = to_string(rand());
    int opt = 0;
    while ((opt = getopt(argc, argv, "h:c:p:i:")) != -1){
        switch(opt) {
            case 'h':
                host_coor = optarg;
                break;
            case 'c':
                port_coor = optarg;
                break;
            case 'p':
                port_synch = optarg;
                break;
            case 'i':
                curr_id = optarg;
                break;
        }
    }
    int coor = connectToCoordinator(host_coor, port_coor, port_synch);
    if (coor > 0){
        thread th1(RunSynch, port_synch);
        thread th2(GetOtherInfo);
        thread th3(List);
        thread th4(ListOther);
        thread th5(WriteToList);
        thread th6(HandleRequest);

        th1.join();
        th2.join();
        th3.join();
        th4.join();
        th5.join();
        th6.join();
    }
    return 0;
}