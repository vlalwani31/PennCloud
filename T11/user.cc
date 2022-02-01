#include "user.h"



User::User(string _username, string backend_master_ip) {
  CoordinationClient backend_master_client(grpc::CreateChannel(
    backend_master_ip, grpc::InsecureChannelCredentials()));
  CoordinationClient* backend_master = &backend_master_client;
  username = _username;
  // ask for replicas
  string serverlist;
  backend_master->GetBackend(username, serverlist);
  boost::split(replica_group, serverlist, boost::is_any_of(","));
  for (auto& config : replica_group) {
    KValueStoreClient* kvClient = new KValueStoreClient(
      grpc::CreateChannel(config,
        grpc::InsecureChannelCredentials()));
    connections.push_back(kvClient);
    }
  }

string User::get_value(string col_key) {
  // ask for seq num
  map<int, int> seq_nums; // idex of connections, seq_num
  // load balancing: shuffle connections
  srand(unsigned(time(0)));
  random_shuffle(connections.begin(), connections.end());

  int num_of_response = 0;
  int iter = 0;
  while (iter < connections.size()) {
    int seq = connections[iter]->GetSeqNum(username, col_key);
    seq_nums[iter] = seq;
    if (seq >= 0) {
      num_of_response++;
      if (num_of_response == NUM_OF_R) break;
      }
    iter++;
    }
  cout << "num of response: " << num_of_response << endl;

  // TODO: Frontend need to check and handle this situation
  if (num_of_response == 0) return ""; // return empty string when found no data 
  // get maximum value 
  int max_seq = -1;
  for (auto& [id, num] : seq_nums) {
    if (num > max_seq) max_seq = num;
    }

  // assume everything works fine
  // then any replica that has the same highest seq num would return the same latest num
  bool isDataSet = false;
  vector<int> outdated_replica;
  string data;
  for (auto& [i, num] : seq_nums) {
    if (num != max_seq) {
      outdated_replica.push_back(i);
      }
    else if (!isDataSet) {
      data = connections[i]->GetValues(username, col_key);
      isDataSet = true;
      }
    }

  // update if any R was outdated
  for (auto& i : outdated_replica) {
    connections[i]->PutValues(username, col_key, data, max_seq);
    }
  return data;
  }

int User::max_seq_num(string col_key) {
  map<int, int> seq_nums;
  srand(unsigned(time(0)));
  random_shuffle(connections.begin(), connections.end());
  int num_of_response = 0;
  int iter = 0;
  while (iter < connections.size()) {
    int seq = connections[iter]->GetSeqNum(username, col_key);
    seq_nums[iter] = seq;
    if (seq >= 0) {
      num_of_response++;
      if (num_of_response == NUM_OF_W) break;
      }
    iter++;
    }
  if (num_of_response < NUM_OF_W) return -1;
  int max_seq = 0;
  for (auto& [id, num] : seq_nums) {
    if (num > max_seq) max_seq = num;
    }
  return max_seq;
  }

bool User::put_value(string col_key, string val)
  {
  int max_seq;
  if ((max_seq = max_seq_num(col_key)) < 0) return false;
  cout << "ready to put value with " << max_seq + 1 << "seq num" << endl;
  for (auto& config : connections) {
    config->PutValues(username, col_key, val, max_seq + 1);
    }
  return true;
  }


bool User::cput_value(string col_key, string orig_val, string new_val)
  {
  int max_seq;
  if ((max_seq = max_seq_num(col_key)) < 0) return false;
  cout << "ready to cput_value with " << max_seq + 1 << "seq num" << endl;
  for (auto& config : connections) {
    config->CPutValues(username, col_key, orig_val, new_val, max_seq + 1);
    }
  return true;
  }

bool User::delete_value(string col_key)
  {
  int max_seq;
  if ((max_seq = max_seq_num(col_key)) < 0) return false;
  cout << "ready to delete_value with " << max_seq + 1 << "seq num" << endl;
  for (auto& config : connections) {
    config->DeleteValues(username, col_key, max_seq + 1);
    }
  return true;
  }


// int main(int argc, char** argv)
//   {
//   // CoordinationClient backend_master(
//   //   grpc::CreateChannel("localhost:7777",
//   //     grpc::InsecureChannelCredentials()));
//   User a(string("linh_lol"), "localhost:7777");
//   string col1("child videos");
//   string col2("mickey");
//   string col3("voodoo magic");
//   string col4("col4");
//   string col5("col5");
//   string col6("col6");
//   string col7("col7");
//   string col8("col8");
//   string col9("col9");

//   a.put_value(col1, string("lol"));
//   a.put_value(col2, string("building"));
//   a.put_value(col3, string("hehe"));
//   a.put_value(col1, string("stop, do not open it!"));
//   a.get_value(col1);
//   a.delete_value(col1);
//   a.put_value(col2, string("Here comes the child magic"));
//   a.cput_value(col3, string("hehe"), string("uwuwu"));

//   a.put_value(col4, string("col4"));
//   a.put_value(col5, string("col5"));
//   a.put_value(col6, string("col6"));
//   a.put_value(col7, string("col7"));
//   a.put_value(col8, string("col8"));
//   a.put_value(col9, string("col9"));
//   return 0;
//   }
