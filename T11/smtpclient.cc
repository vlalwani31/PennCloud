#include "smtpclient.h"

#include <arpa/inet.h>
#include <netdb.h>   //getaddrinfo
#include <string.h>  //memset
#include <sys/socket.h>
#include <unistd.h>  //close
#include <netinet/in.h> //res_query
#include <arpa/nameser.h>
#include <resolv.h>

#include <iostream>
#include <string>
#include <vector>

using namespace std;

string SmtpClient::recv_response() {
    char buf[1001];
    int bytes_read = read(sock, &buf[0], 1000);
    if (bytes_read < 0) {
        perror("ERROR SMTP client cannot read response from socket");
        return "";
    }
    buf[bytes_read] = '\0';
    string response(buf);
    cout << "S: got following response from SMTP server: " << response;
    return response;
}

string SmtpClient::dns_query() {
    u_char buf[4096];
    int mx_length = res_query(host.c_str(), C_IN, T_MX, &buf[0], 4096);
    if (mx_length < 0) {
        perror("unaable to query dns");
        return "";
    }
    ns_msg msg;
    if (ns_initparse(buf, mx_length, &msg) < 0) {
        perror("unable to parse mx buffer");
        return "";
    }
    cout << "S: smtp client successfully completed mx query\n";

    int num_records = ns_msg_count(msg, ns_s_an);
    int min_priority = INT_MAX;
    string server;
    for (int i = 0; i < num_records; i++) {
        ns_rr rr;
        char parsed_buf[4096];
        if (ns_parserr(&msg, ns_s_an, i, &rr) < 0) {
            perror("ns parser failed");
            continue;
        }
        ns_sprintrr(&msg, &rr, NULL, NULL, parsed_buf, sizeof(parsed_buf));
        cout << "S: SMTP client parsing mx query results\n";

        const u_char* rdata = ns_rr_rdata(rr);
        const uint16_t priority = ns_get16(rdata);
        char exchange[NS_MAXDNAME];
        int len = dn_expand(buf, buf + mx_length, rdata + 2, exchange, sizeof(exchange));
        if (len < 0) {
            perror("dn expand failed");
            continue;
        }
        cout << "S: SMTP client got mx record " << exchange << " with priority " << priority << endl;

        if (priority < min_priority) {
            min_priority = priority;
            server = exchange;
        }
    }

    return server;
}

/**
 * Creates socket, binds to address and sends hello message
 * Sets "sock" field of instance
 * Returns -1 on error, 0 on success
 */
int SmtpClient::send_hello() {
    cout << "S: SMTP client sending helo\n";

    string server = dns_query();
    /* === get address of remote recipient === */
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = 0;
    hints.ai_protocol = 0; 
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;

    cout << "S: getting address info for " << recipient << " on server " << server << endl;
    struct addrinfo *result, *rp;
    int ret = getaddrinfo(server.c_str(), NULL, &hints, &result);
    if (ret < 0) {
        cout << "ERROR on getaddrinfo: " << gai_strerror(ret) << endl;
        return -1;
    }
    if (result == NULL) {
        cout << "S: found no IPs for given recipient email\n";
        return -2;
    }

    cout << "S: attempting to create socket\n";
    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
      perror("ERROR SMTP client attemped to create socket but failed");
      return -1;
    }

    for (rp = result; rp != NULL; rp = rp->ai_next) {

        struct sockaddr_in dest = *(sockaddr_in*)rp->ai_addr;
        dest.sin_port = htons(25);
        cout << "S: attempting to connect to socket\n";
        if (connect(sock, (struct sockaddr*)&dest, sizeof(dest)) < 0) {
            perror("ERROR SMTP client attemped to bind to socket but failed");
            continue;
        }

        /* === receive greeting from smtp server === */
        string response = recv_response();
        if (response.find("220") != string::npos) {
            cout << "S: SMTP client got 220 greeting from server\n";
        }

        /* === send helo message to smtp server === */
        cout << "S: SMTP client attempting to send HELO\n";
        string helo = "HELO localhost\r\n";
        if (write(sock, helo.c_str(), helo.length()) < 0) {
            perror("ERROR SMTP client attempted to write to socket but failed");
            freeaddrinfo(result);
            return -3;
        }

        /* === receive response from smtp server === */
        response = recv_response();
        if (response.find("250") != string::npos) {
            cout << "S: SMTP client successfully sent hello\n";
            freeaddrinfo(result);
            return 0;
        } else {
            cout << "ERROR SMTP client cannot send hello\n";
            freeaddrinfo(result);
            return -1;
        }
    }
    freeaddrinfo(result); 
    return -1;
}

/**
 * Sends "Rcpt to", "Send from", "data" and "." commands to SMTP server
 */
int SmtpClient::send_email() {
    cout << "S: SMTP client entering transaction\n";
    vector<string> commands = {"MAIL FROM:<" + sender + "@" + host + "> \r\n",
                               "RCPT TO:<" + recipient + "> \r\n", 
                               "DATA\r\n",
                               email, 
                               "\r\n.\r\n"};

    for (int i = 0; i < commands.size(); i++) {
        /* === send command === */
        string command = commands[i];
        cout << "S: SMTP client sending command " + command;

        if (write(sock, command.c_str(), command.length()) < 0) {
          string err_msg = "ERROR SMTP client failed to send " + command;
          perror(err_msg.c_str());
          return -1;
        }

        if (i == 3) {
            //don't get response for email content
            continue;
        }
        /* === receive response === */
        string expected_code = i == 2 ? "354" : "250";
        string response = recv_response();
        if (response.find(expected_code) != string::npos) {
          cout << "S: SMTP client got success response for " + command;
        } else {
          cout << "ERROR SMTP client got error response for " + command;
          return -1;
        }
    }
    return 0;
}

/**
 * quits smpt session by sending "quit"
 */
int SmtpClient::send_quit() {
    cout << "S: SMTP client sending quit\n";
    string quit = "QUIT\r\n";
    if (write(sock, quit.c_str(), quit.length()) < 0) {
        perror("ERROR SMTP client failed to send QUIT");
    }
    if (close(sock) < 0) {
        perror("ERROR unable to close socket created for email sending");
        return -1;
    }
    return 0;
}

