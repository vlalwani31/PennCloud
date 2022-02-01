#include <set>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h> //perror
#include <iostream> //cout
#include <pthread.h>
#include <string>
#include <strings.h> //bzero
#include <unistd.h> //read, write, close
#include <vector>
#include <map>
#include "webserver.h"
#include "process_request.h"
#include "utils.h"

using namespace std;

set<int> conn_fds; //file descriptors of each client connection
set<pthread_t> conn_tids; //thread ids of threads spawned

int listen_fd = -1; //file descriptor that coordinator is listening on
unsigned short master_port = 8080; //master_port
string addr; //= "127.0.0.1:8000"; //store full address of the webserver

bool sigint = false; //whether ^C was pressed

pthread_mutex_t conn_fd_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t conn_tid_mutex = PTHREAD_MUTEX_INITIALIZER;

CoordinationClient* backend_master;

/*
 * worker for sending heartbeat
 */
void* worker2(void *arg) {
	int fd = *((int*) arg);
	cout << "addr is " << addr << endl;
	string heartbeat = "checking from ip:" + addr + "\r\n\r\n";

	cout << "heartbeat is " << heartbeat << endl;
	while (true) {
		do_write(fd, (char*)heartbeat.c_str(), heartbeat.size());
		usleep(1000000);
	}

	return NULL;
}


/*
 * spawns workder threads to handle client TCP connections
 * returns -1 on error
 */
int coordinator(unsigned short port) {
	/* =========== connect to master for sending heart beat =============== */
	int sock = 0;
	struct sockaddr_in serv_addr;
	char buffer[1024] = {0};
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		printf("\n Socket creation error \n");
		return -1;
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(master_port);

	// Convert addresses
	if(inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr)<=0)
	{
		printf("\nInvalid address/ Address not supported \n");
		return -1;
	}

	if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	{
		printf("\nConnection Failed \n");
		return -1;
	}

	pthread_t tid_heart;
	if (pthread_create(&tid_heart, NULL, worker2, &sock) < 0) {
		perror("server failed to create thread for sending heatbeat/n");
		return -1;
	}

	/* =========== open TCP stream socket =============== */
	listen_fd = socket(PF_INET, SOCK_STREAM, 0);
	if (listen_fd < 0) {
		perror("server unable to open listen socket");
		return -1;
	}

	/* == construct server address and BIND to socket === */
	struct sockaddr_in server_addr;
	bzero(&server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	inet_pton(AF_INET, "127.0.0.1", &(server_addr.sin_addr));

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
		int new_fd = accept(listen_fd, (struct sockaddr*) &client_addr,
				&client_addr_len);
		if (new_fd < 0) {
			break;
		}

		/* ========= store new file descriptor === */
		pthread_mutex_lock(&conn_fd_mutex);
		conn_fds.insert(new_fd);
		pthread_mutex_unlock(&conn_fd_mutex);

		cout << "S: [" << new_fd << "] New connection" << endl;

		/* ======== create worker thread ========== */
		pthread_t tid;
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

/**
 * handles individual TCP connections
 * returns -1 on error
 */
void* worker(void *arg) {
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

	int conn_fd = *((int*) arg);
	vector < string > copy;

	map < string, string > header_content;
	string body_content;
	bool finished_header = false;
	int body_bytes = 0;
	string request_line = "GET /";
    string cookie_va = "None";
    string domain = "None";

	do {
		/* ======== read client data from socket ======= */
		char buffer[4096];
		int bytes_read = read(conn_fd, buffer, 4095);
		if (bytes_read < 0) {
			perror("errored when reading from client. Closing connection");
			break;
		} else if (bytes_read == 0) {
			//client ^C
			break;
		}
		buffer[bytes_read] = '\0';
		cout << "\nS: [" << conn_fd << "] GOT NEW DATA \n" << endl;

		/* ======== store each line of request into vector of strings ===== */
		int prev_str_end = -1;
		for (int i = 0; i < bytes_read - 1; i++) {
			if (finished_header) {
                cout << "S: saving body: " << string(&buffer[i]) << endl;
				for (int j = i; j < bytes_read; j++) {
                    body_content.push_back(buffer[j]);
                    body_bytes += 1;
                }
				break;
			}

			if (buffer[i] == '\r' && buffer[i + 1] == '\n') {
				string req_line = char_arr_to_string(buffer, prev_str_end + 1, i + 1);

				if (prev_str_end + 1 == i) { //single CRLF line separating header and body
					finished_header = true;
					cout << "S: Finished header\n";
				} else {
					if (req_line.rfind("Cookie:", 0) == 0) { // starts with "Cookie:"
						string va_head = "id=";
						size_t position = req_line.find(va_head);

						cookie_va = req_line.substr(position+va_head.length(), req_line.length()-position-va_head.length()-2);
						cout << "S: cookie value sent from browser is: " << cookie_va << "\n";
					}

					size_t key_val_split = req_line.find(": ");
					if (key_val_split == string::npos) {
						if (header_content.size() == 0) { //first line is request-line
							request_line = req_line;
						} else {
							cout << "ERROR Unable to parse header line " << req_line << endl;
							continue;
						}
					}

					string key = req_line.substr(0, key_val_split);
					string val = req_line.substr(key_val_split + 2, req_line.length() - key_val_split - 2 - 2);

                    header_content.insert(pair<string, string>(key, val));
				}
				prev_str_end = i + 1;
				i++;
			}
		}

		domain = header_content.find("Host")->second; //get the domain
		cout << "S: current host domain is: " << domain << "\n";

		/* ======= handle request if no body or finished reading body ======= */\
		string response = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";

		if (header_content.find("Content-Length") != header_content.end()) {
			cout << "S: header indicates content length is " << stoi(header_content.find("Content-Length")->second) << endl;
            cout << "S: actually body length read is " << body_bytes << endl;
            // cout << "S: body read so far is .........\n" << body_content << "\n.........\n";
            if (stoi(header_content.find("Content-Length")->second) > body_bytes) {
                "S: server continuing to read request body\n";
                continue;
            }
        }

        vector < string > parsed_req_line = parse_request_line(request_line);
        cout << "S: Processing request: " << parsed_req_line[0] << " " << parsed_req_line[1] << endl;

        if (parsed_req_line[1].find("/email") != string::npos) {
            EmailRequest email_req(parsed_req_line[0], parsed_req_line[1], cookie_va);
            if (parsed_req_line[0] == "GET") {
                response = email_req.handle_get();
            } else if (parsed_req_line[0] == "POST") {
                if (body_content.size() == 0) {
                    cout << "ERROR Post has no data\n";
                    response = "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\n\r\n";
                } else {
                    response = email_req.handle_post(body_content);
                }
            }
        } 
        
        if (parsed_req_line[1].find("/file") != string::npos) {
            FileRequest file_req(parsed_req_line[0], parsed_req_line[1], header_content, cookie_va, domain);
            if (parsed_req_line[0] == "GET") {
                response = file_req.handle_get();
            } else if (parsed_req_line[0] == "POST") {
                if (body_content.size() == 0) {
                    cout << "ERROR Post has no data\n";
                    response = "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\n\r\n";
                } else {
                    response = file_req.handle_post(body_content);
                }
            }
        }

        if (parsed_req_line[1].find("/login") != string::npos) {
            if (cookie_va != "None") {
                cout << "S: user is already logged in\n";

                string html = "<head><meta http-equiv=\"Refresh\" content=\"0; URL=http://";
                html.append(domain);
                html.append("/main\"></head>");
                response = "HTTP/1.1 200 OK\r\n";
                response.append("Content-Type: text/html\r\n");
                response.append("Content-Length: " + to_string(html.length()) + "\r\n");
                response.append("\r\n"); //header-body separator
                response.append(html); //go to current user's main page

            } else {
                LoginReq login_req(parsed_req_line[0], parsed_req_line[1], domain);
                if (parsed_req_line[0] == "GET") {
                    response = login_req.handle_get();
                } else if (parsed_req_line[0] == "POST") {
                    if (body_content.size() == 0) {
                        cout << "ERROR Post has no data\n";
                        response = "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\n\r\n";
                    } else {
                        response = login_req.handle_post(body_content);
                    }
                }
            }
        }

        if (parsed_req_line[1].find("/register") != string::npos) {
            RegisterReq register_req(parsed_req_line[0], parsed_req_line[1]);
            if (parsed_req_line[0] == "GET") {
                response = register_req.handle_get();
            } else if (parsed_req_line[0] == "POST") {
                if (body_content.size() == 0) {
                    cout << "ERROR Post has no data\n";
                    response = "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\n\r\n";
                } else {
                    response = register_req.handle_post(body_content);
                }
            }
        }

        if (parsed_req_line[1].find("/main") != string::npos) {
            MainReq main_req(parsed_req_line[0], parsed_req_line[1], cookie_va);
            if (parsed_req_line[0] == "GET") {
                response = main_req.handle_get();
            } else if (parsed_req_line[0] == "POST") {
//					if (body_content.size() == 0) {
//						cout << "ERROR Post has no data\n";
//						response = "HTTP/1.1 500 Internal Server Error\r\n\r\n";
//					} else {
//						response = main_req.handle_post(body_content);
//					}
            }
        }

        if (parsed_req_line[1].find("/admin") != string::npos) {
            cout << "admin get now\n";
            AdminReq admin_req(parsed_req_line[0], parsed_req_line[1], backend_master, cookie_va);

            if (parsed_req_line[0] == "GET") {
                response = admin_req.handle_get();
            } else if (parsed_req_line[0] == "POST") {
            	response = admin_req.handle_post(body_content);
            }
        }

        if (parsed_req_line[1].find("/logout") != string::npos) {
            cout << "S: logging out now\n";
            cookie_va = "None";
            LogoutReq logout(parsed_req_line[0], parsed_req_line[1], cookie_va);
            if (parsed_req_line[0] == "GET") {
                response = logout.handle_get();
            }
        }

        if (parsed_req_line[1].find("/account") != string::npos) {
            AccountReq account_req(parsed_req_line[0], parsed_req_line[1]);
            if (parsed_req_line[0] == "GET") {
                response = account_req.handle_get();
            }
            if (parsed_req_line[0] == "POST") {
                response = account_req.handle_post(body_content);
            }
        }

		int bytes_written = write(conn_fd, response.c_str(), response.length());
		if (bytes_written < 0) {
			perror("cannot write response to client");
			break;
		}
		cout << "S: wrote to server " << bytes_written << " bytes" << endl;

		//reset variables
		header_content.clear();
		body_content.clear();
        body_bytes = 0;
		finished_header = false;
		request_line = "GET /";

	} while (!sigint);

	/* =========== cleanup ================== */
	if (close(conn_fd) < 0) {
		perror("cannot close socket file descriptor in worker thread");
	}

	if (!sigint) {
		pthread_mutex_lock(&conn_fd_mutex);
		conn_fds.erase(conn_fd);
		pthread_mutex_unlock(&conn_fd_mutex);

		pthread_mutex_lock(&conn_tid_mutex);
		conn_tids.erase((pthread_t) pthread_self());
		pthread_mutex_unlock(&conn_tid_mutex);
	}

	cout << "S: [" << conn_fd << "] Connection closed" << endl;

	pthread_detach((pthread_t) pthread_self());
	pthread_exit (NULL);

	return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        cout << "ERROR missing required arguments [config][server number]\n";
        return -1;
    }

    int s_id = 	s_id = stoi(argv[2]);
    unsigned short port = 8000; //default port
	fstream config_file;
	config_file.open(argv[1], std::ios::in);
	if (config_file.is_open()) {
		string line;
		int count = 0;
		while (getline(config_file, line)) {
			if (count == s_id) {
				port = stoi(line.substr(line.find(":") + 1));
                addr = "127.0.0.1:" + to_string(port);
				cout << "Configured webserver to listen on " << port << endl;
			} 
			count++;
		}
		config_file.close();
	}

	backend_master = new CoordinationClient(
		    grpc::CreateChannel("localhost:7777",
		      grpc::InsecureChannelCredentials()));

	if (coordinator(port) < 0) {
		return -1;
	}

	return 0;
}
