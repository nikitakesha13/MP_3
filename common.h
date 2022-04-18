#ifndef COMMON_H_
#define COMMON_H_

#include <ctime>

#include <google/protobuf/timestamp.pb.h>
#include <google/protobuf/duration.pb.h>

#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <google/protobuf/util/time_util.h>
#include <grpc++/grpc++.h>
#include <string>
#include <dirent.h>
#include <thread>
#include <bits/stdc++.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <algorithm>
#include <mutex>
#include <chrono>
#include <utility>

#include "sns.grpc.pb.h"
#include "ccs.grpc.pb.h"

using grpc::CreateChannel;
using grpc::InsecureChannelCredentials;
using grpc::InsecureServerCredentials;
using grpc::ClientContext;
using grpc::Channel;
using google::protobuf::Timestamp;
using google::protobuf::Duration;
using google::protobuf::util::TimeUtil;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ClientReaderWriter;
using grpc::ServerWriter;
using grpc::Status;
using csce438::Message;
using csce438::Request;
using csce438::Reply;
using csce438::SNSService;
using csce438::CCService;
using csce438::RequestServer;
using csce438::ReplyServer;
using csce438::RequestHost;
using csce438::ReplyHost;
using csce438::RequestSynchData;
using csce438::ReplySynchData;

using namespace std;

#endif