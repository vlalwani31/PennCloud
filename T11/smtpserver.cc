#include <arpa/inet.h>
#include <ctype.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "user.h"
#include "utils.h"

using namespace std;

pthread_mutex_t conn_fd_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t thread_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t map_mutex = PTHREAD_MUTEX_INITIALIZER;
set<int> connections;
set<pthread_t> tids;
int listen_fd;
bool verbose = false;
bool sigint_caught = false;
string backend_master = "localhost:7777";

void sig_handler(int signo) {
  if (signo == SIGINT) {
    bool cancel_self = false;
    sigint_caught = true;
    pthread_mutex_lock(&conn_fd_mutex);
    for (int fd : connections) {
      char msg[] = "-ERR Server shutting down\r\n";
      int bytes = write(fd, msg, strlen(msg));
      if (bytes < 0) {
        perror("cannot write shut down message to client");
        exit(EXIT_FAILURE);
      }
      if (close(fd) < 0) {
        perror("server can't close client connection on ^C");
        exit(EXIT_FAILURE);
      }
      if (verbose) {
        fprintf(stderr, "closing fd %d\n", fd);
      }
    }
    connections.clear();
    pthread_mutex_unlock(&conn_fd_mutex);

    pthread_mutex_lock(&thread_mutex);
    for (pthread_t tid : tids) {
      if (verbose) {
        fprintf(stderr, "canceling thread %ld\n", tid);
      }

      // if child catches sig, don't call cancel on itself to keep signal
      // handler running
      if (tid == pthread_self()) {
        cancel_self = true;
      } else {
        if (pthread_cancel(tid) < 0) {
          perror("server cannot cancel thread");
        }
      }
    }
    pthread_mutex_unlock(&thread_mutex);

    close(listen_fd);

    // if child didn't cancel itself previously, do so now
    if (cancel_self) {
      pthread_cancel((pthread_t)pthread_self());
    }
  }
}

void* worker(void* arg) {
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
    int conn_fd = *((int*)arg);
    bool quit = false;
    char partial_cmd[1100];
    int partial_cmd_ind = 0;
    bool got_helo = false;
    string from = "";
    vector<string> to;
    bool sending = false;
    string mail = "";

    do {
        // read data from socket
        char buffer[1100];
        int bytes_read = read(conn_fd, buffer, 1100);
        if (bytes_read < 0) {
            perror("errored when reading from client. Closing connection");
            break;
        } else if (bytes_read == 0) {
            //^C
            break;
        }

        // one or more commands in buffer, process each one
        for (int i = 0; i < bytes_read; i++) {
            partial_cmd[partial_cmd_ind] = buffer[i];
            partial_cmd_ind++;

            if (buffer[i] == '\n' && i > 0 && buffer[i - 1] == '\r') {
                // append null character to do string operations
                partial_cmd[partial_cmd_ind] = '\0';

                if (verbose) {
                    fprintf(stderr, "[%d] C: %s", conn_fd, partial_cmd);
                }

                // check which command and run
                string text;
                if (!sending && is_command(partial_cmd, "helo", 4)) {
                    got_helo = true;
                    text = "250 localhost\r\n";
                } else if (!sending && is_command(partial_cmd, "mail from:", 10)) {
                    if (!got_helo) {
                        text = "503 Bad sequence of commands, HELO expected\r\n";
                    } else {
                        if (from.length() > 0) {
                            text = "503 Bad sequence of commands\r\n";
                        }
                        string raw_from(&partial_cmd[10]);
                        if (!valid_addr(raw_from)) {
                            text = "550 Requested action not taken: sender is not valid\r\n";
                        } else {
                            from = parse_addr(raw_from);
                            text = "250 OK\r\n";
                        }
                    }
                } else if (!sending && is_command(partial_cmd, "quit", 4)) {
                    if (!got_helo) {
                        text = "503 Bad sequence of commands, HELO expected\r\n";
                    } else {
                        text = "221 localhost Service closing transmission channel\r\n";
                        quit = true;
                    }
                } else if (!sending && is_command(partial_cmd, "rcpt to:", 8)) {
                    if (!got_helo) {
                        text = "503 Bad sequence of commands, HELO expected\r\n";
                    } else if (from.length() == 0) {
                        text = "503 Bad sequence of commands\r\n";
                    } else {
                        string raw_to(&partial_cmd[8]);
                        if (!valid_addr(raw_to)) {
                            text = "550 Requested action not taken: recipient invalid\r\n";
                        } else {
                            string username = parse_addr(raw_to);
                            to.push_back(username);
                            User kvClient(username, backend_master);
                            if (kvClient.get_value("pwd").length() == 0) {
                                text = "550 Requested action not taken: recipient doesn't exist\r\n";
                            } else {
                                text = "250 OK\r\n";  
                            } 
                        }
                    }
                } else if (!sending && is_command(partial_cmd, "data", 4)) {
                    if (!got_helo) {
                        text = "503 Bad sequence of commands, HELO expected\r\n";
                    } else if (to.size() == 0) {
                        text = "503 Bad sequence of commands, TO unspecified\r\n";
                    } else {
                        sending = true;
                        text = "354 Start mail input; end with <CRLF>.<CRLF>\r\n";
                        char time_str[100];
                        time_t t = time(NULL);
                        struct tm* tmp = localtime(&t);
                        if (tmp == NULL) {
                            perror("localtime");
                            quit = true;
                        }
                        if (strftime(time_str, 100, "%c", tmp) == 0) {
                            perror("failed to parse current time");
                            quit = true;
                        }
                        string msg = "From <" + from + "@localhost> " + time_str + "\r\n";
                        mail.append(msg);
                    }
                } else if (sending && partial_cmd[0] == '.') {
                    for (string username : to) {
                        if (!put_local_box(username, "localhost", mail, backend_master)) {
                            cout << "ERROR SMTP server failed to put mail in " << username << "'s mailbox";
                        }
                    }
                    sending = false;
                    from.clear();
                    mail.clear();
                    to.clear();
                    text = "250 OK\r\n";
                } else if (!sending && is_command(partial_cmd, "rset", 4)) {
                    if (!got_helo) {
                        text = "503 Bad sequence of commands, HELO expected\r\n";
                    } else {
                        text = "250 OK\r\n";
                        from = "";
                        to.clear();
                    }
                } else if (!sending && is_command(partial_cmd, "noop", 4)) {
                    if (!got_helo) {
                        text = "503 Bad sequence of commands, HELO expected\r\n";
                    } else {
                        text = "250 OK\r\n";
                    }
                } else {
                    if (sending) {
                        string data(&partial_cmd[0]);
                        mail.append(data);
                    } else {
                        text = "502 Command not implemented\r\n";
                    }
                }

                int bytes_written = write(conn_fd, text.c_str(), text.length());
                if (bytes_written < 0) {
                    perror("cannot write message to client");
                    quit = true;
                }

                if (verbose && !sending) {
                    fprintf(stderr, "[%d] S: %s", conn_fd, text.c_str());
                }

                if (quit) {
                    break;
                }

                // reset cmd buffer
                partial_cmd_ind = 0;
            }
        }
    } while (!sigint_caught && !quit);

    // close socket connection
    if (close(conn_fd) < 0) {
    perror("cannot close socket file descriptor in worker thread");
    }

    if (!sigint_caught) {
        pthread_mutex_lock(&conn_fd_mutex);
        connections.erase(conn_fd);
        pthread_mutex_unlock(&conn_fd_mutex);

        pthread_mutex_lock(&thread_mutex);
        tids.erase((pthread_t)pthread_self());
        pthread_mutex_unlock(&thread_mutex);
    }

    if (verbose) {
    fprintf(stderr, "[%d] Connection closed\n", conn_fd);
    }

    pthread_detach((pthread_t)pthread_self());
    pthread_exit(NULL);
}

int main(int argc, char* argv[]) {
    // register signal handler
    if (signal(SIGINT, sig_handler) == SIG_ERR) {
        perror("can't register signal handler");
        exit(EXIT_FAILURE);
    }

    // initialize variables
    uint16_t port = 2500;
    int c;
    while ((c = getopt(argc, argv, "avp:")) != -1) {
        if (c == 'p') {
            port = atoi(optarg);
        } else if (c == 'v') {
            verbose = true;
        } else if (c == '?') {
            perror("getopt failed: ");
            exit(1);
        } else {
            perror("invalid argument\n");
            exit(1);
        }
    }

    // open SOCKET
    listen_fd = socket(PF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("server unable to open listen socket");
        exit(1);
    }

    // construct server address and BIND to socket
    struct sockaddr_in server_addr;
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &(server_addr.sin_addr));
    if (bind(listen_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("server unable to bind socket port to address");
        exit(1);
    }

    // start listening
    if (listen(listen_fd, 100) < 0) {
        perror("server unable to start listening");
        exit(1);
    }

    do {
        // accept connection
        struct sockaddr_in client_addr;
        bzero(&client_addr, sizeof(client_addr));
        socklen_t client_addr_len = sizeof(client_addr);
        int new_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &client_addr_len);
        if (new_fd < 0) {
            break;
        }

        // add file descriptor to vector
        pthread_mutex_lock(&conn_fd_mutex);
        connections.insert(new_fd);
        pthread_mutex_unlock(&conn_fd_mutex);

        if (verbose) {
            fprintf(stderr, "[%d] New connection\n", new_fd);
        }

        // send greeting
        char const* greeting = "220 localhost Server ready \r\n";
        int bytes_written = write(new_fd, greeting, strlen(greeting));
        if (bytes_written < 0) {
            perror("cannot write greeting to client");
            exit(EXIT_FAILURE);
        }

        // create thread
        pthread_t tid;
        if (pthread_create(&tid, NULL, worker, &new_fd) < 0) {
            perror("server failed to create thread for new connection");
            exit(1);
        }

        // add tid to set
        pthread_mutex_lock(&thread_mutex);
        tids.insert(tid);
        pthread_mutex_unlock(&thread_mutex);

    } while (!sigint_caught);

    // join cancelled threads
    for (pthread_t tid : tids) {
        if (pthread_join(tid, NULL) < 0) {
            if (verbose) {
                fprintf(stderr, "joining thread %ld\n", tid);
            }
            perror("cannot join thread");
        }
    }

    if (!sigint_caught && close(listen_fd) < 0) {
        perror("unable to close socket file descriptor");
        exit(1);
    }

    return 0;
}
