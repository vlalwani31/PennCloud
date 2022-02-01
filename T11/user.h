#ifndef USER_H
#define USER_H

#include <string>
#include <map>
#include <bits/stdc++.h>

#include "key_value_store_client.h"

#include <boost/algorithm/string.hpp>

#define NUM_OF_REPLICA 5
#define NUM_OF_R 3
#define NUM_OF_W 3

using namespace std;
class User {
public:
  // map<string, string> read_quorum; //map from ip to port number
  // map<string, string> write_quorum;
  string username;
  vector<string> replica_group;
  vector<KValueStoreClient*> connections;

  // constructor
  User(string _username, string backend_master_ip);

  // standard interface functions   
  string get_value(string col_key);
  bool put_value(string col_key, string val);
  bool cput_value(string col_key, string orig_val, string new_val);
  bool delete_value(string col_key);

private:
  int max_seq_num(string col_key);
  };

#endif
