#include "user_data.h"


// write log to file, append if exists, create if not exist.
// TODO: @Varun add lock here, replace the use variable: version number
int write_to_log(string logfile, string log) {
  fstream file;
  file.open(logfile, ios_base::app | ios_base::in);
  if (!file.is_open())
    return FAILURE;
  // cout << "write to the logfile " << log << endl;
  file << log << endl;
  file.close();
  return SUCCESS;
  }


void UserData::write_entry(string col_key, string value) {
  // PUT + row key + col key + value
  stringstream ss;
  ss << WRITE_LOG << "," << row_key.size() << "," << col_key.size() << ","
    << value.size() << "," << version_num[col_key] << "\n";
  ss << row_key << col_key << value;
  string result = ss.str();
//  cout << "get the log " << result << endl;
//  cout << "the logfile is " << logfile << endl;
  write_to_log(logfile, result);

  }

void UserData::delete_entry(string col_key) {
  // DELETE + row key + col key
  stringstream ss;
  ss << DELETE_LOG << "," << row_key.size() << "," << col_key.size() << "," << version_num[col_key] << "\n";
  ss << row_key << col_key;
  string result = ss.str();
  write_to_log(logfile, result);

  }

void UserData::put(string col_key, string v_num, string& value, string& status) {
  status.clear();
  if (data.find(col_key) != data.end()) {
    int new_v = stoi(v_num);
    if (new_v > version_num[col_key]) {
      version_num[col_key] = new_v;
      data[col_key] = value;
      status = "+OK data updated";
      }
    else {
      status = "-ERR. There is already an entry at this location.\r\n";
      }
    }
  else if (col_key == "") {
    status = "-ERR. Empty col_key.\r\n";
    }
  else {
    version_num[col_key] = stoi(v_num);
    write_entry(col_key, value);
    numOfEntries++;
    data[col_key] = value;
    status = "+OK.\r\n";
    }
  }

void UserData::get(string col_key, string& value, string& status) {
  status.clear();
  if (data.find(col_key) != data.end()) {
    value = data[col_key];
    status = "+OK.\r\n";
    }
  else {
    value.clear();
    status = "-ERR. Invalid colKey.\r\n";
    }
  }

void UserData::cput(string col_key, string v_num, string& value1, string& value2,
  string& status) {
  status.clear();
  if (data.find(col_key) != data.end()) {
    int new_v = stoi(v_num);
    if (new_v > version_num[col_key]) {
      version_num[col_key] = new_v;
      if (data[col_key] == value1) { // store value2
        write_entry(col_key, value2);
        data[col_key] = value2;
        status = "+OK. [" + value2 + "] is stored.\r\n";
        }
      else {
        status = "+OK. No change.\r\n";
        }
      }
//      cout << "new_v is: " << new_v << endl;
//      cout << "version: " << version_num[col_key] << endl;
    }
  else {
    status = "+OK. There is no entry at [" + row_key + "][" + col_key
      + "].\r\n";
    }
  }

void UserData::del(string col_key, string v_num, string& status) {
  status.clear();
  if (data.find(col_key) != data.end()) {
    version_num[col_key] = stoi(v_num);
    delete_entry(col_key);
    numOfEntries--;
    data.erase(col_key);
    status = "+OK. Value deleted.\r\n";
    }
  else {
    status = "-ERR. Invalid colKey.\r\n";
    }
  }

void UserData::write_checkpoint(string cpfile) {
  // rowlen, collen, valuelen\n
  // rowkeycolkeyvaluekey\n
  fstream file;
  file.open(cpfile, ios_base::app | ios_base::in);
  if (!file.is_open())
    return;

  for (auto& [col_key, value] : data) {
    stringstream ss;
    ss << row_key.size() << "," << col_key.size() << "," << value.size() << "," << version_num[col_key]
      << "\n";
    ss << row_key << col_key << value;
    string result = ss.str();
    // cout << "get the checkpoint record " << result << endl;

    //		cout << "write to the logfile " << log << endl;
    file << result << endl;
    //		cout << "the cpfile is " << cpfile << endl;
    }
  file.close();

  }
