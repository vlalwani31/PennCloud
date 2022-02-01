#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <map>
#include <vector>
#include "key_value_store_client.h"

bool do_write(int fd, char *buf, int len);
std::vector<std::string> get_server_vec(string server_line);
string get_formatted_admin_contents(string st);
std::string char_arr_to_string(char arr[], int start, int end);
std::map<std::string, std::string> parse_query_params(std::string post_data);
std::map<std::string, std::string> parse_recipients(std::string recipient_field);
std::string get_time();
std::vector<std::string> parse_request_line(std::string reqline);
std::vector<std::vector<std::string>> parse_dir_contents(std::string dir_contents);
std::string parse_uri(std::string root, std::string uri);
std::string remove_from_parent(std::string parent, std::string child);
void recursive_delete(std::string backend_ip, std::string user, std::string col_key_prefix);
std::string get_file_hash(std::string filename, std::string parent_id);
std::map<std::string, std::string> get_mv_options(std::string backend_ip,
                                                std::string dir_id,
                                                std::string user,
                                                std::string parent,
                                                std::string calling_dir);
std::map<std::string, std::string> get_all_files(std::string backend_ip, std::string user,
                                  std::string this_dir_id, std::string parent_name);
std::string create_cookie(std::string username, std::string pwd);
std::string get_UTC();
bool check_cookie(std::string cookie_from_browser, std::string ip);
KValueStoreClient create_RPC_client(std::string ip);

bool is_command(char* input, std::string cmd, int length);
bool valid_addr(std::string addr);
std::string parse_addr(std::string addr);

bool put_local_box(std::string username, std::string domain, std::string put_value, std::string backend_ip);

#endif
