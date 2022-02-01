#ifndef PROCESS_REQUEST_H
#define PROCESS_REQUEST_H

#include "key_value_store_client.h"

#include <string>
#include <vector>

class Request {
 public:
  std::string method;
  std::string uri;

  // constructor
  Request(std::string input_m, std::string input_uri) {
    method = input_m;
    uri = input_uri;
  }
  std::string send_form(std::string filepath);
  std::string read_html(std::string filepath);
  std::string make_response(std::string http_body,
                            std::vector<std::string> headers);
  virtual std::string handle_get() = 0;
  virtual std::string handle_post(std::string post_data) = 0;
};

std::vector<std::string> parse_request_line(std::string reqline);

class EmailRequest : public Request {
public:
	std::string cookie;
	EmailRequest(std::string input_m, std::string input_uri, std::string _cookie) : Request(input_m, input_uri) {
		cookie = _cookie;
	}
	std::string handle_get();
	std::string handle_post(std::string post_data);

};

class LoginReq : public Request
{
public:
	std::string Domain;

	LoginReq(std::string method, std::string uri, std::string domain) : Request(method, uri) {
		Domain = domain;
	}

	std::string handle_get();
	std::string handle_post(std::string post_data);
};


class FileRequest : public Request {
public:
    std::string cookie;
    std::map<std::string, std::string> headers;
    std::string domain;

    FileRequest(std::string input_m, std::string input_uri, std::map<std::string, 
        std::string> _headers, std::string _cookie, std::string _domain) : Request(input_m, input_uri) {
        headers = _headers;
        cookie = _cookie;
        domain = _domain;
    }
    std::string handle_get();
	std::string handle_post(std::string post_data);
};

class RegisterReq : public Request
{
public:
	std::string password;

	RegisterReq(std::string method, std::string uri) : Request(method, uri) {

	}

	std::string handle_get();
	std::string handle_post(std::string post_data);
};

class MainReq : public Request
{
public:
	std::string Cookie;

	MainReq(std::string method, std::string uri, std::string cookie) : Request(method, uri) {
		Cookie = cookie;
	}

	std::string handle_get();
	std::string handle_post(std::string post_data);
};

class AdminReq : public Request
{
public:
	CoordinationClient* Backend_master;
	string Cookie;

	AdminReq(std::string method, std::string uri, CoordinationClient* backend_master, string cookie) : Request(method, uri) {
		Backend_master = backend_master;
		Cookie = cookie;
	}

	std::string handle_get();
	std::string handle_post(std::string post_data);
};

class LogoutReq : public Request
{
public:
	std::string Cookie;

	LogoutReq(std::string method, std::string uri, std::string cookie) : Request(method, uri) {
		Cookie = cookie;
	}

	std::string handle_get();
	std::string handle_post(std::string post_data);
};

class AccountReq : public Request
{
public:
	std::string password;

	AccountReq(std::string method, std::string uri) : Request(method, uri) {

	}

	std::string handle_get();
	std::string handle_post(std::string post_data);
};




#endif
