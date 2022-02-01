#ifndef BACKEND_UTILS_H_
#define BACKEND_UTILS_H_

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string>
#include <vector>
#include <cstdint>
#include <map>
#include <iostream>
#include <algorithm>
#include <fstream>
#include <bits/stdc++.h>
#include <boost/algorithm/string.hpp>
#include "user_data.h"

using namespace std;

#define SUCCESS 1
#define FAILURE 0


//one_at_a_time (check wiki page: Jenkins hash function)
uint32_t one_at_a_time_hash(string key);
uint32_t djb2_hash(string key, uint32_t hash_key);
uint32_t sdbm_hash(string key, uint32_t hash_key);
uint32_t sdbm_hash2(string key, uint32_t hash_key);
uint32_t sdbm_hash3(string key, uint32_t hash_key);

void hash_func(int numOfServers, vector<int>& node_set, string& key);


int read_log_file(string logfile, vector<string>& logs);
void get_size_info(vector<int>& key_size, string config);
int update_kvstore_with_log(string logfile, map<string, UserData*>& storage);
int update_kvstore_with_cp(string cpfile, string logfile, map<string, UserData*>& storage);
#endif /* BACKEND_UTILS_H_ */
