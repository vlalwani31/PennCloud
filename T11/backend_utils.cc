#include "backend_utils.h"

// read all logs in the log file and store it in vector logs
// Format:     0, 3, 5, 6\n  (0 for write and 1 for delete), the following three numbers are the size info
//             rowcolvalue\n
int read_log_file(string logfile, vector<string>& logs) {
  logs.clear();
  string line;

  ifstream file(logfile);
  if (!file.is_open())
    return FAILURE;

  bool readNums = true;
  stringstream ss;
  vector<int> size_info;
  int total_size = 0;
  while (getline(file, line)) {

    if (readNums) {
      ss << line << "\n";
      get_size_info(size_info, line);
      // first one is DELETE or WRITE
      for (auto iter = size_info.begin() + 1; iter < size_info.end() - 1;
        ++iter) {
        total_size += *iter;
        }
      // cout << "initialized total size is " << total_size << endl;
      readNums = false;
      }
    else {
      ss << line;
      total_size -= line.size();
      // cout << "total size is " << total_size << endl;
      if (total_size <= 0) {
        logs.push_back(ss.str());
        total_size = 0;
        readNums = true;
        ss.str("");
        }
      else {
        ss << "\n";
        total_size -= 1;  // include newline size
        }
      }
    }

  file.close();
  return SUCCESS;
  }

void get_size_info(vector<int>& key_size, string config) {
  key_size.clear();
  vector < string > result;
  boost::split(result, config, boost::is_any_of(","));

  for (int i = 0; i < result.size(); i++)
    key_size.emplace_back(stoi(result[i]));
  }


int update_kvstore_with_log(string logfile, map<string, UserData*>& storage) {

  vector < string > saved_logs;
  if (int status = read_log_file(logfile, saved_logs) == 0) {
    cout << "Reading Log file failed" << endl;
    return FAILURE;
    }
  vector<int> size_info;
  for (auto& log : saved_logs) {
    size_t found = log.find_first_of("\n");
    if (found != string::npos) {
      string s_info = log.substr(0, found);
      get_size_info(size_info, s_info);
      if (size_info[0] == 0) {
        if (size_info.size() > 4) { // flag, rowlen. collen, vallen
          int row_len = size_info[1];
          int col_len = size_info[2];
          int val_len = size_info[3];
          int version_number = size_info[4];
          string row = log.substr(found + 1, row_len);
          string col = log.substr(found + row_len + 1, col_len);
          string val = log.substr(found + row_len + col_len + 1, val_len);
          // cout << "Row: " << row << " Col: " << col << " Value: " << val << endl;
          if (storage.find(row) == storage.end()) {
            storage.insert(
              make_pair(row, new UserData(row, logfile)));
            }
          storage[row]->numOfEntries++;
          storage[row]->data[col] = val;
          storage[row]->version_num[col] = version_number;
          //if (version_number > storage[row]->version_number) {
          // storage[row]->version_number = version_number;
          //  }

          }
        else {
          cout << "Not enough size info for write" << endl;
          }
        }
      else if (size_info[0] == 1) {
        if (size_info.size() > 3) { // flag, rowlen, collen
          int row_len = size_info[1];
          int col_len = size_info[2];
          int version_number = size_info[3];
          string row = log.substr(found + 1, row_len);
          string col = log.substr(found + row_len + 1, col_len);
          // cout << "Row: " << row << " Col: " << col << endl;
          storage[row]->numOfEntries--;
          storage[row]->data.erase(col);
          //if (version_number > storage[row]->version_number[col]) {
          storage[row]->version_num[col] = version_number;
          //  }
          }
        else {
          cout << "Not enough size info for delete" << endl;
          }
        }
      else {
        cout << "Unidentified command in log_file" << size_info[0]
          << endl;
        }
      }
    // cout << log << endl;
    }

  return SUCCESS;
  }

int update_kvstore_with_cp(string cpfile, string logfile, map<string, UserData*>& storage) {
  stringstream ss;
  string line;
  ifstream file(cpfile);
  if (!file.is_open())
    return FAILURE;

  bool readNums = true;
  vector<int> size_info;
  int total_size = 0;
  string record, row, col, val;
  while (getline(file, line)) {
    if (readNums) {
      // ss << line << "\n";
      get_size_info(size_info, line);
      total_size = size_info[0] + size_info[1] + size_info[2];
      readNums = false;
      }
    else {
      ss << line;
      total_size -= line.size();
      // cout << "total size is " << total_size << endl;
      if (total_size <= 0) {
        // logs.push_back(ss.str());
        record = ss.str();
        int row_len = size_info[0];
        int col_len = size_info[1];
        int val_len = size_info[2];
        int version_number = size_info[3];
        row = record.substr(0, row_len);
        col = record.substr(row_len, col_len);
        val = record.substr(row_len + col_len, val_len);

        if (storage.find(row) == storage.end()) {
          storage.insert(
            make_pair(row, new UserData(row, logfile)));
          }
        storage[row]->numOfEntries++;
        storage[row]->data[col] = val;
        // if (version_number > storage[row]->version_number) {
        storage[row]->version_num[col] = version_number;
        //  }


        total_size = 0;
        readNums = true;
        ss.str("");
        }
      else {
        ss << "\n";
        total_size -= 1;  // include newline size
        }
      }
    }

  file.close();
  return SUCCESS;

  }

uint32_t one_at_a_time_hash(string key) {
  size_t i = 0;
  uint32_t hash = 0;
  while (i != key.size()) {
    hash += key[i++];
    hash += hash << 10;
    hash ^= hash >> 6;
    }
  hash += hash << 3;
  hash ^= hash >> 11;
  hash += hash << 15;
  return hash;
  }

uint32_t djb2_hash(string key, uint32_t hash_key) {
  char* str = &key[0];
  //	uint32_t hash_key = 5381;

  int c;
  while (c = *str++) {
    // magic number 33 -> hash * 33 + c
    hash_key = ((hash_key << 5) + hash_key) + c;
    }
  return hash_key;
  }

uint32_t sdbm_hash(string key, uint32_t hash_key) {
  char* str = &key[0];
  //	uint32_t hash_key = 0;
  int c;
  while (c = *str++) {
    // hash[i] = hash[i-1] * 65599 + str[i]
    hash_key = c + (hash_key << 6) + (hash_key << 16) - hash_key;
    }
  return hash_key;
  }

uint32_t sdbm_hash2(string key, uint32_t hash_key) {
  char* str = &key[0];
  //	uint32_t hash_key = 0;
  int c;
  while (c = *str++) {
    // hash[i] = hash[i-1] * 65599 + str[i]
    hash_key = c + (hash_key << 8) + (hash_key << 16) - hash_key;
    }
  return hash_key;
  }

uint32_t sdbm_hash3(string key, uint32_t hash_key) {
  char* str = &key[0];
  //	uint32_t hash_key = 0;
  int c;
  while (c = *str++) {
    // hash[i] = hash[i-1] * 65599 + str[i]
    hash_key = c + (hash_key << 7) + (hash_key << 16) - hash_key;
    }
  return hash_key;
  }

// id starts from 0 to numOfServers - 1
void hash_func(int numOfServers, vector<int>& node_set, string& key) {
  node_set.clear();
  node_set.emplace_back(one_at_a_time_hash(key) % (numOfServers));
  uint32_t hash_key_djb2 = 5381, hash_key_sdbm = 0;
  int djb2_result, sdbm_result;
  // make sure unique nodes are selected
  do {
    djb2_result = (int)(djb2_hash(key, hash_key_djb2) % (numOfServers));
    hash_key_djb2++;
    } while (find(node_set.begin(), node_set.end(), djb2_result)
      != node_set.end());
    node_set.emplace_back(djb2_result);
    do {
      sdbm_result = (int)(sdbm_hash(key, hash_key_sdbm) % (numOfServers));
      hash_key_sdbm++;
      } while (find(node_set.begin(), node_set.end(), sdbm_result)
        != node_set.end());
      node_set.emplace_back(sdbm_result);

      do {
        sdbm_result = (int)(sdbm_hash2(key, hash_key_sdbm) % (numOfServers));
        hash_key_sdbm++;
        } while (find(node_set.begin(), node_set.end(), sdbm_result)
          != node_set.end());
        node_set.emplace_back(sdbm_result);
        do {
          sdbm_result = (int)(sdbm_hash3(key, hash_key_sdbm) % (numOfServers));
          hash_key_sdbm++;
          } while (find(node_set.begin(), node_set.end(), sdbm_result)
            != node_set.end());
          node_set.emplace_back(sdbm_result);
  }


