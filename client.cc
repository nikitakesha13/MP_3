#include "common.h"
#include "client.h"

class Client : public IClient
{
    public:
        Client(const std::string& hname,
               const std::string& uname,
               const std::string& p)
            :hostname_coor(hname), username(uname), port_coor(p)
            {};
        Client(const string& host_sever, const string& port_server)
            : host_server(host_server), port_coor(port_coor){};

    protected:
        virtual int connectToCoordinator();
        virtual int connectToServer();
        virtual IReply processCommand(std::string& input);
        virtual void processTimeline();
        void send_handle_loop();
        void recv_handle_loop();
    private:
        string hostname_coor;
        string username;
        string port_coor;
        string host_server;
        string port_server;
        
        // You can have an instance of the client stub
        // as a member variable.
        unique_ptr<SNSService::Stub> stub_server;
        unique_ptr<CCService::Stub> stub_coor;

        unique_ptr<ClientReaderWriter<Message, Message>> stream_;
};

int main(int argc, char** argv) {

    string hostname_coor = "localhost";
    srand(time(0));
    string username = to_string(rand());
    string port_coor = "9090";
    int opt = 0;
    while ((opt = getopt(argc, argv, "h:p:u:")) != -1){
        switch(opt){
            case 'h':
                hostname_coor = optarg;
                break;
            case 'p':
                port_coor = optarg;
                break;
            case 'u':
                username = optarg;
                break;
            default:
                cerr << "Invalid Command Line Argument\n";
        }
    }

    Client myc(hostname_coor, username, port_coor);
    // You MUST invoke "run_client" function to start business logic
    myc.run_client();

    return 0;
}

int Client::connectToCoordinator(){
    auto channel = CreateChannel(hostname_coor + ":" + port_coor, InsecureChannelCredentials());

    stub_coor = CCService::NewStub(channel);

    RequestHost request_host;
    request_host.set_username(username);
    ClientContext context;
    ReplyHost reply_host;

    Status status = stub_coor->getServer(&context, request_host, &reply_host);

    if (status.ok()){
        host_server = reply_host.hostname();
        port_server = reply_host.port_num();
        return 1;
    }

    return -1;
}

int Client::connectToServer(){

    auto channel = CreateChannel(host_server + ":" + port_server, InsecureChannelCredentials());

    stub_server = SNSService::NewStub(channel);

    Request request;
    request.set_username(username);
    ClientContext context;
    Reply reply;

    Status status = stub_server->Login(&context, request, &reply);

    if (status.ok()){
        return 1;
    }

    return -1; // return 1 if success, otherwise return -1
}

IReply Client::processCommand(std::string& input)
{
    IReply ire;
    Status status;

    ClientContext context;
    Request request;
    Reply reply;

    request.set_username(username);

    string command;
    string arguments;

    int pos = input.find(' ');

    if (pos != string::npos){
        command = input.substr(0, pos);
        arguments = input.erase(0, pos + 1);
        arguments.erase(remove(arguments.begin(), arguments.end(), ' '), arguments.end());
        request.add_arguments(arguments);
    }
    else {
        command = input;
    }

    if (command == "LIST"){
        status = stub_server->List(&context, request, &reply);
        for (auto x: reply.all_users()){
            ire.all_users.push_back(x);
        }
        for (auto x: reply.following_users()){
            ire.following_users.push_back(x);
        }
    }
        
    
    else if (command == "FOLLOW"){
        status = stub_server->Follow(&context, request, &reply);
    }

    else if (command == "UNFOLLOW"){
        status = stub_server->UnFollow(&context, request, &reply);
    }

    else if (command == "TIMELINE"){
        ire.grpc_status = Status::OK;
        ire.comm_status = SUCCESS;
        return ire;
    }

    ire.grpc_status = status;
    if (status.ok()) {
        if (reply.msg() == "SUCCESS"){
            ire.comm_status = SUCCESS;
        }
        else if (reply.msg() == "FAILURE_ALREADY_EXISTS"){
            ire.comm_status = FAILURE_ALREADY_EXISTS;
        }
		else if (reply.msg() == "FAILURE_NOT_EXISTS"){
            ire.comm_status = FAILURE_NOT_EXISTS;
        }
		else if (reply.msg() == "FAILURE_INVALID_USERNAME"){
            ire.comm_status = FAILURE_INVALID_USERNAME;
        }
		else if (reply.msg() == "FAILURE_INVALID"){
            ire.comm_status = FAILURE_INVALID;
        }
		else if (reply.msg() == "FAILURE_UNKNOWN"){
            ire.comm_status = FAILURE_UNKNOWN;
        }
    } 
    else {
        ire.comm_status = FAILURE_UNKNOWN;
    }
    
    return ire;
}

void Client::send_handle_loop(){
    Message send_msg;

    while(1){
        string msg_str = getPostMessage();
        send_msg.set_username(username);
        send_msg.set_msg(msg_str);

        stream_->Write(send_msg);
    }
}

void Client::recv_handle_loop(){
    Message recv_msg;
    while(1){
        if (stream_->Read(&recv_msg) > 0){
            string sender = recv_msg.username();
            string msg = recv_msg.msg();
            time_t time = TimeUtil::TimestampToTimeT(recv_msg.timestamp());
            displayPostMessage(sender, msg, time);
        }
        
    }
}

void Client::processTimeline()
{
	// ------------------------------------------------------------
    // In this function, you are supposed to get into timeline mode.
    // You may need to call a service method to communicate with
    // the server. Use getPostMessage/displayPostMessage functions
    // for both getting and displaying messages in timeline mode.
    // You should use them as you did in hw1.
	// ------------------------------------------------------------

    // ------------------------------------------------------------
    // IMPORTANT NOTICE:
    //
    // Once a user enter to timeline mode , there is no way
    // to command mode. You don't have to worry about this situation,
    // and you can terminate the client program by pressing
    // CTRL-C (SIGINT)
	// ------------------------------------------------------------

    ClientContext context;
    Message msg;
    msg.set_username(username);
    stream_ = stub_server->Timeline(&context);
    stream_->Write(msg);

    thread send_msg(&Client::send_handle_loop, this);
    thread recv_msg(&Client::recv_handle_loop, this);

    send_msg.join();
    recv_msg.join();
}
