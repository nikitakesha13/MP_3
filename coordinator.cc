#include "common.h"

struct ServerDB {
    string id;
    string hostname;
    string port_num;
    time_t time;
    int heart_miss;
};

vector<ServerDB> servers_master;
vector<ServerDB> servers_slave;
vector<ServerDB> servers_synch;

int search_db(vector<ServerDB>& current, struct ServerDB ser){
    bool exists = false;
    ser.id = to_string(current.size()+1);
        
    for (auto x: current){
        if (x.id == ser.id){
            exists = true;
            break;
        }
    }
    if (exists || current.size() == 3){
        return -1;
    }
    current.push_back(ser);
    return 1;
}

void VerifyHeartBeat(){
    vector<time_t> prev_master_time{0, 0, 0};
    while(true){
        for (int i = 0; i < servers_master.size(); ++i){
            if (servers_master[i].time == prev_master_time[i]){
                servers_master[i].heart_miss += 1;
                if (servers_master[i].heart_miss == 2){
                    servers_master[i].id = "DOWN";
                }
            }
            else {
                servers_master[i].heart_miss = 0;
            }
            prev_master_time[i] = servers_master[i].time;
        }

        this_thread::sleep_for(chrono::milliseconds(9000));

    }
}

class CoordinatorImpl final : public CCService::Service {
    Status getServer(ServerContext* context, const RequestHost* request, ReplyHost* reply) override {
        
        string username = request -> username();
        string type = request -> type();

        string hostname = "";
        string port_num = "";

        if (type == "user"){
            int index = (stoi(username) % 3);

            if (index < servers_master.size()){
                if (servers_master[index].id != "DOWN"){
                    hostname = servers_master[index].hostname;
                    port_num = servers_master[index].port_num;
                }
                else {
                    hostname = servers_slave[index].hostname;
                    port_num = servers_slave[index].port_num;
                }
                reply->set_hostname(hostname);
                reply->set_port_num(port_num);
                reply->set_msg("SUCCESS");
            }
            else {
                reply->set_msg("FAILURE_NOT_EXISTS");
            }
        }

        else { // master requesting slave server

            int index = stoi(username) - 1;
            reply -> set_msg("FAILURE_NOT_EXISTS");
            
            if (servers_slave.size() > index){
                hostname = servers_slave[index].hostname;
                port_num = servers_slave[index].port_num;
                reply -> set_msg("SUCCESS");
            }
            

            reply -> set_hostname(hostname);
            reply -> set_port_num(port_num);
        }
        

        return Status::OK;
    }

    Status addServer(ServerContext* context, const RequestServer* request, ReplyServer* reply) override {

        string id = request -> id();
        string port_server = request -> port_num();
        string type = request -> type();
        struct ServerDB ser = {id, "localhost", port_server};
        int server_num = 0;

        int ret = 0;
        if (type == "master"){
            ser.time = time(0);
            ser.heart_miss = 0;
            ret = search_db(servers_master, ser);
            server_num = servers_master.size();
        }
        else if (type == "slave"){
            ret = search_db(servers_slave, ser);
            server_num = servers_slave.size();
        }
        else if (type == "synch"){
            ret = search_db(servers_synch, ser);
            server_num = servers_synch.size();
        }
        else {
            reply->set_msg("FAILURE_INVALID");
        }

        if (ret == 1){
            reply->set_msg("SUCCESS");
            reply->set_name(to_string(server_num));
        }
        else if (ret == -1) {
            reply->set_msg("FAILURE_ALREADY_EXISTS");
        }

        return Status::OK;
    }
    
    Status getFollowerSynch(ServerContext* context, const RequestSynchData* request, ReplySynchData* reply) override {

        string id = to_string(stoi(request -> id()));

        bool exists = false;
        if (servers_synch.size() > 1){
            for (int i = 0; i < servers_synch.size(); ++i){
                if (id != servers_synch[i].id){
                    exists = true;
                    reply -> add_id(servers_synch[i].id);
                    reply -> add_hostname(servers_synch[i].hostname);
                    reply -> add_port_num(servers_synch[i].port_num);
                }
            }
            if (exists){
                reply -> set_msg("SUCCESS");            
            }
            else {
                reply -> set_msg("FAILURE_NOT_EXISTS");
            }
        }
        else {
            reply -> set_msg("EMPTY");
        }
        
        return Status::OK;
    }

    Status HeartBeat(ServerContext* context, const RequestHeartBeat* request, ReplyHeartBeat* reply) override {
        
        reply -> set_msg("SUCCESS");
        string type = request -> type();

        if (type == "master"){
            int index = stoi(request -> id()) - 1;
            time_t curr_time = time(0);
            servers_master[index].time = curr_time;
        }

        return Status::OK;

    }

};

void RunCoordinator(string host, string port){
    string server_address(host + ":" + port); 
    CoordinatorImpl service;
    
    ServerBuilder builder;
    builder.AddListeningPort(server_address, InsecureServerCredentials());
    builder.RegisterService(&service);
    unique_ptr<Server> server(builder.BuildAndStart());
    cout << "Coordinator listening on " << server_address << endl;
    server->Wait();
}

int main(int argc, char** argv){

    string host = "localhost";
    string port = "9090";

    int opt = 0;
    while((opt = getopt(argc, argv, "h:p:")) != -1){
        switch(opt){
            case 'h':
                host = optarg;
                break;
            case 'p':
                port = optarg;
                break;
            default:
                cerr << "Invalid Command Line Argument\n";
        }
    }

    thread th1(RunCoordinator, host, port);
    thread th2(VerifyHeartBeat);

    th1.join();
    th2.join();
    return 0;
}