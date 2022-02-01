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

#include <stdlib.h>
#include <stdio.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <memory>
#include <string>
#include <fstream>
#include <filesystem>
#include <boost/algorithm/string.hpp>
#include "user_data.h"
#include "backend_utils.h"


#include <grpc/grpc.h>
#include <grpcpp/security/server_credentials.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/server_context.h>
 //#ifdef BAZEL_BUILD
 //#include "examples/protos/route_guide.grpc.pb.h"
 //#else
#include "kvstore.grpc.pb.h"
//#endif

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ServerWriter;
using grpc::Status;

using backend::KValueStore;
using backend::Coordination;


using backend::SeqRequest;
using backend::SeqResponse;
using backend::PutRequest;
using backend::PutResponse;
using backend::GetRequest;
using backend::GetResponse;
using backend::CPutRequest;
using backend::CPutResponse;
using backend::DeleteRequest;
using backend::DeleteResponse;
using backend::AdminDataReq;
using backend::AdminDataRsp;
using backend::AdminSwitchReq;
using backend::AdminSwitchRsp;


using backend::AdminEchoReq;
using backend::AdminEchoRsp;
using backend::InitUserReq;
using backend::InitUserRsp;

using std::chrono::system_clock;

#define MAX_LOG_NUM 10

string logfile;
string cpfile;
bool isActive = true;

//using namespace std;
map<string, UserData*> storage;
int log_counter = 0;
string my_serv("0.0.0.0:50051");

// write to cp, clear log, clear counter 
void save_to_cp(string cpfile, string logfile) {
  log_counter++;
  cout << "log_counter: " << log_counter << endl;

  if (log_counter >= MAX_LOG_NUM) {
    ofstream ofs;
    ofs.open(cpfile, ofstream::out | ofstream::trunc);
    ofs.close();
    for (auto& [user, info] : storage) {
      info->write_checkpoint(cpfile);
      cout << "writting user " << user << endl;
      }
    ofs.open(logfile, ofstream::out | ofstream::trunc);
    ofs.close();
    log_counter = 0;
    }
  }


class KValueStoreImpl final : public KValueStore::Service {
public:
  explicit KValueStoreImpl() {
    }

  Status GetSeqNum(ServerContext* context, const SeqRequest* sReq,
    SeqResponse* sRsp) override {
    // Server is sleep

    if (!isActive) return Status::CANCELLED;

    string row = sReq->row();
    string value;
    string status;
    if (storage.find(row) != storage.end()) {
      //valid row
      value = to_string(storage[row]->version_num[sReq->col()]);
      status = "+OK got it";
      }
    else { // row key not exist.
      value = "0";
      status = "+OK row key not exist.\r\n";
      }
    sRsp->set_status(status);
    sRsp->set_version(value);
    // save_to_cp(cpfile, logfile);
    return Status::OK;
    }



  //  Status GetValues(ServerContext* context, const GetRequest* getReq,
  //    GetResponse* getRsp) override {
  //    // Server is sleep
  //
  //    if (!isActive) return Status::CANCELLED;
  //
  //    string row = getReq->row();
  //    string value;
  //    string status;
  //    if (storage.find(row) != storage.end()) {
  //      //valid row
  //
  //      storage[row]->get(getReq->col(), value, status);
  //
  //      }
  //    else { // invalid
  //      value.clear();
  //      status = "-ERR. Invalid rowKey.\r\n";
  //      }
  //    getRsp->set_status(status);
  //    getRsp->set_value(value);
  //    // save_to_cp(cpfile, logfile);
  //    return Status::OK;
  //    }
  Status GetValues(ServerContext* context, const GetRequest* getReq,
    ServerWriter<GetResponse>* writer) override
    {
    // Server is sleep
    if (!isActive) return Status::CANCELLED;

    string row = getReq->row();
    string value;
    string status;
    vector<GetResponse> get_vec;
    if (storage.find(row) != storage.end())
      {
      //valid row
      storage[row]->get(getReq->col(), value, status);
      int get_buf_size = 32768;
      int start_position = 0;
      int v_len = value.size();
      while ((start_position + get_buf_size) < v_len)
        {
        GetResponse* getRsp = new GetResponse;
        getRsp->set_status(status);
        getRsp->set_value(value.substr(start_position, get_buf_size));
        get_vec.push_back(*getRsp);
        start_position += get_buf_size;
        }
      GetResponse* getRsp = new GetResponse;
      getRsp->set_status(status);
      getRsp->set_value(value.substr(start_position));
      get_vec.push_back(*getRsp);
      }
    else
      {
      // Row does not exits
      GetResponse* getRsp = new GetResponse;
      getRsp->set_status("-ERR. Invalid rowKey.\r\n");
      getRsp->set_value("");
      get_vec.push_back(*getRsp);
      }
    for (const GetResponse& GRe : get_vec)
      {
      if (!writer->Write(GRe))
        {
        // Broken stream.
        cout << "Here at Broken Get Stream" << endl;
        break;
        }
      }
    return Status::OK;
    }

  //  Status PutValues(ServerContext* context, const PutRequest* preq,
  //    PutResponse* prsp) override
  //    {
  //
  //    if (!isActive) return Status::CANCELLED;
  //    string row = preq->row();
  //    string col = preq->col();
  //    string value = preq->value();
  //    string v_number = preq->version();
  //    string status;
  //    string primary = "";
  //    vector<string> replica_vec;
  //    replica_vec.push_back("lol");
  //    if (storage.find(row) == storage.end()) {
  //      cout << "inside putvalue and logfile is " << logfile << endl;
  //      storage.insert(make_pair(row, new UserData(row, logfile)));
  //      }
  //    storage[row]->put(preq->col(), v_number, value, status);
  //
  //    prsp->set_status(status);
  //    save_to_cp(cpfile, logfile);
  //    return Status::OK;
  //    }
  Status PutValues(ServerContext* context, ServerReader<PutRequest>* PRe,
    PutResponse* prsp) override
    {
    if (!isActive) return Status::CANCELLED;
    PutRequest preq;
    bool first_time = true;
    string row;
    string col;
    string value;
    string v_number;
    while (PRe->Read(&preq))
      {
      if (first_time == true)
        {
        first_time = false;
        row = preq.row();
        col = preq.col();
        value = preq.value();
        v_number = preq.version();
        }
      else
        {
        value += preq.value();
        }
      }
    string status;
    if (storage.find(row) == storage.end()) {
      //		cout << "inside putvalue and logfile is " << logfile << endl;
      storage.insert(make_pair(row, new UserData(row, logfile)));
      }
    // cout << "Row: " << row << ", Col: " << col << ", Value: " << value << ", version: " << v_number << endl;
    storage[row]->put(col, v_number, value, status);

    prsp->set_status(status);
    save_to_cp(cpfile, logfile);
    return Status::OK;
    }
  //  Status CPutValues(ServerContext* context, const CPutRequest* cpreq,
  //    CPutResponse* cprsp) override
  //    {
  //
  //    if (!isActive) return Status::CANCELLED;
  //    string status;
  //    string row = cpreq->row();
  //    string col = cpreq->col();
  //    string val1 = cpreq->value1();
  //    string val2 = cpreq->value2();
  //    string v_number = cpreq->version();
  //    if (storage.find(row) == storage.end()) {
  //      // Invalid row, col pair
  //      status = "+OK There is no entry at [" + row + "][" + col + "].\r\n";
  //      }
  //    else {
  //      // Valid request
  //      status = "+OK Value updated.\r\n";
  //      storage[row]->cput(col, v_number, val1, val2, status);
  //      }
  //    cprsp->set_status(status);
  //    save_to_cp(cpfile, logfile);
  //    return Status::OK;
  //    }

  Status CPutValues(ServerContext* context, const CPutRequest* cpreq,
    CPutResponse* cprsp) override
    {

    if (!isActive) return Status::CANCELLED;
    string status;
    string row = cpreq->row();
    string col = cpreq->col();
    string val1 = cpreq->value1();
    string val2 = cpreq->value2();
    string v_number = cpreq->version();
    if (storage.find(row) == storage.end()) {
      // Invalid row, col pair
      status = "+OK There is no entry at [" + row + "][" + col + "].\r\n";
      }
    else {
      // Valid request
      status = "+OK Value updated.\r\n";
      storage[row]->cput(col, v_number, val1, val2, status);
      }
    cprsp->set_status(status);
    save_to_cp(cpfile, logfile);
    return Status::OK;
    }


  Status DeleteValues(ServerContext* context, const DeleteRequest* dReq,
    DeleteResponse* dRsp) override {

    if (!isActive) return Status::CANCELLED;

    string status;
    string row = dReq->row();
    string col = dReq->col();
    string v_number = dReq->version();
    if (storage.find(row) == storage.end()) {
      // Invalid rowkey
      status = "-ERR. Invalid rowKey.\r\n";
      }
    else {
      storage[row]->del(col, v_number, status);
      }
    dRsp->set_status(status);

    save_to_cp(cpfile, logfile);
    return Status::OK;

    }

  Status GetDataId(ServerContext* context, const AdminDataReq* aReq,
		  ServerWriter<AdminDataRsp>* writer) override {

    if (!isActive) return Status::CANCELLED;
    int id = stoi(aReq->id());
    int counter;
    vector<AdminDataRsp> get_vec;
    for (auto& [user, info] : storage)
    {
      if (id >= info->numOfEntries) {
        id -= info->numOfEntries;
        }
      else
      {
        counter = 0;
        for (auto& [col, val] : info->data)
        {
          if (id == counter)
          {
        	  int get_buf_size = 32768;
        	  int start_position = 0;
        	  int v_len = val.size();
        	  while ((start_position + get_buf_size) < v_len)
        	  {
        		  AdminDataRsp * aRsp = new AdminDataRsp;
        		  aRsp->set_status(string("+OK"));
        		  aRsp->set_row(user);
        		  aRsp->set_col(col);
        		  aRsp->set_value(val.substr(start_position, get_buf_size));
        		  get_vec.push_back(*aRsp);
        		  start_position += get_buf_size;
        		  // return Status::OK;
        	  }
        	  AdminDataRsp * aRsp = new AdminDataRsp;
			  aRsp->set_status(string("+OK"));
			  aRsp->set_row(user);
			  aRsp->set_col(col);
			  aRsp->set_value(val.substr(start_position));
			  get_vec.push_back(*aRsp);
			  for (const AdminDataRsp& ARs : get_vec)
			  {
				  if (!writer->Write(ARs))
				  {
					  // Broken stream.
					  cout << "Here at Broken GetDataID Stream" << endl;
					  break;
				  }
			  }
			  return Status::OK;
          }
          counter++;
        }
      }
    }
    AdminDataRsp * aRsp = new AdminDataRsp;
    aRsp->set_status(string("-ERR data not found"));
    aRsp->set_row("");
    aRsp->set_col("");
    aRsp->set_value("");
    get_vec.push_back(*aRsp);
    for (const AdminDataRsp& ARs : get_vec)
    {
    	if (!writer->Write(ARs))
    	{
    		// Broken stream.
    		cout << "Here at Broken GetDataID Stream" << endl;
    		break;
    	}
    }
    return Status::OK;
    }

  Status SwitchServer(ServerContext* context, const AdminSwitchReq* sreq,
    AdminSwitchRsp* srsp) override
    {
    isActive = !isActive;
    srsp->set_status("+OK. Switched");
    return Status::OK;
    }


private:
  std::mutex mu_;
  };

class AdminStoreImpl final : public Coordination::Service {
public:
  explicit AdminStoreImpl() {
    }

  Status GetEcho(ServerContext* context, const AdminEchoReq* echoReq,
    AdminEchoRsp* echoRsp) override {
    if (!isActive) return Status::CANCELLED;
    string status = "active";
    string value = my_serv;
    echoRsp->set_status(status);
    echoRsp->set_serverid(value);
    return Status::OK;
    }
  /***
    Status InitUser(ServerContext* context, const InitUserReq* initReq,
      InitUserRsp* initRsp) override {

      if (!isActive) return Status::CANCELLED;
      // init user
      string row = initReq->rowkey();
      string primary = initReqf->primary();
      string replica = initReq->replica();
      vector<string> replica_vec;
      boost::split(replica_vec, replica, boost::is_any_of(","));

      if (storage.find(row) == storage.end()) {
        cout << "inside putvalue and logfile is " << logfile << endl;
        storage.insert(make_pair(row, new UserData(row, logfile, primary, replica_vec)));

        }

      string status = "+OK";
      initRsp->set_status(status);
      return Status::OK;
      }

  ***/

private:


  std::mutex mu_;
  };

void RunServer(string serv_ip) {
  string server_address(serv_ip);
  KValueStoreImpl kv_service;
  AdminStoreImpl admin_service;

  ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());

  builder.RegisterService(&kv_service);
  builder.RegisterService(&admin_service);

  std::unique_ptr < Server > server(builder.BuildAndStart());

  std::cout << "Server listening on " << server_address << std::endl;
  server->Wait();
  }


void init(int s_id) {
  // filesystem::current_path(filesystem::temp_directory_path());
  filesystem::create_directory("log");
  filesystem::create_directory("cp");
  cout << "here" << endl;
  logfile = "./log/logfile_" + to_string(s_id);
  cpfile = "./cp/cpfile_" + to_string(s_id);

  update_kvstore_with_cp(cpfile, logfile, storage);
  update_kvstore_with_log(logfile, storage);
  log_counter = MAX_LOG_NUM; // to trigger condition inside save_to_cp
  save_to_cp(cpfile, logfile);

  for (auto& [user, info] : storage) {
    cout << "***********row key is " << user << "***********" << endl;
    for (auto& [col, val] : info->data)
      cout << col << "," << val << endl;
    }

  }
int main(int argc, char** argv) {
  if (argc == 1) {
    fprintf(stderr, "*** Author: Team 11 (CIS 505)\n");
    exit(1);
    }
  if (argc < 3) {
    fprintf(stderr, "Not enough arguments");
    exit(1);
    }

  // cout
  //   << "**********************this is the testcase for hash function**********************"
  //   << endl;
  // int numOfServers =4;
  // vector<int> node_set;
  // vector < string > keys = { "linh_file", "linh_mail", "lanting_file",
  //     "yinhong_file", "varun_file", "yike_file" };
  // for (auto key : keys) {
  //   hash_func(numOfServers, node_set, key);
  //   cout << "the node for " << key << " is: ";
  //   for (auto i : node_set) {
  //     cout << i << ",";
  //     }
  //   cout << endl;
  //   }
  // cout << "**********************end of test**********************" << endl;

  int s_id;
  std::fstream config_file;
  s_id = atoi(argv[2]);
  config_file.open(argv[1], std::ios::in);
  if (config_file.is_open()) {
    string line;
    // string config; // the 1st / 2nd part of the config, (i.e. either forwarding address or bind address)
    int count = 0;
    while (getline(config_file, line)) {
      if (count == s_id) {
        my_serv = line;
        break;
        }
      count++;
      }
    }
  init(s_id);

  // Assuming that input is in format of ./kery_valua_store_server filename id

  RunServer(my_serv);

  return 0;
  }
