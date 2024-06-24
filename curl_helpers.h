#pragma once
#include <curl/curl.h>
#include <curses.h>
#include <iostream>
#include <regex>
#include <stdexcept>
#include <string>
#include <vector>
#include "./display.h"
#include "jtb/jtbstr.h"


enum class HTTP_MODE { GET, POST };

enum class DEBUGMODE { OFF, ON };

template <typename T>
bool inVec(std::vector<T>& vec, T&& target);

std::string url_encode(const std::string value); 

std::string getOutputFromShellCommand(const char* cmd); 

size_t write__callback(void* contents, size_t size, size_t nmemb, void* userdata); 

void curl_execute(CURL *curl, 
		  JTB::Str& readBuffer,
		  JTB::Str& URL,
		  HTTP_MODE mode = HTTP_MODE::GET,
		  JTB::Str post_command = "", 
		  size_t (*write_callback)(void* contents, size_t size, size_t nmemb, void* userdata) = write__callback);


bool testForRoku(CURL *curl, JTB::Str& ip); 

bool testForDenon(CURL *curl, JTB::Str& ip); 

struct IPs {
    IPs(Display& display) { 
	try {
	    setIPs(display);
	} catch (std::runtime_error e) {
	    std::regex re { R"(timeout)", std::regex_constants::icase };
	    if (std::regex_search(std::string(e.what()), re)) {
		display.displayMessage("Timeout reached...");
		display.displayMessage("Trying again...");
		setIPs(display);
	    }
	}
    };

    struct Found {
	Found() { roku = false; denon = false; }
	bool roku;
	bool denon;
	operator bool() { return roku && denon; }
    };

    void setIPs(Display&); 

    const JTB::Str& getRoku() {
	return roku;
    };

    const JTB::Str& getDenon() {
	return denon;
    };

private: 
    JTB::Str denon;
    JTB::Str roku;
    Found found;
};
