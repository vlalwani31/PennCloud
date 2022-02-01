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

#include "key_value_store_client.h"

KValueStoreClient::KValueStoreClient(std::shared_ptr<Channel> channel) :
  stub_(KValueStore::NewStub(channel)) {
  cout << "KVC: new instance created" << endl;
  }

int KValueStoreClient::GetSeqNum(string row_key, string col_key) {
  SeqRequest sReq;
  sReq.set_row(row_key);
  sReq.set_col(col_key);
  SeqResponse sRsp;
  return getSeqOneValue(sReq, &sRsp);
  }


string KValueStoreClient::GetValues(string row_key, string col_key) {
  GetRequest getReq;
  getReq.set_row(row_key);
  getReq.set_col(col_key);
  GetResponse getRsp;
  ClientContext context;
  std::unique_ptr<ClientReader<GetResponse> > reader(
    stub_->GetValues(&context, getReq));
  string value = "";
  while (reader->Read(&getRsp)) {
    value += getRsp.value();
    }
  Status status = reader->Finish();
  if (status.ok()) {
    std::cout << "GetValues rpc succeeded." << std::endl;
    }
  else {
    std::cout << "GetValues rpc failed." << std::endl;
    }
  return value;
  }

bool KValueStoreClient::PutValues(string r, string c, string v, int seq_num) {
  // PutRequest PReq;
  // PReq.set_row(r);
  // PReq.set_col(c);
  // PReq.set_value(v);
  // PReq.set_version(to_string(seq_num));
  PutResponse PRsp;
  // return putOneValue(PReq, &PRsp);
  int put_buf_size = buffer_size;
  int start_position = 0;
  int v_len = v.size();
  ClientContext context;
  std::unique_ptr<ClientWriter<PutRequest> > writer(stub_->PutValues(&context, &PRsp));
  vector<PutRequest> put_vec;
  while ((start_position + put_buf_size) < v_len)
    {
    PutRequest* PReq = new PutRequest;
    PReq->set_row(r);
    PReq->set_col(c);
    PReq->set_value(v.substr(start_position, put_buf_size));
    PReq->set_version(to_string(seq_num));
    put_vec.push_back(*PReq);
    start_position += put_buf_size;
    }
  PutRequest* PReq = new PutRequest;
  PReq->set_row(r);
  PReq->set_col(c);
  PReq->set_value(v.substr(start_position));
  PReq->set_version(to_string(seq_num));
  put_vec.push_back(*PReq);
  for (const PutRequest& PRe : put_vec)
    {
    if (!writer->Write(PRe)) {
      // Broken stream.
      cout << "Here at Broken Put Stream" << endl;
      break;
      }
    }
  writer->WritesDone();
  Status status = writer->Finish();
  if (!status.ok()) {
    // If status returned is not Okay
    cout << "KVC: PutValues rpc failed." << endl;
    return false;
    }
  if (PRsp.status().empty()) {
    // If the status variable returned is empty
    cout << "KVC: PutValues found no status for {row key, col key} "
      << r << ", " << c << endl;
    return false;
    }
  else {
    // The status variable is not empty
    cout << "KVC: PutValues got status: " << PRsp.status() << " for row key: "
      << r << ", col key: " << c << endl;
    }
  return true;
  }


bool KValueStoreClient::CPutValues(string r, string c, string v1,
  string v2, int seq_num) {
  CPutRequest CPReq;
  CPReq.set_row(r);
  CPReq.set_col(c);
  CPReq.set_value1(v1);
  CPReq.set_value2(v2);
  CPReq.set_version(to_string(seq_num));
  CPutResponse CPRsp;
  return cputOneValue(CPReq, &CPRsp);
  }


bool KValueStoreClient::DeleteValues(string r, string c, int seq_num) {
  DeleteRequest dReq;

  dReq.set_row(r);
  dReq.set_col(c);
  dReq.set_version(to_string(seq_num));

  DeleteResponse dRsp;
  return deleteOneValue(dReq, &dRsp);
  }

bool KValueStoreClient::GetDataId(std::string id, string& full_get) {
  AdminDataReq aReq;

  aReq.set_id(id);

  AdminDataRsp aRsp;
  ClientContext context;
  std::unique_ptr<ClientReader<AdminDataRsp> > reader(
      stub_->GetDataId(&context, aReq));
  string value = "";
  string row;
  string col;
  string out_status;
  bool first_time = true;
  bool a;
  while (reader->Read(&aRsp))
  {
	  if(first_time == true)
	  {
		  first_time = false;
		  row = aRsp.row();
		  col = aRsp.col();
		  out_status = aRsp.status();
		  value = aRsp.value();
	  }
	  else
	  {
		  value += aRsp.value();
	  }
  }
  Status status = reader->Finish();
  if (status.ok())
  {
	  full_get = "KVClient got status:" + out_status + " row:" + row + " col:" + col + " val:" + value;
	  a = true;
  }
  else
  {
	  cout << "KVClient getDataId rpc failed." << endl;
	  a = false;
  }
  // bool a = getDataOneValue(aReq, &aRsp, full_get);
  return a;
  }

void KValueStoreClient::SwitchServer(std::string msg) {
  AdminSwitchReq sReq;

  sReq.set_msg(msg);

  AdminSwitchRsp sRsp;
  bool a = switchOneValue(sReq, &sRsp);
  }

int KValueStoreClient::getSeqOneValue(const SeqRequest& srt, SeqResponse* srp) {
  ClientContext context;
  int version = -1;
  Status status = stub_->GetSeqNum(&context, srt, srp);
  if (!status.ok()) {
    cout << "KVC: GetSeqNum rpc failed." << endl;
    }
  else if (srp->status().empty()) {
    cout << "KVC: GetSeqNum found no value for {row key, col key} " << srt.row() << ", " << srt.col() << endl;
    }
  else {
    cout << "KVC: GetSeqNum got status: " << srp->status()
      << " with version number " << srp->version()
      << endl;
    version = stoi(srp->version());
    }
  return version;
  }


// string KValueStoreClient::getOneValue(const GetRequest& grt, GetResponse* grp) {
//   ClientContext context;

//   Status status = stub_->GetValues(&context, grt, grp);
//   if (!status.ok()) {
//     cout << "KVC: GetValue rpc failed." << endl;
//     return "";
//     }
//   if (grp->value().empty()) {
//     cout << "KVC: GetValue found no value for {row key, col key} " << grt.row() << ", " << grt.col() << endl;
//     return "";
//     }
//   else {
//     cout << "KVC: GetValue got value for {row key, col key} " << grt.row() << ", " << grt.col()
//       << endl;
//     }
//   return grp->value();
//   }

//bool KValueStoreClient::putOneValue(const PutRequest& preq, PutResponse* prsp) {
//  // Initialize the function caller to be a client
//  ClientContext context;
//  // Call the PutValues function
//  Status status = stub_->PutValues(&context, preq, prsp);
//  if (!status.ok()) {
//    // If status returned is not Okay
//    cout << "KVC: PutValues rpc failed." << endl;
//    return false;
//    }
//  if (prsp->status().empty()) {
//    // If the status variable returned is empty
//    cout << "KVC: PutValues found no status for {row key, col key} "
//      << preq.row() << ", " << preq.col() << endl;
//    return false;
//    }
//  else {
//    // The status variable is not empty
//    cout << "KVC: PutValues got status: " << prsp->status() << " for row key: "
//      << preq.row() << ", col key: " << preq.col() << endl;
//    }
//  return true;
//  }

bool KValueStoreClient::cputOneValue(const CPutRequest& cpreq,
  CPutResponse* cprsp) {
  // Initialize the function caller to be a client
  ClientContext context;
  // Call the CPutValues function
  Status status = stub_->CPutValues(&context, cpreq, cprsp);
  if (!status.ok()) {
    // If status returned is not Okay
    cout << "KVC: CPutValues rpc failed." << endl;
    return false;
    }
  if (cprsp->status().empty()) {
    // If the status variable returned is empty
    cout << "KVC: CPutValues found no status for {row key, col key} "
      << cpreq.row() << ", " << cpreq.col() << endl;
    return false;
    }
  else {
    // The status variable is not empty
    cout << "KVC: CPutValues got Status: " << cprsp->status() << " with row: "
      << cpreq.row() << ", col: " << cpreq.col()
      << endl;
    }
  return true;
  }

bool KValueStoreClient::deleteOneValue(const DeleteRequest& dReq,
  DeleteResponse* dRsp) {
  // Initialize the function caller to be a client
  ClientContext context;
  // Call the CPutValues function
  Status status = stub_->DeleteValues(&context, dReq, dRsp);
  if (!status.ok()) {
    // If status returned is not Okay
    cout << "KVC: DeleteValues rpc failed." << endl;
    return false;
    }
  if (dRsp->status().empty()) {
    // If the status variable returned is empty
    cout << "KVC: DeleteValues found no status." << endl;
    return false;
    }
  else {
    // The status variable is not empty
    cout << "KVC: DeleteValues got Status: " << dRsp->status() << endl;
    }
  return true;
  }


//bool KValueStoreClient::getDataOneValue(const AdminDataReq& aReq,
//  AdminDataRsp* aRsp, string& full_get) {
//  // Initialize the function caller to be a client
//  ClientContext context;
//  // Call the CPutValues function
//  Status status = stub_->GetDataId(&context, aReq, aRsp);
//  if (!status.ok()) {
//    // If status returned is not Okay
//    cout << "KVClient getDataId rpc failed." << endl;
//    return false;
//    }
//  if (aRsp->status().empty()) {
//    // If the status variable returned is empty
//    cout << "KVClient getDataId found no status." << endl;
//    return false;
//    }
//  else {
//    // The status variable is not empty
//    cout << "KVClient got Status: " << aRsp->status() << endl;
//    cout << "KVClient got row: " << aRsp->row() << endl;
//    cout << "KVClient got col: " << aRsp->col() << endl;
//    //status,row_len,col_len,val_length,rowcolval
//    full_get = "KVClient got status:" + aRsp->status() + " row:" + aRsp->row() + " col:" + aRsp->col() + " val:" + aRsp->value();
//
//    // cout << "full_get is:" << endl;
//    // cout << full_get << endl;
//    }
//  return true;
//  }


bool KValueStoreClient::switchOneValue(const AdminSwitchReq& sReq,
  AdminSwitchRsp* sRsp) {
  // Initialize the function caller to be a client
  ClientContext context;
  // Call the CPutValues function
  Status status = stub_->SwitchServer(&context, sReq, sRsp);
  if (!status.ok()) {
    // If status returned is not Okay
    cout << "SwitchServer rpc failed." << endl;
    return false;
    }
  if (sRsp->status().empty()) {
    // If the status variable returned is empty
    cout << "SwitchServer found no status." << endl;
    return false;
    }
  else {
    // The status variable is not empty
    cout << "Got Status: " << sRsp->status() << endl;
    }
  return true;
  }


CoordinationClient::CoordinationClient(std::shared_ptr<Channel> channel) :
  stub_(Coordination::NewStub(channel)) {
  cout << "Coordination Client: Ready to communicate with server!" << endl;
  }



void CoordinationClient::GetActiveServers(string& activelist, string& entirelist) {
  AdminSeverReq ASreq;
  ASreq.set_msg("lol");
  AdminSeverRsp ASrsp;
  getactiveValue(ASreq, &ASrsp, activelist, entirelist);
  }



void CoordinationClient::getactiveValue(const AdminSeverReq& ASreq, AdminSeverRsp* ASrsp, string& activelist, string& entirelist) {
  ClientContext context;
  Status status = stub_->GetActiveServers(&context, ASreq, ASrsp);
  if (!status.ok()) {
    // If status returned is not Okay
    cout << "Coordination Client: GetActiveServers rpc failed." << endl;
    return; //string("");
    }
  if (ASrsp->status().empty()) {
    // If the status variable returned is empty
    cout << "Coordination Client: GetActiveServers found no status." << endl;
    return; //string("");
    }
  else {
    // The status variable is not empty
    cout << "Coordination Client: Got Status: " << ASrsp->status()
      << " and Value: " << ASrsp->serverlist() << endl;
    }
  activelist = ASrsp->serverlist();
  entirelist = ASrsp->allserverlist();
  // return ASrsp->serverlist();
  }


void CoordinationClient::GetBackend(string row_key, string& server) {
  BkendReq bReq;
  bReq.set_rowkey(row_key);
  BkendRsp bRsp;
  getBackendValue(bReq, &bRsp, server);
  }

void CoordinationClient::getBackendValue(const BkendReq& breq, BkendRsp* brsp, string& server) {
  ClientContext context;
  Status status = stub_->GetBackend(&context, breq, brsp);
  if (!status.ok()) {
    // If status returned is not Okay
    cout << "GetBackend rpc failed." << endl;
    return; //string("");
    }
  if (brsp->status().empty()) {
    // If the status variable returned is empty
    cout << "GetBackend found no backend server." << endl;
    return; //string("");
    }
  else {
    // The status variable is not empty
    cout << "Got Status: " << brsp->status() << " and backend node: " << brsp->replicas() << endl;
    }
  server = brsp->replicas();
  }

// int main(int argc, char** argv) {
//   CoordinationClient backend_master(
//     grpc::CreateChannel("localhost:7777",
//       grpc::InsecureChannelCredentials()));
//   string replica;
//   string user1 = "linh_file";
//   string col1 = "file1";
//   string value1 = "value1";
//   string user2 = "linh_mail";
//   string col2 = "file2";
//   string value2 = "value2";
//   // string user2 = "linh_mail";
//   // string user3 = "linh_bubble";

//   backend_master.GetBackend(user1, replica);
//   cout << "get replica for " << user1 << ": " << replica << endl;
//   vector < string> replica_vec;
//   boost::split(replica_vec, replica, boost::is_any_of(","));

//   vector<KValueStoreClient*> connections;
//   for (auto& cfig : replica_vec) {
//     KValueStoreClient* node = new KValueStoreClient(grpc::CreateChannel(cfig,
//       grpc::InsecureChannelCredentials()));
//     connections.push_back(node);
//     }

//   // 
//   // put command:
//   for (int i = 0; i < NUM_OF_W; ++i) {
//     int curr_max_id;
//     int s_num = connections[i]->GetSeqNum(user1, col1);

//     }

//   }







// int main(int argc, char** argv) {




//   CoordinationClient kvClient2(
//     grpc::CreateChannel("localhost:7777",
//       grpc::InsecureChannelCredentials()));
//   string servlist;
//   string servlist2;
//   kvClient2.GetActiveServers(servlist, servlist2);
//   cout << "Got: " << servlist << " From: " << servlist2 << endl;

//   std::string str_row("linhphan_file");
//   std::string str_row2("linhphan_mail");
//   std::string str_col("file1");
//   std::string str_val("Dog vs Shark");
//   std::string str_val2("Hello World");
//   string bankendserver1;
//   string bankendserver2;
//   kvClient2.GetBackend(str_row, bankendserver1);
//   cout << "got bankendserver: " << bankendserver1 << endl;

//   KValueStoreClient kvClient(
//     grpc::CreateChannel(bankendserver1,
//       grpc::InsecureChannelCredentials()));

//   kvClient.PutValues(str_row, str_col, str_val);
//   kvClient.GetValues(str_row, str_col);
//   kvClient.GetValues(str_row2, str_col);
//   // cout << "Here" << endl;
//   kvClient.CPutValues(str_row2, str_col, str_val, str_val2);
//   kvClient.CPutValues(str_row, str_col, str_val, str_val2);
//   kvClient.GetValues(str_row, str_col);
//   kvClient.DeleteValues(str_row, str_col);
//   kvClient.GetValues(str_row, str_col);
//   kvClient.GetDataId("1");
//   // kvClient.SwitchServer("lol");
//   // kvClient.SwitchServer("lala");
//   kvClient.PutValues(str_row2, str_col, str_val2);
//   kvClient2.GetActiveServers(servlist, servlist2);
//   cout << "Got: " << servlist << " From: " << servlist2 << endl;
//   // kvClient.PutValues(str_row, str_col, str_val);
//   // kvClient.GetValues(str_row, str_col);
//   // kvClient.CPutValues(str_row, str_col, str_val, str_val2);
//   // kvClient.GetDataId("1");
//   // kvClient.DeleteValues(str_row, str_col);
//   // kvClient.SwitchServer("lol");
//   // kvClient.GetDataId("0");
//   // kvClient2.GetActiveServers(servlist, servlist2);
//   // cout << "Got: " << servlist << " From: " << servlist2 << endl;


//   //	kvClient.GetActiveServers();
//   //	// Connect to Admin
//   //	KValueStoreClient kvClient2(
//   //			grpc::CreateChannel("localhost:7777",
//   //					grpc::InsecureChannelCredentials()));
//   //	kvClient2.GetActiveServers();
//   //	cout << "-------------- GetValues --------------" << endl;
//   //	kvClient.GetValues();
//   //	cout << "-------------- PutValues --------------" << endl;
//   //	kvClient.PutValues(str_row, str_col, str_val);
//   //	cout << "-------------- CPutValues --------------" << endl;
//   //	std::string str_val2("Dog vs Sharp");
//   //	kvClient.CPutValues(str_row, str_col, str_val, str_val2);
//   //	cout << "-------------- DeleteValues --------------" << endl;
//   //	kvClient.DeleteValues();
//   //	cout << "-------------- GetActiveServers --------------" << endl;
//   //	kvClient.GetActiveServers();
//   //	cout << "-------------- GetData --------------" << endl;
//   //	kvClient.GetData();
//   return 0;
//   }
