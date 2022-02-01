#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <iostream> //cout
#include <pthread.h>
#include <set>
#include <fstream>
#include <map>
#include <strings.h>
#include <unistd.h>
#include <errno.h>
#include <vector>
#include <limits.h> //INT_MAX
#include <vector>
#include <string.h>
#include "utils.h"

using namespace std;

map<string, int> all_servers;//map node_ip--number of clients the node is servings
set<string> alive_servers;
map<string, bool> server_status;
string master;

set<int> conn_fds; //file descriptors of each client connection
set<pthread_t> conn_tids; //thread ids of threads spawned

bool sigint = false; //whether ^C was pressed

pthread_mutex_t conn_fd_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t conn_tid_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t change_servermap_mutex = PTHREAD_MUTEX_INITIALIZER;

void print_all_servers() {
	map<string, bool>::iterator it_status;

	cout << "printing all servers\n";
	for (it_status = server_status.begin(); it_status != server_status.end(); it_status++) {
		cout << it_status->first << " status: " << it_status->second << " load count: " << all_servers.find(it_status->first)->second << endl;
	}
}

bool is_alive(string addr) {
	bool status = server_status.find(addr)->second;
	return status;
}

string make_response(string http_body, vector<string> headers) {
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


void* worker(void *arg) {
	int conn_fd = *((int*) arg);
	vector <string> copy;

	map <string, string> header_content;
	string body_content;
	bool finished_header = false;
	int body_bytes = 0;
	string request_line = "GET /";
	string addr;

	do {
		//reading from conn_fd
		char buffer[4096];
		int bytes_read = read(conn_fd, buffer, 4095);
		if (bytes_read < 0) {
			perror("errored when reading from client. Closing connection");
			if (!addr.empty()) {
				perror("server closing connection");
				server_status.find(addr)->second = false;
				all_servers.find(addr)->second = 0;
			}
			break;
		} else if (bytes_read == 0) {
			//client ^C
			perror("nothing read in");
			if (!addr.empty()) {
				perror("nothing from the server, assuming dead");
				server_status.find(addr)->second = false;
				all_servers.find(addr)->second = 0;
			}
			break;
		}
		buffer[bytes_read] = '\0';
		cout << "\nFS: [" << conn_fd << "] GOT NEW REQUEST " << buffer  << "\n";

		/* ======== store each line of request into vector of strings ===== */
		if (finished_header) {
			cout << "FS: saving body: " << buffer << endl;
			body_content.append(buffer);
			body_bytes += bytes_read;
			continue;
		}

		int prev_str_end = -1;
		bool browser_mess = false;
		for (int i = 0; i < bytes_read - 1; i++) {
			if (finished_header) {
				body_content.append(&buffer[i]);
				body_bytes += (bytes_read - i);
				break;
			}
			if (buffer[i] == '\r' && buffer[i + 1] == '\n') {
				string req_line = char_arr_to_string(buffer, prev_str_end + 1, i + 1);

				if (prev_str_end + 1 == i) { //single CRLF line separating header and body
					finished_header = true;
					cout << "FS: Finished header\n";
				} else {
					cout << "FS: Parsing header line " << req_line;

					if (req_line.rfind("checking from ip:", 0) == 0) { // starts with "checking from ip:",
						//full string should be "checking from ip:real ip\r\n";
						string va_head = "checking from ip:";

						addr = req_line.substr(va_head.length(), req_line.find("\r\n")-va_head.length());
						cout << "alive server is: " << addr << "\n";

						server_status.find(addr) -> second = true;
						browser_mess = false;

						print_all_servers();
						//usleep(1000000);
					} else {
						browser_mess = true;
						size_t key_val_split = req_line.find(": ");
						if (key_val_split == string::npos) {
							if (header_content.size() == 0) { //first line is request-line
								request_line = req_line;
							} else {
								cout << "ERROR Unable to parse header line" << endl;
								continue;
							}
						}

						string key = req_line.substr(0, key_val_split);
						string val = req_line.substr(key_val_split + 2, req_line.length() - key_val_split - 3);

						if (finished_header) {
							cout << "FS: saving body: " << req_line << endl;
							body_content.append(req_line);
							body_bytes += req_line.length();
						} else {
							header_content.insert(pair<string, string>(key, val));
						}
					}
				}
				prev_str_end = i + 1;
				i++;
			}
		}

		if (browser_mess) {
			string domain;
			domain = header_content.find("Host")->second; //get the domain
			cout << "FS: current host domain is: " << domain << "\n";

			cout << "FS before assignment: " << endl;
			print_all_servers();
			map<string, bool>::iterator it_status;
			int min_count = INT_MAX;
			string min_server = "None";

			for (it_status = server_status.begin(); it_status != server_status.end(); it_status++) {
				if (it_status->second) { //alive
					int load = all_servers.find(it_status->first)->second; //this server's load
					if (load < min_count) {//update the min_server
						min_count = load;
						min_server = it_status->first;
					}
				}
			}


			string html;
			html = "<head>\n<meta http-equiv=\"Refresh\" content=\"0; URL=http://";
			html.append(min_server);
			html.append("/login/send\"></head>");

			cout << "FS body is:" << endl;
			cout << html << endl;
			string response = make_response(html, vector<string>{});
			all_servers.find(min_server)->second++;

			cout << "FS after assignment: " << endl;
			print_all_servers();
			int bytes_written = write(conn_fd, response.c_str(), response.length());
			if (bytes_written < 0) {
				perror("cannot write response to client");
				break;
			}
			cout << "FS: wrote to server " << bytes_written << " bytes" << endl;


			//reset variables
			header_content.clear();
			body_content.clear();
			finished_header = false;
			request_line = "GET /";
		}

	} while (!sigint);

	return NULL;
}


int coordinator() {
	string master_port = master.substr(master.find(":")+1);
	string master_ip = master.substr(0, master.find(":"));

	print_all_servers();
	/* =========== open TCP stream socket =============== */
	int listen_fd = socket(PF_INET, SOCK_STREAM, 0);
	if (listen_fd < 0) {
		perror("server unable to open listen socket");
		return -1;
	}

	/* == construct server address and BIND to socket === */
	struct sockaddr_in server_addr;
	bzero(&server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(stoi(master_port));
	inet_pton(AF_INET, master_ip.c_str(), &(server_addr.sin_addr));

	int optval = 1;
	setsockopt(listen_fd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));

	if (bind(listen_fd, (struct sockaddr*) &server_addr, sizeof(server_addr))
			< 0) {
		perror("server unable to bind socket port to address");
		return -1;
	}

	/* ========= listen on socket ============= */
	if (listen(listen_fd, 100) < 0) {
		perror("server unable to start listening");
		return -1;
	}

	do {
		/* ========== accept connection ======= */
		struct sockaddr_in client_addr;
		bzero(&client_addr, sizeof(client_addr));
		socklen_t client_addr_len = sizeof(client_addr);
		int new_fd = accept(listen_fd, (struct sockaddr*) &client_addr, &client_addr_len);
		if (new_fd < 0) {
			break;
		}

		/* ========= store new file descriptor === */
		pthread_mutex_lock(&conn_fd_mutex);
		conn_fds.insert(new_fd);
		pthread_mutex_unlock(&conn_fd_mutex);

		cout << "Frontend Master: [" << new_fd << "] New connection" << endl;

		/* ======== create worker thread ========== */
		pthread_t tid;

		//for listening to aliveness check
		if (pthread_create(&tid, NULL, worker, &new_fd) < 0) {
			perror("server failed to create thread for new connection");
			return -1;
		}
		pthread_mutex_lock(&conn_tid_mutex);
		conn_tids.insert(tid);
		pthread_mutex_unlock(&conn_tid_mutex);

	} while (!sigint);

	/* ========== join cancelled threads ======== */
	for (pthread_t tid : conn_tids) {
		cout << "joining thread " << tid << endl;
		if (pthread_join(tid, NULL) < 0) {
			perror("cannot join thread");
		}
	}

	return 0;
}



int main(int argc, char *argv[]) {
    if (argc < 3) {
        cout << "Frontent master error: input config filename and server id";
        return -1;
    }

	int s_id;
	fstream config_file;
	s_id = atoi(argv[2]);
	config_file.open(argv[1], std::ios::in);
	if (config_file.is_open()) {
		string line;
		// string config; // the 1st / 2nd part of the config, (i.e. either forwarding address or bind address)
		int count = 0;
		while (getline(config_file, line)) {
			if (count == s_id) {
				master = line;
				cout << "master is " << master << "\n";
			} else {
				cout << "putting into map\n";
				all_servers.insert(std::pair<string, int>(line, 0)); //put every other potential servers into the set
				server_status.insert(std::pair<string, bool>(line, false));
			}
			count++;
		}
		config_file.close();
	}
	// Assuming that input is in format of ./frontend_master filename id

	if (coordinator() < 0) {
		return -1;
	}



}
