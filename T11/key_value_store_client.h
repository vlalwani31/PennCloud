#ifndef KEY_VALUE_STORE_CLIENT_H_
#define KEY_VALUE_STORE_CLIENT_H_

#include <chrono>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <thread>
#include <boost/algorithm/string.hpp>
#include "user_data.h"

#include <grpc/grpc.h>
#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>
#include "kvstore.grpc.pb.h"



using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReader;
using grpc::ClientReaderWriter;
using grpc::ClientWriter;
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

using backend::AdminSeverReq;
using backend::AdminSeverRsp;
using backend::BkendReq;
using backend::BkendRsp;

using namespace std;

class KValueStoreClient {
public:
  KValueStoreClient(shared_ptr<Channel> channel);

  int GetSeqNum(string row_key, string col_key);
  string GetValues(string row_key, string col_key);
  bool PutValues(string r, string c, string v, int seq_num);
  bool CPutValues(string r, string c, string v1, string v2, int seq_num);
  bool DeleteValues(string r, string c, int seq_num);
  bool GetDataId(string id, string& full_get);
  void SwitchServer(string msg);
private:
  int buffer_size = 32768;
  unique_ptr<KValueStore::Stub> stub_;

  int getSeqOneValue(const SeqRequest& srt, SeqResponse* srp);
  // string getOneValue(const GetRequest& grt, GetResponse* grp);
  //  bool putOneValue(const PutRequest& preq, PutResponse* prsp);
  bool cputOneValue(const CPutRequest& cpreq, CPutResponse* cprsp);
  bool deleteOneValue(const DeleteRequest& dReq, DeleteResponse* dRsp);
  // bool getDataOneValue(const AdminDataReq& aReq,
  //  AdminDataRsp* aRsp, string& full_get);
  bool switchOneValue(const AdminSwitchReq& sReq,
    AdminSwitchRsp* sRsp);
  };

class CoordinationClient {
public:
  CoordinationClient(shared_ptr<Channel> channel);

  void GetActiveServers(string& activelist, string& entirelist);


  void GetBackend(string row_key, string& server);

private:

  unique_ptr<Coordination::Stub> stub_;

  void getactiveValue(const AdminSeverReq& ASreq, AdminSeverRsp* ASrsp, string& activelist, string& entirelist);
  void getBackendValue(const BkendReq& breq, BkendRsp* brsp, string& server);
  };

#endif /* KEY_VALUE_STORE_CLIENT_H_ */
