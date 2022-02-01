#ifndef SMTPCLIENT_H
#define SMTPCLIENT_H

#include <string>

class SmtpClient {
    public:
        std::string sender; //username of sender
        std::string recipient; //full recipient email
        std::string host; //network host (whatever's after @)
        std::string email;
        int sock;

        SmtpClient(std::string _sender, std::string _recipient, std::string _host, std::string _email) {
          sender = _sender;
          recipient = _recipient;
          host = _host;
          email = _email;
        }
        std::string recv_response();
        std::string dns_query();
        int send_hello();
        int send_email();
        int send_quit();
};

#endif