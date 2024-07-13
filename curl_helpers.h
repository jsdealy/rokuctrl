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
	    display.displayMessage("Got roku IP from .rokuip: " + roku.stdstr());
	    std::ifstream denonDotEnv { home.concat("/.denonip").c_str() };
	    denonDotEnv >> denon;
	    display.displayMessage("Got denon IP from .denonip: " + denon.stdstr());
	}
	catch (std::runtime_error& e) {
	    display.flashMessage("Some kind of problem with getting .env...");
	    display.flashMessage(e.what(), 1000);
	}
	if (!found()) {
	    display.flashMessage("Didn't find it in the .envs...", 1000);
	    int count { 0 };
	    while (++count < 11 && !found()) {
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
	    if (!found()) {
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

    bool found() const {
	return denonFound() && rokuFound();
    }

    bool denonFound() const {
	return denon.startsWith(ipstart);
    }

    bool rokuFound() const {
	return roku.startsWith(ipstart);
    }

    void setIPs(Display&, Curl&); 

    JTB::Str getRoku() {
	return roku;
    };

    JTB::Str getDenon() {
	return denon;
    };

    static bool testForRoku(const JTB::Str&, Curl&); 

    static bool testForDenon(const JTB::Str&, Curl&);

    constexpr static const char* const ipstart { "192.168." };

private: 
    bool testAndHandleRoku(const JTB::Str&, Curl&, Display&);
    bool testAndHandleDenon(const JTB::Str&, Curl&, Display&);

    JTB::Str denon;
    JTB::Str roku;
};
