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
#include "jtb/jtbvec.h"
#include <fstream>


enum class HTTP_MODE { GET, POST };

enum class DEBUGMODE { OFF, ON };

template <typename T>
bool inVec(std::vector<T>& vec, T&& target);

std::string url_encode(const std::string value); 

std::string getOutputFromShellCommand(const char* cmd); 

class Curl {
    CURL* curlpointer;
    static size_t write__callback(void* contents, size_t size, size_t nmemb, void* userdata);
public: 
    Curl(): curlpointer(curl_easy_init()) {
	if (!curlpointer) { throw std::runtime_error("curl initialization failed"); }
    }

    ~Curl() { curl_easy_cleanup(curlpointer); }

    void curl_execute(JTB::Str&,
		  JTB::Str&,
		  HTTP_MODE mode = HTTP_MODE::GET,
		  JTB::Str post_command = "", 
		  size_t (*write_callback)(void* contents, size_t size, size_t nmemb, void* userdata) = write__callback);
};

struct IPs {
    IPs(Display& display, Curl& curl) { 
	try {
	    JTB::Str home { std::getenv("HOME") };
	    std::ifstream rokuDotEnv { home.concat("/.rokuip").c_str() };
	    rokuDotEnv >> roku;
	    std::ifstream denonDotEnv { home.concat("/.denonip").c_str() };
	    rokuDotEnv >> denon;
	}
	catch (std::runtime_error& e) {
	    display.flashMessage(e.what());
	}
	if (!found) {
	    int count { 0 };
	    while (++count < 11 && !found) {
		try {
		    setIPs(display, curl);
		} catch (std::runtime_error& e) {
		    JTB::Str errorMessage { e.what() };
		    if (errorMessage.lower().boolFind("timeout")) {
			display.displayMessage("Timeout reached...");
			display.displayMessage("Trying once more...");
		    }
		}
	    }
	    if (!found) {
		display.displayMessage("Too many problems...");
		exit(1);
	    }
	}
    };

    struct Found {
	Found() { roku = false; denon = false; }
	bool roku;
	bool denon;
	operator bool() { return roku && denon; }
    };

    void setIPs(Display&, Curl&); 

    JTB::Str getRoku() {
	return roku;
    };

    JTB::Str getDenon() {
	return denon;
    };

    static bool testForRoku(JTB::Str&, Curl&); 

    static bool testForDenon(JTB::Str&, Curl&);

private: 
    JTB::Str denon;
    JTB::Str roku;
    Found found;
};
