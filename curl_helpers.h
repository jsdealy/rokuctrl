#pragma once
#include <curl/curl.h>
#include <iostream>
#include <regex>
#include <stdexcept>
#include <string>
#include <vector>

enum class HTTP_MODE { GET, POST };

enum class DEBUGMODE { OFF, ON };

template <typename T>
bool inVec(std::vector<T>& vec, T&& target);

std::string url_encode(const std::string value); 

std::string getOutputFromShellCommand(const char* cmd); 

void flash_string(std::string s); 

size_t write__callback(void* contents, size_t size, size_t nmemb, void* userdata); 

void curl_execute(CURL *curl, 
		  std::string& readBuffer,
		  std::string& URL,
		  HTTP_MODE mode = HTTP_MODE::GET,
		  std::string post_command = "", 
		  size_t (*write_callback)(void* contents, size_t size, size_t nmemb, void* userdata) = write__callback);


bool testForRoku(CURL *curl, std::string ip); 

bool testForDenon(CURL *curl, std::string ip); 

struct IPs {
    IPs() { 
	try {
	    setIPs();
	} catch (std::runtime_error e) {
	    std::regex re { R"(timeout)", std::regex_constants::icase };
	    if (std::regex_search(std::string(e.what()), re)) {
		std::cout << "Timeout reached..." << '\n';
		std::cout << "Trying again..." << '\n';
		setIPs();
	    }
	}
    };

    struct Found {
	Found() { roku = false; denon = false; }
	bool roku;
	bool denon;
	operator bool() { return roku && denon; }
    };

    void setIPs(); 

    std::string getRoku() {
	return roku;
    };

    std::string getDenon() {
	return denon;
    };

private: 
    std::string denon;
    std::string roku;
    Found found;
};
