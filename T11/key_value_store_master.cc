/*
 *
 * Copyright 2015 gRPC authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include <random>
#include <thread>
#include <stdlib.h>
#include <stdio.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <iterator>
#include <sstream>
#include <memory>
#include <string>
#include <fstream>
#include <vector>
#include "user_data.h"
#include "backend_utils.h"

#include <grpc/grpc.h>
#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>
#include <grpcpp/security/server_credentials.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/server_context.h>
#include "kvstore.grpc.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReader;
using grpc::ClientReaderWriter;
using grpc::ClientWriter;
using grpc::Status;

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ServerWriter;

using backend::Coordination;

using backend::AdminSeverReq;
using backend::AdminSeverRsp;
using backend::AdminEchoReq;
using backend::AdminEchoRsp;
using backend::BkendReq;
using backend::BkendRsp;
using backend::InitUserReq;
using backend::InitUserRsp;

using std::chrono::system_clock;

#define MAX_CONNECTIONS 100
vector<string> backservlist;
int numOfServers;


class AdminMaster {
public:
  AdminMaster(std::shared_ptr<Channel> channel) :
    stub_(Coordination::NewStub(channel)) {
    std::cout << "Ready to communicate with backend server!" << std::endl;
    }

  string GetEcho() {
    AdminEchoReq echoReq;
    echoReq.set_msg(string("echo"));
    AdminEchoRsp echoRsp;
    bool a = getEchoOneValue(echoReq, &echoRsp);
    if (a == true) {
      return echoRsp.serverid();
      }
    else {
      return string("");
      }
    }
  /***
string InitUser(string user, string primary, string serverlist) {
  InitUserReq initReq;
  initReq.set_rowkey(user);
  initReq.set_primary(primary);
  initReq.set_replica(serverlist); // ip:port,ip:port...

  InitUserRsp initRsp;
  if (initOneValue(initReq, &initRsp)) return initRsp.status();
  return string("");
  }
  ***/
private:
  bool getEchoOneValue(const AdminEchoReq& echoReq, AdminEchoRsp* echoRsp) {
    ClientContext context;

    Status status = stub_->GetEcho(&context, echoReq, echoRsp);
    if (!status.ok()) {
      cout << "GetEcho rpc failed." << endl;
      return false;
      }
    cout << "Got active server: " << echoRsp->serverid() << endl;

    return true;
    }
  /***
  bool initOneValue(const InitUserReq& initReq, InitUserRsp* initRsp) {
    ClientContext context;

    Status status = stub_->InitUser(&context, initReq, initRsp);
    if (!status.ok()) {
      cout << "InitUser rpc failed." << endl;
      return false;
      }
    cout << "Got InitUser status: " << initRsp->status() << endl;

    return true;
    }
    ***/
  std::unique_ptr<Coordination::Stub> stub_;
  };

vector<AdminMaster*> connections;


class CoordinationImpl final : public Coordination::Service {
public:
  explicit CoordinationImpl() {
    }

  Status GetActiveServers(ServerContext* context, const AdminSeverReq* ASReq, AdminSeverRsp* ASRsp) override {
    bool first_time = true;
    bool second_time = true;
    string serv_output("");
    string all_serv_output("");
    // TODO: set up global variables later
    // for (auto i = backservlist.begin(); i != backservlist.end(); ++i) {
    int id = 0;
    for (auto& masterNode : connections) {
      string output = masterNode->GetEcho();
      if (output.compare(string("")) != 0)
        {
        if (first_time == true) {
          first_time = false;
          serv_output += output;
          }
        else {
          serv_output += string(",");
          serv_output += output;
          }
        }
      if (second_time == true)
        {
        all_serv_output += backservlist[id];
        second_time = false;
        }
      else
        {
        all_serv_output += string(",");
        all_serv_output += backservlist[id];
        }
      id++;
      }
    if (first_time == true)
      {
      ASRsp->set_status("-ERR found no server");
      ASRsp->set_serverlist("");
      ASRsp->set_allserverlist(all_serv_output);
      }
    else
      {
      ASRsp->set_status("+OK found servers");
      ASRsp->set_serverlist(serv_output);
      ASRsp->set_allserverlist(all_serv_output);
      }
    return Status::OK;
    }
  Status GetBackend(ServerContext* context, const BkendReq* bReq, BkendRsp* bRsp) override {
    vector<string> servergroup;
    string rowkey = bReq->rowkey();
    vector<int> replica_group;
    hash_func(numOfServers, replica_group, rowkey);
    // set the first node as primary 
    for (auto& id : replica_group) {
      servergroup.push_back(backservlist[id]);
      }

    stringstream ss;
    copy(servergroup.begin(), servergroup.end(), ostream_iterator<string>(ss, ","));
    string replicas = ss.str();
    replicas = replicas.substr(0, replicas.length() - 1);  // get rid of the trailing space

    // load balancing 
    bRsp->set_status("+OK");
    bRsp->set_replicas(replicas);
    return Status::OK;
    }
  };




void RunServer(string serv_ip) {
  string server_address(serv_ip);
  //	KValueStoreImpl kv_service;
  CoordinationImpl admin_service;

  ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());

  //	builder.RegisterService(&kv_service);
  builder.RegisterService(&admin_service);

  std::unique_ptr < Server > server(builder.BuildAndStart());

  cout << "Server listening on " << server_address << endl;
  server->Wait();
  }

int main(int argc, char** argv) {
  //	AdminMaster masterNode(
  //			grpc::CreateChannel("localhost:50051",
  //					grpc::InsecureChannelCredentials()));
  //
  //	masterNode.GetEcho();
  string my_serv("");
  fstream config_file;
  config_file.open(argv[1], std::ios::in);
  if (config_file.is_open()) {
    string line;
    int count = 0;
    while (getline(config_file, line)) {
      if (count == 0) {
        my_serv += line;
        }
      else {
        backservlist.push_back(line);

        AdminMaster* masterNode = new AdminMaster(grpc::CreateChannel(line, grpc::InsecureChannelCredentials()));
        connections.push_back(masterNode);

        }
      count++;
      }
    }
  numOfServers = backservlist.size();
  for (auto i = backservlist.begin(); i != backservlist.end(); ++i)
    cout << *i << endl;



  RunServer(my_serv);
  return 0;
  }
