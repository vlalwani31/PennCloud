#include "utils.h"
#include "user.h"

#include <sys/time.h> //gettimeofday

#include <iostream>  //cout
#include <map>
#include <string>
#include <vector>
#include <string.h>

using namespace std;

bool do_write(int fd, char *buf, int len) {
	int sent = 0;
	while (sent < len) {
		int n = write(fd, &buf[sent],len-sent);
		if (n<0)
		return false;
		sent += n;
		}
	return true;
}

vector<string> get_server_vec(string server_line) {
	char *token = strtok((char*)server_line.c_str(), ",");
	vector<string> ss;

	while (token!=NULL) {
		ss.push_back(token);
		token = strtok(NULL, ",");
	}

	for (string s : ss) {
		cout << s << endl;
	}

	return ss;
}

/**
 * converts raw data into formatted form
 */

string get_formatted_admin_contents(string st) {
	int period = 33;
	int p_count = 0;
	while (p_count < st.length()) { //format html contents
		p_count+=period;

		if (p_count>=st.length()) {
			break;
		} else {
			string before = st.substr(0, p_count);
			string after = st.substr(p_count);
			string new_s = before + "<br>" + after;
			st = new_s;
		}
	}

	return st;
}

/* ========================================================== */
/* ===================== web server utils =================== */
/* ========================================================== */

/**
 * converts input char array from start to end indices (inclusive) into string
 */
string char_arr_to_string(char arr[], int start, int end) {
	string out;
	for (int i = start; i <= end; i++) {
		out.push_back(arr[i]);
	}
	return out;
}

/**
 * @brief Decodes UTF-8 encoded strings sent from browser
 * See https://www.w3schools.com/tags/ref_urlencode.ASP for details
 *
 * @param encoded
 * @return string
 */
string decode_utf(string encoded) {
    cout << "S: parse_query_params called decode_uti; decoding " << encoded << endl;
    map<string, string> dict = {
      {"%20", " "}, {"+", " "},   {"%21", "!"}, {"%22", "\""}, {"%23", "#"},
      {"%24", "$"}, {"%25", "%"}, {"%26", "&"}, {"%27", "\'"}, {"%28", "("},
      {"%29", ")"}, {"%2A", "*"}, {"%2B", "+"}, {"%2C", ","}, {"%2D", "-"}, {"%2E", "."}, 
      {"%2F", "/"}, {"%3A", ":"}, {"%3B", ";"}, {"%3C", "<"}, {"%3D", "="}, {"%3E", ">"},
      {"%3F", "?"},  {"%40", "@"}, {"%5B", "["}, {"%5C", "/"}, {"%5D", "]"}, {"%5E", "^"}, 
      {"%5F", "_"}, {"%7B", "{"}, {"%7C", "|"}, {"%7D", "}"}, {"%7E", "~"},
      {"%0D", "\r"}, {"%0A", "\n"}, {"%09", "	"}};

    int l = encoded.length();
    bool decoded_last_three = false;
    for (int i = 0; i < l - 2; i++) {
        if (encoded[i] == '%') {
            if (i == l - 3) {
                decoded_last_three = true;
            }
          	map<string, string>::iterator it = dict.find(encoded.substr(i, 3));
			if (it != dict.end()) {
				encoded.replace(i, 3, it->second);
				l = l - 2;
			}
        } else if (encoded[i] == '+') {
			encoded.replace(i, 1, " ");
		}
    }
    
    if (!decoded_last_three) {
        for (int i = l - 2; i < l; i++) {
            if (encoded[i] == '+') {
                encoded.replace(i, 1, " ");
            }
        }
    }
    return encoded;
}

/**
 * parses query params in post_data into map
 * e.g. email-recipient=hi&email-subject=test&email-content=world
 */
map<string, string> parse_query_params(string post_data) {
	map<string, string> output;
	size_t prev_delim = -1;
	size_t param_delim = post_data.find("&");
	do {
		string key_val_pair = post_data.substr(prev_delim + 1, param_delim - prev_delim - 1);
		cout << "S: Parsing query param " << key_val_pair << endl;

		size_t key_val_delim = key_val_pair.find("=");
		if (key_val_delim == string::npos) {
			cout << "ERROR malformed query param: " << key_val_pair << endl;
		}

		string key = key_val_pair.substr(0, key_val_delim);
		string val = key_val_pair.substr(key_val_delim + 1);
		output.insert(pair<string, string>(decode_utf(key), decode_utf(val)));

		prev_delim = param_delim;
		param_delim = post_data.find("&", param_delim + 1);
	} while (param_delim != string::npos);


	string key_val_pair = post_data.substr(prev_delim + 1, param_delim - prev_delim - 1);
	cout << "S: Parsing query param " << key_val_pair << endl;

	size_t key_val_delim = key_val_pair.find("=");
	if (key_val_delim == string::npos) {
		cout << "ERROR malformed query param: " << key_val_pair << endl;
	}

	string key = key_val_pair.substr(0, key_val_delim);
	string val = key_val_pair.substr(key_val_delim + 1);
	output.insert(pair<string, string>(decode_utf(key), decode_utf(val)));

	return output;
}

/**
 * parses string of comma-separated email recipients into map<username, domain>
 */
map<string, string> parse_recipients(string recipient_field) {
	map<string, string> recipients; //key = username, value = domain
	size_t recip_delim = recipient_field.find(",");
	int next_recip_start = 0;

	while (recip_delim != string::npos) {
		string recipient = recipient_field.substr(next_recip_start, recip_delim - next_recip_start);
		size_t username_delim = recipient.find("@");
		string username = recipient.substr(0, username_delim);
		string domain = recipient.substr(username_delim + 1);
		recipients.insert(pair<string, string>(username, domain));

		next_recip_start = recip_delim + 1;
		recip_delim = recipient_field.find(",", next_recip_start);
	}
	
	string recipient = recipient_field.substr(next_recip_start, recip_delim - next_recip_start);
	size_t username_delim = recipient.find("@");
	string username = recipient.substr(0, username_delim);
	string domain = recipient.substr(username_delim + 1);
	recipients.insert(pair<string, string>(username, domain));

	return recipients;
}

/**
 * Parses "&"-separated string of contents in a directory 
 * into vector of strings. First vector of strings is directories, second
 * vector of strings is files
 */
vector<vector<string>> parse_dir_contents(string dir_contents) {
    vector<vector<string>> contents;
    if (dir_contents == " ") {
        contents.push_back(vector<string>{});
        contents.push_back(vector<string>{});
        return contents;
    }
    vector<string> directories;
    vector<string> files;
    size_t next_delim = dir_contents.find(","), prev_delim = -1;

    do {
        size_t length = next_delim - prev_delim - 1;
        string item = dir_contents.substr(prev_delim + 1, length);
        //parse F:83159 or D:83159
        size_t type_delim = item.find(":");
        string item_type = item.substr(0, type_delim);
        if (item_type == "D") {
            directories.push_back(item.substr(type_delim + 1));
            cout << "S: parse_dir_contents stored the ID of " << item << " in directories\n";
        } else {
            files.push_back(item.substr(type_delim + 1));
            cout << "S: parse_dir_contents stored the ID of " << item << " in files\n";
        }
        prev_delim = next_delim;
        next_delim = dir_contents.find(",", prev_delim + 1);
    } while (prev_delim != string::npos);

    contents.push_back(directories);
    contents.push_back(files);
    return contents;
}

string get_time() {
	struct timeval tv;
	if (gettimeofday(&tv, NULL) < 0) {
		perror("ERROR failed to get time of day");
	}
	struct tm* t = localtime(&(tv.tv_sec));

	char formatted_time[16];
	if (snprintf(formatted_time, 16, "%02d:%02d:%02d.%06ld", t->tm_hour,
				t->tm_min, t->tm_sec, tv.tv_usec) < 0) {
		perror("ERROR failed to format time string");
	}
	return formatted_time;
}

/**
 * Removes the root of the uri to get parameters
 */
string parse_uri(string root, string uri) {
    if (uri == root) {
        cout << "S: parse_uri - uri " << uri << " is same as input root " << root << endl;
        return "";
    }
    size_t start = uri.find(root);
    if (start == string::npos) {
        cout << "S: parse_uri - uri " << uri << " doesn't contain root " << root << endl;
        return "";
    }
    start += root.length();
    string params = uri.substr(start);
    cout << "S: rest of uri parsed is " << params << endl;
    return decode_utf(params);
}

/**
 * Parses request line [method-token]SP[request-uri]SP[protocol-version]CRLF
 * and returns Request object instance
 */
vector<string> parse_request_line(string reqline) {
	size_t meth_end = reqline.find(" ");
	string meth_tok = reqline.substr(0, meth_end);
	size_t uri_end = reqline.find(" ", meth_end + 1);
	string uri = reqline.substr(meth_end + 1, uri_end - meth_end - 1);

	vector < string > parsed;
	parsed.push_back(meth_tok);
	parsed.push_back(uri);
	return parsed;
}

KValueStoreClient create_RPC_client(string ip) {
	KValueStoreClient kvClient(grpc::CreateChannel(ip, grpc::InsecureChannelCredentials()));
	return kvClient;
}

/**
 * Removes child file / folder from parent directory content 
 * and returns new parent directory content
 */
string remove_from_parent(string parent, string child) {
    //is only file in parent -- return " " to siginify empty directory
    if (child.length() == parent.length()) {
        return " ";
    }

    size_t child_start = parent.find(child);
    if (child_start == string::npos) {
        cout << "S: WARNING parent directory doesn't contain child file/folder " << child << "to be deleted\n";
        return parent;
    }
    // is first file in comma separated string
    if (child_start == 0) {
      return parent.substr(child.length() + 1); // additional +1 for comma
    }

    string before_child = parent.substr(0, child_start - 1);

    //is last file in comma separated string 
    if (child_start + child.length() == parent.length()) {
        return before_child;
    }

    string after_child = parent.substr(child_start + child.length());
    return before_child + after_child;
}

/**
 * recursively gets all directory names in absolute path format rooted at input dir_id
 * that calling_dir (or file) is allowed to move to (which excludes all descendats of calling_dir)
 * returns map from directory id to directory name
 * Note: calling_dir is uuid, parent is dir_name in abosolute path format
 */
map<string, string> get_mv_options(string backend_ip, string dir_id, string user, string parent, string calling_dir) {
    map<string, string> all_dirs;
    User kvClient(user, backend_ip);
    // Step 1: add current directory to output with parent appended
    string this_dir_name = kvClient.get_value("D:" + dir_id + ":name");
    string abs_path = parent.length() == 0 ? this_dir_name : parent + "/" + this_dir_name;
    if (dir_id != calling_dir) {
        all_dirs.insert(pair<string, string>(dir_id, abs_path));
    }

    string dir_contents = kvClient.get_value("D:" + dir_id + ":content");
    vector<vector<string>> parsed_dir_contents = parse_dir_contents(dir_contents);

    for (string dir : parsed_dir_contents[0]) {
        //ensures no descendants of calling directory get traversed
        if (dir != calling_dir) {
            map<string, string> children = get_mv_options(backend_ip, dir, user, abs_path, calling_dir);
            all_dirs.insert(children.begin(), children.end());
        }
    }
    return all_dirs;
}

/**
 * recursively gets all files in absolute path format
 * returns map from uuid to filename
 */
map<string, string> get_all_files(string backend_ip, string user, string this_dir_id, string parent_name) {
    map<string, string> all_files;
    User kvClient(user, backend_ip);

    // Step 1: get this directory's absolute path and contents
    string this_dir_name = kvClient.get_value("D:" + this_dir_id + ":name");
    string this_dir_abs_path = parent_name.length() == 0 ? this_dir_name : parent_name + "/" + this_dir_name;

    string this_dir_contents = kvClient.get_value("D:" + this_dir_id + ":content");
    vector<vector<string>> parsed_dir_contents = parse_dir_contents(this_dir_contents);

    // Step 2: add files in this directory to output
    for (string file_id : parsed_dir_contents[1]) {
        string filename = kvClient.get_value("F:" + file_id + ":name");
        all_files.insert(pair<string, string>(file_id, this_dir_abs_path + "/" + filename));
    }

    // Step 3: recurse on chidren directories in this directory and add children result to output
    for (string dir_id : parsed_dir_contents[0]) {
        map<string, string> children = get_all_files(backend_ip, user, dir_id, this_dir_abs_path);
        all_files.insert(children.begin(), children.end());
    }
    return all_files;
}

/**
 * recursively deletes contents of directory
 */
void recursive_delete(string backend_ip, string user, string col_key_prefix) {
    cout << "S: recursive delete invoked on directory " << col_key_prefix << endl;
    User kvClient(user, backend_ip);

    // Step 1: don't need to delete from parent directory contents since parent directory will be deleted

    // Step 2: delete column containing parent directory
    kvClient.delete_value(col_key_prefix + ":parent");
    cout << "S: removed mapping to parent directory\n";

    // Step 3: delete column containing name
    kvClient.delete_value(col_key_prefix + ":name");
    cout << "S: removed mapping from uuid to name\n";

    // Step 4: delete contents of folder
    string contents = kvClient.get_value(col_key_prefix + ":content");
    vector<vector<string>> dir_contents = parse_dir_contents(contents);
    vector<string> directories = dir_contents[0], files = dir_contents[1];
    // delete files first
    for (string file : files) {
        kvClient.delete_value(file + ":parent");
        kvClient.delete_value(file + ":name");
        kvClient.delete_value(file + ":content");
    }
    cout << "S: deleted files in directory\n";

    // recurse on contained folders
    for (string dir : directories) {
        recursive_delete(backend_ip, user, dir);
    }

    cout << "S: recursive delete returned for " << col_key_prefix << endl;
    kvClient.delete_value(col_key_prefix + ":content");
}

/**
 * generates uuid for file based on filename, parent id and current time
 */
string get_file_hash(string filename, string parent_id) {
    string key = filename + parent_id + get_time();
    hash<string> str_hash;
    size_t hash_value = str_hash(key);
    string hash_string = to_string(hash_value);
    return filename + "~" + hash_string;
}

/* ========================================================== */
/* =============== webserver COOKIE utils =================== */
/* ========================================================== */
string create_cookie(string username, string pwd) {
    //Set-Cookie: <em>value</em>[; expires=<em>date</em>][;domain=<em>domain</em>][; path=<em>path</em>][; secure] 
    string id = username + pwd;
    cout << "S: id generated from username to create cookie is " << id << endl;

    hash<string> str_hash;
    size_t hash_v = str_hash(id);
    string hash_s = to_string(hash_v);

    string cookie = username;
    cookie.append("~");
    cookie.append(hash_s);

    return cookie;
}

string get_UTC() {
	struct timeval input_seconds;
	gettimeofday(&input_seconds, NULL);
	int days, hours, minutes, seconds, micros;

	days = input_seconds.tv_sec / 60 / 60 / 24;
	hours = (input_seconds.tv_sec / 60 / 60) % 24;
	minutes = (input_seconds.tv_sec / 60) % 60;
	seconds = input_seconds.tv_sec % 60;
	micros = input_seconds.tv_usec;
	string UTC = to_string(hours)+":"+to_string(minutes)+":"+to_string(seconds)+"."+to_string(micros);
	return UTC;
}

bool check_cookie(string cookie_from_browser, string ip) {
	if (cookie_from_browser.find("~") == string::npos) {
		cout << "ERROR request contains wrong cookie format: " << cookie_from_browser << endl;
		return false;
	}

	string username = cookie_from_browser.substr(0, cookie_from_browser.find("~"));
    cout << "S: retriving cookie for client " << username << endl; 
	User client(username, ip);
	string real_cookie = client.get_value("cookie");
	string status = client.get_value("status");
	cout << "S: retrieved client cookie is " << real_cookie << " with length " << real_cookie.length() << endl;
    cout << "S: cookie from browser is " << cookie_from_browser << " with length " << cookie_from_browser.length() << endl;
    cout << "S: user status is " << status << endl;

    if (real_cookie.empty()) {
      cout << "ERROR client has no cookie stored in backend\n";
      return false;
	}

	return cookie_from_browser == real_cookie && status == "online";
}

/* ========================================================== */
/* ===================== smtp server utils =================== */
/* ========================================================== */

/**
 *  check if input is supported command
 */
bool is_command(char* input, string cmd, int length) {
    char* input_ptr = input;
    while (*input_ptr == ' ') {
        input_ptr++;
    }
    for (int i = 0; i < length; i++) {
    if (tolower(*input_ptr) != cmd[i]) {
        return false;
    }
    input_ptr++;
    }
    return true;
}

/**
 * check if email address has surrounding <> and whether it's localhost
 */
bool valid_addr(string addr) {
    if (addr.length() == 0) {
        return false;
    }
    // check start and end with <>
    size_t start = addr.find("<");
    size_t end = addr.find(">");
    if (start == string::npos || end == string::npos || start > end) {
        return false;
    }

    size_t pos = addr.find("@");
    if (pos == string ::npos) {
        return false;
    }

    // check localhost
    if (strcmp(addr.substr(pos + 1, end - pos - 1).c_str(), "localhost") != 0) {
        return false;
    }

    return true;
}

/**
 * extracts username from <__@localhost> format
 */
string parse_addr(string addr) {
    size_t start = addr.find("<");
    size_t end = addr.find("@");
    return addr.substr(start + 1, end - start - 1);
}


/* ========================================================== */
/* ===================== shared utils =================== */
/* ========================================================== */

/**
 * puts email in local user's mailbox if user is local
 * returns whether user is local
 */
bool put_local_box(string username, string domain, string put_value, string backend_ip) {
    User kvClient(username, backend_ip);
    if (kvClient.get_value("pwd").length() > 0 && domain == "localhost") {
        bool index_updated = true;
        string new_max_index;
        do {
            string max_index = kvClient.get_value("max_email_index");
            if (max_index.length() == 0) {
                cout << "WARNING: user " << username << "has no max email index; creating one\n";
                kvClient.put_value("max_email_index", "0");
                max_index = "0";
            }
            new_max_index = to_string(stoi(max_index) + 1);
            //true if index successfully updated, false otherwise
            index_updated = kvClient.cput_value("max_email_index", max_index, new_max_index);
        } while (!index_updated);

        kvClient.put_value(new_max_index, put_value);
        cout << "S: put email to backend server with index " << new_max_index << endl;
        return true;
    } else {
        cout << "S: email recipient is not local\n";
        return false;
    }
}
