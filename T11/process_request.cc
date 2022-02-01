#include "process_request.h"
#include "smtpclient.h"
#include "user.h"
#include "utils.h"

#include <fcntl.h>  //open
#include <stdio.h>
#include <unistd.h>  //read
#include <stdlib.h>
#include <sys/time.h>

#include <iostream>  //cout
#include <map>
#include <string>
#include <vector>
#include <set>
#include <tuple>
#include <functional>
#include <boost/functional/hash.hpp>
#include "user.h"

using namespace std;

//CoordinationClient kvClient2(
//    grpc::CreateChannel("localhost:7777",
//      grpc::InsecureChannelCredentials()));


//CoordinationClient backend_master(
//	    grpc::CreateChannel("localhost:7777",
//	      grpc::InsecureChannelCredentials()));


/* =========================================== */
/* ========== global variables =============== */
/* =========================================== */

string backend_master = "localhost:7777";

vector<string> paths_with_cookies = {
    // "; path=/main",          "; path=/email/list",    "; path=/file/upload",
    // "; path=/logout",        "; path=/login/send",    "; path=/admin",
    // "; path=/file/list",     "; path=/email/delete",  "; path=/email/reply",
    // "; path=/email/send",    "; path=/email/forward", "; path=/file/delete",
    // "; path=/file/download", "; path=/file/view", "; path=/file/createdir",
    // "; path =/file/move", "; path=/file/all", "path=/email/view",
    "; path=/main",  "; path=/email",  "; path=/file",
    "; path=/login", "; path=/logout", "; path=/admin"};

string http_bad_request = "HTTP/1.1 400 Bad Request\r\nContent-length: 0\r\n\r\n";  
string http_not_found = "HTTP/1.1 404 Not Found\r\nContent-length: 0\r\n\r\n";
string http_server_error = "HTTP/1.1 500 Internal Server Error\r\nContent-length: 0\r\n\r\n";

/* =========================================== */
/* ========== generic request functions ====== */
/* =========================================== */

/**
 * reads html form from filepath and constructs http response to be sent
 */
string Request::send_form(string filepath) {
	int html_fd = open(filepath.c_str(), O_RDONLY);
	if (html_fd < 0) {
		perror("ERROR failed to open html file");
		return "HTTP/1.1 500 Internal Server Error\r\nContent-length: 0\r\n\r\n";;
	}
	string http_body = "";

	/* ====== read in html from file ========== */
	char buffer[4096];
	int bytes_read = read(html_fd, buffer, 4095);
	if (bytes_read < 0) {
		perror("ERROR cannot read html file");
		return "HTTP/1.1 500 Internal Server Error\r\n\r\n";;
	}
	while (bytes_read != 0) {
		//null-terminate buffer
		buffer[bytes_read] = '\0';
		http_body.append(buffer);
		bytes_read = read(html_fd, buffer, 4095);
	}

	if (close(html_fd) < 0) {
		perror("cannot close html file");
	}

	/* ===== construct http response ========= */
	string response = "HTTP/1.1 200 OK\r\n";
	response.append("Content-Type: text/html\r\n");
	response.append("Content-Length: " + to_string(http_body.length()) + "\r\n");
	response.append("\r\n"); //header-body separator
	response.append(http_body);
	response.append("\r\n");

	return response;
}

/**
 * reads html contents and return the string of it
 */
string Request::read_html(string filepath) {
	int html_fd = open(filepath.c_str(), O_RDONLY);
	if (html_fd < 0) {
		perror("ERROR failed to open html file");
		return "HTTP/1.1 500 Internal Server Error\r\nContent-length: 0\r\n\r\n";;
	}
	string http_body = "";

	/* ====== read in html from file ========== */
	char buffer[4096];
	int bytes_read = read(html_fd, buffer, 4095);
	if (bytes_read < 0) {
		perror("ERROR cannot read html file");
		return "HTTP/1.1 500 Internal Server Error\r\nContent-length: 0\r\n\r\n";;
	}
	while (bytes_read != 0) {
		//null-terminate buffer
		buffer[bytes_read] = '\0';
		http_body.append(buffer);
		bytes_read = read(html_fd, buffer, 4095);
	}

	if (close(html_fd) < 0) {
		perror("cannot close html file");
	}

	return http_body;
}

string Request::make_response(string http_body, vector<string> headers) {
	string response = "HTTP/1.1 200 OK\r\n";
	response.append("Content-Type: text/html\r\n");
	response.append("Content-Length: " + to_string(http_body.length()) + "\r\n");
	for (string header : headers) {
		response.append(header);
	}
	response.append("\r\n"); //header-body separator
	response.append(http_body);
	response.append("\r\n");

	return response;
}

/* =========================================== */
/* ============= EMAIL REQUEST =============== */
/* =========================================== */
/**
 * handles HTTP GET request
 * returns HTTP response
 */
string EmailRequest::handle_get() {
	cout << "S: Running email's get handler\n";
    /* === check if user is logged in === */
    if (!check_cookie(cookie, backend_master)) {
        return send_form("../../html/Unauthorized.html");
    }
    string user = cookie.substr(0, cookie.find("~"));
    cout << "S: serving email get request for user " << user << endl;
    User kvClient(user, backend_master);

    /* === return html form or error message === */
    if (uri == "/email/send") {
      cout << "S: Sending email form\n";
      return send_form("../../html/email_form.html");
	} 

    if (uri == "/email/list") {
        string max_email_index = kvClient.get_value("max_email_index");
        if (max_email_index.length() == 0) {
            cout << "ERROR server failed to get max email index\n";
            return "HTTP/1.1 500 Internal Server Error\r\n\r\n";
        }

        string html = "<html><body style=\"white-space: pre-line\"><h1>Inbox</h1><a href=\"/email/send\">New Email</a><br><hr>";
        for (int i = 1; i <= stoi(max_email_index); i++) {
            string email = kvClient.get_value(to_string(i));
            if (email.length() > 0) {
                cout << "S: got email " << i << "from backend\n";
                size_t time_start = email.find("Time: ");
                size_t time_end = email.find("\n", time_start) + 1;

                //display preview of email and list them as hrefs so user can click on it and get full content
                html.append("<p>" +  email.substr(0, time_end) + "</p>");
                html.append("<a href=\"/email/view/" + to_string(i) + "\"> See full mail </a><br>");
                html.append("<div><a href=\"/email/delete/" + to_string(i) + "\">Delete</a></div>");
                html.append("<div><a href=\"/email/reply/" + to_string(i) + "\">Reply</a></div>");
                html.append("<div><a href=\"/email/forward/" + to_string(i) + "\">Forward</a></div><hr>");
                html.append("<br>");
            }
        }
        html.append("<a href=\"/main\">Back Home</a></body></html>");
        return make_response(html, vector<string>{});
	} 

    if (uri.find("/email/view/") != string::npos) {
        string email_index = parse_uri("/email/view/", uri);
        string full_mail = kvClient.get_value(email_index);

        string html = "<html><body style=\"white-space: pre-line\"><a href=\"/email/list\">Back to Inbox</a><br>";
        if (full_mail.length() > 0) {
            html.append("<p>" + full_mail + "</p>");
        } else {
            html.append("<p> No such email </p>");
        }
        return make_response(html, vector<string>{});
    }

    if (uri.find("/email/delete") != string::npos) {
		/* === get email index from uri === */
		string prefix = "/email/delete/";
		size_t index_start = uri.find(prefix) + prefix.length();
		string index = uri.substr(index_start);
		cout << "S: index parsed from email delete uri is " << index << endl;

		/* === delete email from backend === */
		kvClient.delete_value(index);
		cout << "S: made delete request to backend for email " << index << endl;

		/* === construct response === */
		string html = "<html><body><h1>Okay! Email deleted!</h1><a href=\"/email/list\">Back to Inbox</a></body></html></body></html>";
		vector<string> headers;
		return make_response(html, headers);

	} 
    
    if (uri.find("/email/reply") != string::npos || uri.find("/email/forward") != string::npos) {
		bool is_reply = uri.find("/email/reply") != string::npos;
		/* === get email index from uri === */
		string prefix = is_reply ? "/email/reply/" : "/email/forward/";
		size_t index_start = uri.find(prefix) + prefix.length();
		string index = uri.substr(index_start);
		cout << "S: index parsed from email reply or forward uri is " << index << endl;

		/* === get email from backend === */
		string email_content = kvClient.get_value(index);

		/* === parse out original sender and recipient === */
		size_t sender_start = email_content.find("From: ") + 6;
		size_t sender_end = email_content.find("\n", sender_start);
		string sender = email_content.substr(sender_start, sender_end - sender_start);

		size_t recip_start = email_content.find("To: ") + 4;
		size_t recip_end = email_content.find("\n", recip_start);
		string recip = email_content.substr(recip_start, recip_end - recip_start);

		/* === construct email reply form === */
		string html = "<html><body style=\"white-space: pre-line\">";
		if (is_reply) {
			html.append("<form action=\"/email/reply\" method=\"post\" accept-charset=utf-8>");
			html.append("<input type=\"text\" id=\"reply-to\" name=\"reply-to\" value=\"" + sender + "\"><br>");
			html.append("<input type=\"text\" id=\"reply-from\" name=\"reply-from\" value=\"" + recip + "\"><br>");
			html.append("<label for=\"email-reply\">Reply: </label><br>");
			html.append("<textarea id=\"email-reply\" name=\"email-reply\" rows=25 cols=50></textarea><br>");
			html.append("<textarea id=\"orig-content\" name=\"orig-content\" rows=25 cols=50>" + email_content + "</textarea><br>");
			html.append("<input type=\"submit\" value=\"Submit\"></form></body></html>");
		} else {
			html.append(read_html("../../html/email_form.html"));
			html.append("<textarea id=\"orig-content\" name=\"orig-content\" form=\"new-mail-form\" rows=25 cols=50>" + email_content + "</textarea><br>");
		}
		
		vector<string> headers;
		return make_response(html, headers);
	}

	return http_not_found;
}

/**
 * handles HTTP POST request with input post_data in query params format
 * returns HTTP response
 */
string EmailRequest::handle_post(string post_data) {
	cout << "S: called email's post\n";
     /* === check if user is logged in === */
    if (!check_cookie(cookie, backend_master)) {
        return send_form("../../html/Unauthorized.html");
    }
    string sender_uname = cookie.substr(0, cookie.find("~"));
    cout << "S: serving email post request for user " << sender_uname << endl;
    User kvClient(sender_uname, backend_master);

	bool is_reply = uri.find("/email/reply") != string::npos;
	bool is_forward = uri.find("/email/forward") != string::npos;

	map <string, string> query_params = parse_query_params(post_data);
	for (map<string, string>::iterator it = query_params.begin();
			it != query_params.end(); it++) {
		cout << "Key: " << it->first << endl;
		cout << "Val: " << it->second << endl;
	}

    /* === get recipient's username === */
	map<string, string>::iterator it = is_reply ? query_params.find("reply-to") : query_params.find("email-recipient");
	map<string, string> recipients = parse_recipients(it->second);
	
	/* === get sender === */
	//TODO: get sender from cookies
	string sender = is_reply ? query_params.find("reply-from")->second : sender_uname + "@localhost";

	/* === construct email === */  
	string put_value = "From: " + sender + "\n";
	put_value.append("To: " + it->second + "\n");
	put_value.append("Time: " + get_time() + "\n");

	if (is_reply) {
		put_value.append(query_params.find("email-reply")->second + "\n");
		put_value.append("--------------------------- Original Email ---------------------\n");
		put_value.append(query_params.find("orig-content")->second + "\n");
	} else {
		put_value.append("Subject: " + query_params.find("email-subject")->second + "\n");
		put_value.append(query_params.find("email-content")->second);
		if (is_forward) {
			put_value.append("--------------------------- Forwarded email Email ---------------------\n");
			put_value.append(query_params.find("orig-content")->second + "\n");
		}		
	}

	for (map<string, string>::iterator it = recipients.begin(); it != recipients.end(); it++) {
		string recipient = it->first + "@" + it->second;
		string username = it->first;
		string domain = it->second;
		
		//TODO: query master node for IP of backend server that contains data
		if (!put_local_box(username, domain, put_value, backend_master)) {
            /* === if local user, save email to their mailbox on backend server; else send with smtp client ===*/
			SmtpClient client(sender_uname, recipient, domain, put_value);
			int err = client.send_hello();
			if (err == -1 || err == -3) {
				return "HTTP/1.1 500 Internal Server Error\r\nContent-length: 0\r\n\r\n";
			} else if (err == -2) {
				return "HTTP/1.1 400 Bad Request (invalid recipient address)\r\nContent-length: 0\r\n\r\n";
			}		

			err = client.send_email();
			if (err < 0) {
				return "HTTP/1.1 500 Internal Server Error\r\nContent-length: 0\r\n\r\n";
			}
			err = client.send_quit();
			if (err < 0) {
				return "HTTP/1.1 500 Internal Server Error\r\nContent-length: 0\r\n\r\n";
			}
			break;
		}
	}

	string html = "<html><body><h1>Email sent!</h1><a href=\"/email/list\">Back to Inbox</a></body></html></body></html>";
	vector<string> headers;

	return make_response(html, headers);	
}

/* =========================================== */
/* ============== FILE REQUEST =============== */
/* =========================================== */
string FileRequest::handle_get() {
	cout << "S: Running file's get handler\n";
     /* === check if user is logged in === */
    if (!check_cookie(cookie, backend_master)) {
        return send_form("../../html/Unauthorized.html");
    }
    string user = cookie.substr(0, cookie.find("~"));
    cout << "S: serving file get request for user " << user << endl;
    User kvClient(user, backend_master);

    /* === SEE ALL FILES === */
    if (uri.find("/file/all") != string::npos) {
        map<string, string> files = get_all_files(backend_master, user, "/", "");
        string html = "<html><body style=\"white-space: pre-line\"><h1>All Files</h1><hr>";
        for (map<string, string>::iterator it = files.begin(); it != files.end(); it++) {
            html.append("<a href=\"/file/download/" + it->first + "\">" + it->second + "</a><br>");
        }
        html.append("<a href=\"/main\">Back home</a></body></html>");
        return make_response(html, vector<string>{});
    }

    /* === VIEW FOLDER CONTENTS === */
    if (uri.find("/file/list") != string::npos) {
        /* === get directory absolute path; default root directory === */
        string this_dir_id = parse_uri("file/list/", uri);
        if (this_dir_id.length() == 0) {
            this_dir_id = "/";
        }
        
		/* === get all files and folders in current directory === */
		string content = kvClient.get_value("D:" + this_dir_id + ":content");
        if (content.length() == 0) {
            return http_not_found;
        }
        string this_dir_name = kvClient.get_value("D:" + this_dir_id + ":name");
        if (this_dir_name.length() == 0) {
            return http_not_found;
        }

        vector<vector<string>> dir_contents = parse_dir_contents(content);
        vector<string> directories = dir_contents[0], files = dir_contents[1];  

        /* === Get all directories that files in this directory can move to === */
        map<string, string> all_dirs = get_mv_options(backend_master, "/", user, "", "");
        string file_option_list = "<label for=\"dest\">Move to</label><select name=\"dest\" id=\"dest\">";
        for (map<string, string>::iterator it = all_dirs.begin(); it != all_dirs.end(); it++) {
            cout << "S: appending to option list directory " << it->second << endl;
            file_option_list.append("<option value=\"" + it->first + "\">" + it->second + "</option>");
        }
        file_option_list.append("</select>");

        /* === Construct folder view: all folders/files are clickable links; clicking on file gives file content ===*/
        string html = "<html><body style=\"white-space: pre-line\">";
        html.append("<h1>" + this_dir_name + "</h1>");
        html.append("<h2>Folders: </h2><br>");
        html.append(read_html("../../html/folder_create.html"));

        for (string dir_id : directories) {
            string dirname = kvClient.get_value("D:" + dir_id + ":name");
            html.append("<a href=\"/file/list/" + dir_id + "\">" + dirname + "</a><br>");
            html.append("<a href=\"/file/delete/directory/" + dir_id + "\"> Delete " + dirname + "</a><br>");
            // folder rename form
            html.append("<form action=\"/file/rename/directory/" + dir_id);
            html.append(read_html("../../html/rename.html"));

            /* === Get all directories that this folder can move to === */
            map<string, string> option_dirs = get_mv_options(backend_master, "/", user, "", dir_id);
            string dir_option_list = "<br><label for=\"dest\">Move to</label><select name=\"dest\" id=\"dest\">";
            for (map<string, string>::iterator it = option_dirs.begin(); it != option_dirs.end(); it++) {
                cout << "S: appending to option list directory " << it->second << endl;
                dir_option_list.append("<option value=\"" + it->first + "\">" + it->second + "</option>");
            }
            dir_option_list.append("</select>");

            // folder move drop down
            html.append("<form action=\"/file/move/directory/" + dir_id + "\" method=\"post\">");
            html.append(dir_option_list);
            html.append("<input type=\"submit\" value=\"Move\"></form><hr>");
        }

        html.append("<br><h2>Files: </h2><br>");
        html.append(read_html("../../html/file_upload.html"));
        html.append("<hr>");

        for (string file_id : files) {
            string filename = kvClient.get_value("F:" + file_id + ":name");
            html.append("<a href=\"/file/download/" + file_id + "\">" + filename + "</a><br>");
            html.append("<a href=\"/file/delete/file/" + file_id + "\"> Delete " + filename + "</a><br><br>");
            // file rename form
            html.append("<form action=\"/file/rename/file/" + file_id);
            html.append(read_html("../../html/rename.html"));
            // file rename drop down
            html.append("<form action=\"/file/move/file/" + file_id + "\" method=\"post\">");
            html.append(file_option_list);
            html.append("<input type=\"submit\" value=\"Move\"></form><hr>");
        }

        // append link to parent directory if it's not root
        if (this_dir_id != "/") {
            string parent_id = kvClient.get_value("D:" + this_dir_id + ":parent");
            parent_id = parent_id.substr(parent_id.find(":") + 1);
            if (parent_id == "/") {
                parent_id = "";
            }
            html.append("<a href=\"/file/list/" + parent_id + "\"> Back to previous </a><br>");
        }
        html.append("<a href=\"/main\"> Back to main page </a><br>");
        html.append("</body></html>");

        vector<string> headers;
        return make_response(html, headers);

	}

    /* === DOWNLOAD FILE === */
    if (uri.find("/file/download") != string::npos) {
        string file_id = parse_uri("/file/download/", uri);
        /* === get filename and file content from ID === */
        string filename = kvClient.get_value("F:" + file_id + ":name");
        cout << "S: filename found from file ID is " << filename << endl;
        string file_content = kvClient.get_value("F:" + file_id + ":content");
        if (filename.length() == 0 || file_content.length() == 0) {
            return http_not_found;
        }
        cout << "S: retrieved file content is .......\n" << file_content << "\n.........\n";

        /* === parse file content and get content type === */
        size_t content_type_start = file_content.find("Content-Type: ");
        size_t content_type_end = file_content.find("\r\n", content_type_start);
        string content_type = file_content.substr(content_type_start, content_type_end - content_type_start);
        cout << "S: downloading file with content type " << content_type << endl;
        file_content = file_content.substr(content_type_end + 4); //skip two \r\n after content-type line

        vector<string> headers{
            "Content-Disposition: attachment; filename=" + filename + "\r\n",
            content_type + "\r\n"};
        return make_response(file_content, headers);
	} 

    /* === DELETE FILE OR FOLDER === */
    if (uri.find("/file/delete") != string::npos) {
        //can't delete root directory
        if (uri == "/file/delete/file" || uri == "/file/delete/file/" ||
            uri == "/file/delete/directory" || uri == "/file/delete/directory/") {
                return http_bad_request;
        }

        /* === get file id from uri === */
        bool is_file = uri.find("/file/delete/file/") != string::npos;
        string to_delete_id = is_file ? parse_uri("/file/delete/file/", uri)
                                      : parse_uri("/file/delete/directory/", uri);
        string col_key_prefix = is_file ? "F:" + to_delete_id : "D:" + to_delete_id;
        cout << "S: item to delete is " << col_key_prefix << endl;

        //  Step 1: delete mapping from parent directory to file
        // get parent directory
        string parent_dir = kvClient.get_value(col_key_prefix + ":parent");
        if (parent_dir.length() == 0) {
            return "HTTP/1.1 404 Resource to delete not found\r\nContent-length: 0\r\n\r\n";
        }
        cout << "S: parent directory of file to delete is " << parent_dir << endl;
        
        bool cput_success = false;
        do {
            string parent_contents = kvClient.get_value(parent_dir + ":content");
            string new_contents = remove_from_parent(parent_contents, col_key_prefix);
            cput_success = kvClient.cput_value(parent_dir + ":content", parent_contents, new_contents);
        } while (!cput_success);
        cout << "S: removed from parent directory listing\n";

        // Step 2: delete item
        if (is_file) {
            kvClient.delete_value(col_key_prefix + ":parent");
            kvClient.delete_value(col_key_prefix + ":name");
            kvClient.delete_value(col_key_prefix + ":content");
            cout << "S: successfully deleted file\n";
        } else {
            recursive_delete(backend_master, user, col_key_prefix);
            cout << "S: successfully recursively deleted folder and its contents\n";
        }
        string html = "<html><body><h1>Deleted!</h1></body></html>";
        vector<string> headers;
        return make_response(html, headers);
    }

    return http_not_found;
}

string FileRequest::handle_post(string post_data) {
    cout << "S: Running file's post handler\n";
    map<string, string> query_params = parse_query_params(post_data);

    /* === check if user is logged in === */
    if (!check_cookie(cookie, backend_master)) {
        return send_form("../../html/Unauthorized.html");
    }
    string user = cookie.substr(0, cookie.find("~"));
    cout << "S: serving file post request for user " << user << endl;
    User kvClient(user, backend_master);

    /* === get parent directory === */
    string referer = headers.find("Referer")->second;
    cout << "S: file post request came from " << referer << endl;
    string parent_id = parse_uri("/file/list/", referer);
    if (parent_id.length() == 0) {
        parent_id = "/";
    }

    /* === MOVE FILE OR FOLDER === */
    if (uri.find("/file/move") != string::npos) {
        bool is_dir = uri.find("/file/move/directory/") != string::npos;
        bool is_file = uri.find("/file/move/file/") != string::npos;
        if (!is_file && !is_dir) {
            return http_not_found;
        }

        string col_key_prefix =
            is_dir ? "D:" + parse_uri("/file/move/directory/", uri)
                   : "F:" + parse_uri("/file/move/file/", uri);
        cout << "S: col key prefix is of item to move is " << col_key_prefix << endl;

        string orig_parent_prefix = kvClient.get_value(col_key_prefix + ":parent");
        string new_parent_id = query_params.find("dest")->second;

        // Step 1: remove this file from the original parent's content listing
        bool cput_success = false;
        do {
            string orig_parent_content = kvClient.get_value(orig_parent_prefix + ":content");
            string new_parent_content = remove_from_parent(orig_parent_content, col_key_prefix);
            cput_success = kvClient.cput_value(orig_parent_prefix + ":content", orig_parent_content, new_parent_content);
        } while (!cput_success);

        // Step 2: change this file's parent
        cput_success = kvClient.cput_value(col_key_prefix + ":parent", orig_parent_prefix, "D:" + new_parent_id);
        if (!cput_success) {
            cout << "S WARNING: attempting to move " << col_key_prefix << " but its parent directory has changed\n";
            return http_bad_request;
        }

        // Step 3: add this file to new parent's content
        cput_success = false;
        do {
            string new_parent_content = kvClient.get_value("D:" + new_parent_id + ":content");
            string updated_parent_content =
                new_parent_content == " "
                    ? col_key_prefix
                    : new_parent_content + "," + col_key_prefix;
            cput_success = kvClient.cput_value("D:" + new_parent_id + ":content", new_parent_content, updated_parent_content);
        } while (!cput_success);

        cout << "S: successfully moved " << col_key_prefix << " to " << new_parent_id << endl;
    }

    /* === RENAME FILE OR FOLDER === */
     else if (uri.find("/file/rename/") != string::npos) {
        bool is_dir = uri.find("/file/rename/directory/") != string::npos;
        bool is_file = uri.find("/file/rename/file/") != string::npos;
        if (!is_file && !is_dir) {
            return http_not_found;
        }

        string col_key_prefix =
            is_dir ? "D:" + parse_uri("/file/rename/directory/", uri)
                   : "F:" + parse_uri("/file/rename/file/", uri);

        /* === store newname back === */
        string newname = query_params.find("newname")->second;
        bool cput_success = false;
        do {
            string currname = kvClient.get_value(col_key_prefix + ":name");
            cout << "S: item to rename is currently named " << currname << endl;
            cput_success = kvClient.cput_value(col_key_prefix + ":name", currname, newname);
        } while (!cput_success);
	} else {
        string cput_prefix, cput_name, cput_content;

        if (uri == "/file/upload") {
            /* === get file boundary & filename & file type === */
            string boundary_header = headers.find("Content-Type")->second;
            string boundary = boundary_header.substr(boundary_header.find("boundary=") + 9);

            size_t filename_start = post_data.find("filename=\"") + 10;
            size_t filename_end = post_data.find("\"", filename_start);
            string filename = post_data.substr(filename_start, filename_end - filename_start);
            string file_id = get_file_hash(filename, parent_id);
            cout << "S: got filename " << filename << " of length " << filename.length() << endl;

            /* === parse file === */
            // actual start boundary: --[boundary in header]\r\n
            size_t start_boundary = post_data.find(boundary) + boundary.length() + 2; 
            size_t end_boundary = post_data.rfind(boundary);
            // actual end boundary: --[boundary in header]--\r\n
            // subtract \r\n above end boundary and two additional --
            string file_content = post_data.substr(start_boundary, end_boundary - start_boundary - 2 - 2);

            cput_prefix = "F:" + file_id;
            cput_name = filename;
            cput_content = file_content;

        } else if (uri == "/file/createdir") {
            /* === get directory name === */
            string dirname = query_params.find("dirname")->second;
            cout << "S: got dirname " << dirname << " of length " << dirname.length() << endl;
            string dir_id = get_file_hash(dirname, parent_id);

            cput_prefix = "D:" + dir_id;
            cput_name = dirname;
            cput_content = " ";
        }

        /* === store file/dir to backend === */
        if (!kvClient.put_value(cput_prefix + ":content", cput_content)) {
            return http_server_error;
        }
        if (!kvClient.put_value(cput_prefix + ":name", cput_name)) {
            return http_server_error;
        }
        if (!kvClient.put_value(cput_prefix + ":parent", "D:" + parent_id)) {
            return http_server_error;
        }

        bool cput_success = false;
        do {
            string parent_content = kvClient.get_value("D:" + parent_id + ":content");
            string new_parent_content = parent_content == " " ? cput_prefix : parent_content + "," + cput_prefix;
            cput_success = kvClient.cput_value("D:" + parent_id + ":content", parent_content, new_parent_content);
        } while (!cput_success);

        cout << "S: stored file to backend, redirecting to parent file list\n";
    }

    string html = "<head><meta http-equiv=\"Refresh\" content=\"0; URL=" + referer + "\"></head>";
    return make_response(html, vector<string>{});
}

/* =========================================== */
/* ============= LOGIN REQUEST =============== */
/* =========================================== */
string LoginReq::handle_get() {
	if (uri == "/login/send") {
		cout << "S: Sending login form\n";
		/* === return login form or error message === */
		return send_form("../../html/login.html");
	}

    return http_not_found;
}

string LoginReq::handle_post(string post_data) {
	cout << "S: called login's post\n";
	string uname, psw;

	map<string, string> query_params = parse_query_params(post_data);
	for (map<string, string>::iterator it = query_params.begin(); it != query_params.end(); it++) {
		cout << "Key: " << it->first << endl;
		cout << "Val: " << it->second << endl;
		string key = it->first;
		string val = it->second;

		if (key == "psw") {
			psw = val;
		} else if (key == "uname") {
			uname = val;
		}
	}

	/* === check if user exists === */
	User client(uname, backend_master);
	string stored_password = client.get_value("pwd");

    /* === no such user === */
	if (stored_password.empty()) {
		string login_form = read_html("../../html/login.html");

		string add_p = "<label for=\"uname\"><b>Username</b></label>";
		size_t p = login_form.find(add_p);
		string before = login_form.substr(0, p);
		string after = login_form.substr(p);
		before.append("<p style=\"color:red\"> no such user </p>");
		before.append(after);
		cout << "S: error page is \n" << before << endl;

		string response = "HTTP/1.1 200 OK\r\n";
		return make_response(before, vector<string>{});
	}

    /* === if password matches === */
    if (stored_password == psw) {
        string created_cookie = create_cookie(uname, psw);
        string cookie_header = "Set-Cookie: id=" + created_cookie;

        vector<string> cookie_headers;
        for (string path : paths_with_cookies) {
            cookie_headers.push_back(cookie_header + path + "\r\n");
        }

        client.cput_value("status", "off", "online");

        if (client.get_value("cookie").empty()) {
            if (!client.put_value("cookie", created_cookie)) {
                return http_server_error;
            }
        } else {
            cout << "S: replace cookie\n";
            if (!client.delete_value("cookie")) {
                return http_server_error;
            }
            if (!client.put_value("cookie", created_cookie)) {
                return http_server_error;
            }
        }

        string html = "<head><meta http-equiv=\"Refresh\" content=\"0; URL=http://";
        html.append(Domain);
        html.append("/main\"></head>");
        string response = "HTTP/1.1 200 OK\r\n";
        response.append("Content-Type: text/html\r\n");
        response.append("Content-Length: " + to_string(html.length()) + "\r\n");
        for (string c_header : cookie_headers) {
            response.append(c_header);
        }
        response.append("\r\n"); //header-body separator
        response.append(html);
        return response;
    }

    /* === if password doesn't match === */
    string login_form = read_html("../../html/login.html");

    string add_p = "<label for=\"uname\"><b>Username</b></label>";
    size_t p = login_form.find(add_p);
    string before = login_form.substr(0, p);
    string after = login_form.substr(p);
    before.append("<p style=\"color:red\"> password incorrect, please try again </p>");
    before.append(after);
    cout << "S: generated html for password non-match in login post req handler is \n" << before << endl;

    vector<string> headers;
    string html = make_response(before, headers);
    return html;
}


/* =========================================== */
/* ============= REGISTER REQUEST ============ */
/* =========================================== */
string RegisterReq::handle_get() {
	if (uri == "/register/send") {
		cout << "S: Sending Register form\n";
		/* === return login form or error message === */
		return send_form("../../html/register.html");
	}

    return "HTTP/1.1 500 Internal Server Error\r\nContent-length: 0\r\n\r\n";
}


string RegisterReq::handle_post(string post_data) {
	cout << "S: called Register's post\n";
	string uname, psw;

	map<string, string> query_params = parse_query_params(post_data);
	for (map<string, string>::iterator it = query_params.begin(); it != query_params.end(); it++) {
		cout << "Key: " << it->first << endl;
		cout << "Val: " << it->second << endl;
		string key = it->first;
		string val = it->second;

		if (key == "psw") {
			psw = val;
		} else if (key == "uname") {
			uname = val;
		}
	}

	//get password of the uname from backend to check if such username exists already
	User client(uname, backend_master);
	string stored_password = client.get_value("pwd");

    /* === no such user === */
	if (stored_password.empty()) {
		cout << "S: creating new account\n";
		if (!client.put_value("pwd", psw)) {
            return http_server_error;
        }
		if (!client.put_value("D:/:content", " ")) {
            return http_server_error;
        }
        if (!client.put_value("D:/:name", "~")) {
            return http_server_error;
        }
		if (!client.put_value("max_email_index", "0")) {
            return http_server_error;
        }
		if (!client.put_value( "status", "off")) {
            return http_server_error;
        }

		string html = "<html><body><h1>user created</h1></body></html>\r\n";
		html.append("<a href=\"/login/send\">Click here to log in</a>");
		return make_response(html, vector<string>{});
	}

	/* === user exists === */
    string login_form = read_html("../../html/register.html");

    string add_p = "<label for=\"uname\"><b>Username</b></label>";
    size_t p = login_form.find(add_p);
    string before = login_form.substr(0, p);
    string after = login_form.substr(p);
    before.append("<p style=\"color:red\"> user already exists </p>");
    before.append(after);
    fprintf(stderr, "error page is: \n%s\n", before.c_str());

    return make_response(before, vector<string>{});
}

/* =========================================== */
/* ============= ACCOUNT REQUEST ============= */
/* =========================================== */
string AccountReq::handle_get() {
	if (uri == "/account") {
		cout << "S: Sending account change form\n";
		/* === return login form or error message === */
		return send_form("../../html/account.html");
	}

    return "HTTP/1.1 500 Internal Server Error\r\nContent-length: 0\r\n\r\n";
}


string AccountReq::handle_post(string post_data) {
	cout << "S: called Register's post\n";
	string uname, opsw, npsw;

	map<string, string> query_params = parse_query_params(post_data);
	for (map<string, string>::iterator it = query_params.begin(); it != query_params.end(); it++) {
		cout << "Key: " << it->first << endl;
		cout << "Val: " << it->second << endl;
		string key = it->first;
		string val = it->second;

		if (key == "opsw") {
			opsw = val;
		} else if (key == "npsw") {
			npsw = val;
		} else if (key == "uname") {
			uname = val;
		}
	}

	//get password of the uname from backend to check if such username exists already
	User client(uname, backend_master);
	string stored_password = client.get_value("pwd");

	if (stored_password.empty()) { //no such user, error
		string login_form = read_html("../../html/account.html");

		string add_p = "<label for=\"uname\"><b>Username</b></label>";
		size_t p = login_form.find(add_p);
		string before = login_form.substr(0, p);
		string after = login_form.substr(p);
		before.append("<p style=\"color:red\"> user doesn't exist </p>");
		before.append(after);
		fprintf(stderr, "error page is: \n%s\n", before.c_str());

		return make_response(before, vector<string>{});
	}

    /* === correct password === */
    if (opsw == stored_password) {
        cout << "S: changing account\n";
        client.cput_value("pwd", stored_password, npsw);
        string html = "<html><body><h1>Password changed</h1></body></html>\r\n";
        html.append("<a href=\"/login/send\">Click here to log in</a>");

        return make_response(html, vector<string>{});
    }

    /* === wrong password -- login again === */
    string login_form = read_html("../../html/account.html");

    string add_p = "<label for=\"uname\"><b>Username</b></label>";
    size_t p = login_form.find(add_p);
    string before = login_form.substr(0, p);
    string after = login_form.substr(p);
    before.append("<p style=\"color:red\"> passwords don't match </p>");
    before.append(after);
    fprintf(stderr, "error page is: \n%s\n", before.c_str());

    vector<string> headers;
    string html = make_response(before, headers);
    return html;
}

/* =========================================== */
/* ============== MAIN REQUEST =============== */
/* =========================================== */
string MainReq::handle_get() {
	if (uri == "/main") {
		cout << "S: Sending main page\n";
		/* === return main page or error message === */
		if (check_cookie(Cookie, backend_master)) { //cookie matched
			return send_form("../../html/main.html");
		}

		return send_form("../../html/Unauthorized.html");
	}

	return http_not_found;
}

string MainReq::handle_post(string post_data) {
	return http_not_found;
}

/* =========================================== */
/* ============= ADMIN REQUEST =============== */
/* =========================================== */
string AdminReq::handle_get() {
	if (uri == "/admin") {
		cout << "S: Sending admin page\n";
		/* === return admin page or error message === */

		string admin_page = read_html("../../html/admin.html");
//		cout << "original admin page:" <<endl;
//		cout << admin_page <<endl;
//		cout << "##########\n";

		//get all backend nodes
		//for each backend

		string ip = "localhost:50051"; //should derive from backend_master


		if (check_cookie(Cookie, backend_master)) { //cookie matched
			string active_nodes;
			string all_nodes;

			Backend_master-> GetActiveServers(active_nodes, all_nodes);
			//string all_nodes = "dummy";

			cout << "All server string: " << all_nodes << "\n";

			//vector<string>
			vector<string>  active_nodes_vec = get_server_vec(active_nodes);
			vector<string>  all_nodes_vec = get_server_vec(all_nodes);

			map<string, bool> node_status;
			for (string s : all_nodes_vec) {
				node_status.insert(pair<string, bool>(s, false));
			}

			for (string s : active_nodes_vec) {
				node_status.find(s)->second=true;
			}

			//get all nodes

//			<tr>
//				<td>1</td>
//				<td>alive</td>
//				<td><form action = "" method="post"><button name="test" value = "127.0.0.1:ninini">switch</button></form></td>
//			  </tr>

			//vector<KValueStoreClient> client_vec;

			vector<std::shared_ptr<KValueStoreClient>> client_vec;
			for (string active_s : active_nodes_vec) {
				//KValueStoreClient c = create_RPC_client(active_s);
				client_vec.push_back(std::make_shared<KValueStoreClient>(grpc::CreateChannel(active_s,
						grpc::InsecureChannelCredentials())));
				//client_vec.push_back(c);
			}


			string nodes = "\n\t\t<tr>\n\t\t<th>Node</th>\n\t\t<th>status</th>\n\t\t<th></th>\n\t\t<th></th>\n\t\t</tr>\n\t\t";
			map<string, bool>::iterator it;
			for (it = node_status.begin(); it != node_status.end(); it++) {
				string node_addr = it -> first;
				string status;
				if(it -> second) {
					status = "on";
				} else {
					status = "off";
				}
				nodes.append("<tr>\n\t\t");
				nodes.append("<td>");
				nodes.append(node_addr);
				nodes.append("</td>\n\t");
				nodes.append("<td>");
				nodes.append(status);
				nodes.append("</td>\n\t\t");
				nodes.append("<td><form action = \"\" method=\"post\"><button name=\"BackendServer\" value = \"");
				nodes.append(node_addr); //button
				nodes.append("\">switch</button></form></td>\n\t\t");
				nodes.append("<tr>\n\t\t");
			}

//			cout << "checking nodes for append:" << endl;
//			cout << nodes << endl;
//			cout << "finish checking nodes" << endl;
//			cout << "\n";

			string nodes_block = "<table class=\"center\">";
			size_t nodes_p = admin_page.find(nodes_block);

			string before_nodes = admin_page.substr(0, nodes_p+nodes_block.length());
			string after_nodes = admin_page.substr(nodes_p+nodes_block.length());

			before_nodes.append(nodes);
			before_nodes.append(after_nodes);

			admin_page = before_nodes;

//			<tr>
//				<td>1</td>
//				<td>alive</td>
//				<td><button type="submit">switch</button></td>
//			</tr>


			//get all data
			//GetDataId(string)
			cout << "getting all data\n";
			int counter = 0;
			bool ending = false;
			string all_data="";
			all_data = "\n\t\t<tr>\n\t\t<th style=\"width:40%\">Key</th>\n\t\t<th style=\"width:60%\">Value</th>\n\t\t</tr>\n\t\t";

			set<tuple<string, string, string>> data_set; //set of tuple of <row, col, val>

			for (int in = 0; in < client_vec.size(); in++) {
				ending = false;
				while (!ending) {
					for (int i = counter; i < (counter+1)*20; i++) {
						string full_get;
						cout << "i:" << i << endl;
						if (client_vec[in]->GetDataId(to_string(i), full_get)) {
							cout << "enter here" <<endl;
							if (full_get.find("-ERR")==string::npos) {//we get values, not end of reading data
								cout << "parse full_get:" << endl;
								cout << full_get << endl;
								string r = "row:";
								string c = " col:";
								string v = " val:";

								size_t r_p = full_get.find(r);
								size_t c_p = full_get.find(c);
								size_t v_p = full_get.find(v);

								string row = full_get.substr(r_p + r.length(), c_p - r_p - r.length());
								string col = full_get.substr(c_p + c.length(), v_p - c_p - c.length());
								string val = full_get.substr(v_p + v.length());

								tuple<string, string, string> tu = make_tuple(row, col, val);
								data_set.insert(tu);

								cout << "r,c,v: \n";
								cout << row << endl;
								cout << col << endl;
								cout << val << endl;
								cout << "-----\n";

	//							<tr>
	//								<td>(2134141412)</td>
	//								<td>alive</td>
	//							  </tr>

//								all_data.append("<tr>\n\t\t");
//								all_data.append("<td>(");
//								all_data.append(row);
//								all_data.append(", ");
//								all_data.append(col);
//								all_data.append(")</td>\n\t\t");
//								all_data.append("<td>");
//								all_data.append(val);
//								all_data.append("</td>\n\t\t");
//								all_data.append("</tr>\n\t\t");


							} else { //end of reading
								ending = true;
								counter = 0;
								break;
							}
						}
					}

					if (ending) {
						break;
					}
					counter++;

				}
			}

			for (tuple<string, string, string> t : data_set) {
				string row = get<0>(t);
				string col = get<1>(t);
				string val = get<2>(t);

				row = get_formatted_admin_contents(row);
				col = get_formatted_admin_contents(col);
				val = get_formatted_admin_contents(val);

				all_data.append("<tr>\n\t\t");
				all_data.append("<td>(");
				all_data.append(row);
				all_data.append(", ");
				all_data.append(col);
				all_data.append(")</td>\n\t\t");
				all_data.append("<td>");
				all_data.append(val);
				all_data.append("</td>\n\t\t");
				all_data.append("</tr>\n\t\t");
			}


			//after reading all data, append into html
			string data_block = "<table id=\"dataTable\" style=\"width:90%\">";
			size_t data_p = admin_page.find(data_block);

			string before_data = admin_page.substr(0, data_p+data_block.length());
			string after_data = admin_page.substr(data_p+data_block.length());

			before_data.append(all_data);
			before_data.append(after_data);

//			cout << "newest page:" <<endl;
//			cout << before_data << endl;


			string new_admin = before_data;
			//return new_admin;

			vector<string> headers;
			string html = make_response(new_admin, headers);

//			bool comp = admin_page == send_form("../../html/admin.html");
//			cout <<  "comparison ---" << endl;
//			cout << comp <<endl;

			return html;
			//return send_form("../../html/admin.html");
		} else {
			return send_form("../../html/Unauthorized.html");
		}
	}

	return http_not_found;
}

string AdminReq::handle_post(string post_data) {
	//SwitchServer(std::string msg)
	map <string, string> query_params = parse_query_params(post_data);

	string server = query_params.find("BackendServer")->second;

	KValueStoreClient client = create_RPC_client(server);
	client.SwitchServer("switch");

	string html = "<html><body><h1>You have switched the status of one backend server</h1></body></html>\r\n";
	html.append("<a href=\"/admin\">Click here to refresh admin</a>");
	return make_response(html, vector<string>{});

	//return http_not_found;
}

/* =========================================== */
/* ============= LOGOUT REQUEST ============== */
/* =========================================== */
string LogoutReq::handle_get() {
	if (uri == "/logout") {
		cout << "S: Sending admin page\n";
		/* === return login form or error message === */

		string username = Cookie.substr(0, Cookie.find("~"));
		cout << "S: cookie when log out is: " << Cookie << "\n";
		User client(username, backend_master);
		client.cput_value("status", "online", "off");

        vector<string> cookies;
        for (string path : paths_with_cookies) {
            string cookie = "Set-Cookie: id=";
            cookie.append(Cookie);
            cookies.push_back(cookie + path + ";Max-Age=0\r\n");
        }

        string html = "<head>\n";
        html.append(
            "<meta http-equiv=\"Refresh\" content=\"0; "
            "URL=http://localhost:8000/login/send\">");  // domain needs to
                                                         // change
        html.append("</head>");

        cout << "log out request: ----" << endl;
        cout << make_response(html, cookies) << endl;
        cout << "!!!!!" << endl;

        // string delete_cookie();
        return make_response(html, cookies);
	}

	return http_not_found;
}

string LogoutReq::handle_post(string post_data) {
	return http_not_found;
}
