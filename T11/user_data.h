#ifndef USER_DATA_H_
#define USER_DATA_H_

#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <vector>
#include <map>
#include <iostream>
#include <sstream>
#include <utility>
#include <fstream>
#include <algorithm>

using namespace std;

#define WRITE_LOG 0
#define DELETE_LOG 1

#define SUCCESS 1
#define FAILURE 0

class UserData {
public:
  string row_key;
  map<string, string> data;
  map<string, int> version_num;
  string logfile;
  int numOfEntries;
  int version_number;
  UserData(string user_row_key, string serverlogfile) {
    row_key = user_row_key;
    logfile = serverlogfile;
    numOfEntries = 0;
    version_number = 0;
    }

  void put(string col_key, string v_num, string& value, string& status);
  void get(string col_key, string& value, string& status);
  void cput(string col_key, string v_num, string& value1, string& value2, string& status);
  void del(string col_key, string v_num, string& status);

  void write_entry(string col_key, string value);
  void delete_entry(string col_key);

  void write_checkpoint(string cpfile);
  };

int write_to_log(string logfile, string log);


#endif /* USER_DATA_H_ */
