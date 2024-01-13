/* This is an ncurses program for controlling a roku device and a denon avr 
 * using the command line <= 09/18/23 19:32:38 */ 
#include "curl_helpers.h"
#include <cstring>
#include <algorithm>
#include <cstdlib>
#include <csignal>
#include <unistd.h>
#include <iostream>
#include <curl/curl.h>
#include <curl/easy.h>
#include <ncurses.h>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <array>
#include <regex>

DEBUGMODE debugmode = DEBUGMODE::OFF;

template <typename T>
bool inVec(std::vector<T>& vec, T&& target) {
    for (T& x : vec) if (x == target) return true;
    return false;
}

std::string getOutputFromShellCommand(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}


size_t write__callback(void* contents, size_t size, size_t nmemb, void* userdata) {
    // Compute the real size of the incoming buffer
    size_t realSize = size * nmemb;

    // Convert the buffer pointer to a string pointer 
    std::string* outStr = (std::string*)userdata;
    /* note: you can't clear out the outStr before appending, 
     * since this callback will be called multiple times (potentially)
     * for a given http request; just have to be sure to send 
     * in a clean buffer with each call to curl_execute
     * <= 09/30/23 15:56:01 */ 
    outStr->append((char*)contents, realSize);

    return realSize;
}

void curl_execute(CURL *curl, 
		  std::string& readBuffer,
		  std::string& URL,
		  HTTP_MODE mode,
		  std::string post_command, 
		  size_t (*write_callback)(void* contents, size_t size, size_t nmemb, void* userdata)) {

    curl_easy_setopt(curl, CURLOPT_URL, URL.c_str());

    if (mode == HTTP_MODE::POST) {
	/* this ensures we send a post request <= 09/18/23 18:39:06 */ 
	curl_easy_setopt(curl, CURLOPT_POST, 1L);

	/* this is what sets the posted data <= 09/18/23 18:39:16 */ 
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_command.c_str());
    }     

    // Send all returned data to this function
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);

    // Pass our 'readBuffer' to the callback function
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 3L);  // timeout after 3 seconds

    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);  // connection timeout after 5 seconds

    /* TURN THIS ON FOR DEBUGGING <= 09/18/23 13:25:46 */ 
    /* curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L); */

    // Perform the request, and check for errors
    CURLcode res = curl_easy_perform(curl);

    if(res != CURLE_OK) {
	/* endwin(); */
	/* std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl; */
	throw std::runtime_error(curl_easy_strerror(res));
    } 

    std::regex re { R"([^\s])" };

    if (debugmode == DEBUGMODE::ON && std::regex_search(readBuffer, re)) 
	printw("%s\n", readBuffer.c_str());
}


bool testForRoku(CURL *curl, std::string ip) {
    std::string buffer = "";
    try {
	curl_execute(curl, buffer, std::string("http://").append(ip.append(":8060/query/media-player")));
    } catch (std::runtime_error e) {
	if (std::string(e.what()).find("Couldn't connect") == std::string::npos) throw e;
    }
    
    if (buffer.find("player") != std::string::npos) 
	return true;
    return false;
}

bool testForDenon(CURL *curl, std::string ip) {
    std::string buffer = "";
    try {
	curl_execute(curl, buffer, std::string("http://").append(ip.append("/MainZone/index.html")));
    } catch (std::runtime_error e) {
	if (std::string(e.what()).find("Couldn't connect") == std::string::npos) throw e;
    }

    if (buffer.find("Volume") != std::string::npos) 
	return true;
    return false;
}


void IPs::setIPs(Display& display) {
    display.displayMessage("Getting ips...");
    std::string pingOutput { getOutputFromShellCommand("timeout 7 ping -b 192.168.1.255 2> /dev/null") };
    pingOutput.append(getOutputFromShellCommand("nmap -sP 192.168.1.0/24 2> /dev/null"));
    display.displayMessage("Regexing...");
    std::regex re { R"(192\.168\.1\.[\d]{1,3})" };
    std::vector<std::string> ips;
    /* this works since regex_search returns true each time it finds a hit <= 09/25/23 23:44:40 */ 
    for (std::smatch sm; std::regex_search(pingOutput, sm, re);) {
	if (!inVec(ips, std::string(sm[0]))) ips.push_back(sm[0]);
	pingOutput = sm.suffix();
    }
    display.displayMessage("Testing ips for roku & denon responses...");
    std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> curl(curl_easy_init(), curl_easy_cleanup);
    for (int i = 0; !found && i < ips.size(); i++) {
	if (!found.roku) {
	    try {
		found.roku = testForRoku(curl.get(), ips.at(i));
	    } catch (std::runtime_error e) {
		display.displayMessage("Roku Test Error: ");
		display.displayMessage(e.what());
	    }  
	    if (found.roku) { 
		roku = ips.at(i); 
		display.displayMessage("Roku found.");
		/* if (debugmode == DEBUGMODE::ON) { printw("%s\n", roku.c_str()); } */
	    }
	} 
	if (!found.denon) {
	    try {
		found.denon = testForDenon(curl.get(), ips.at(i));
	    } catch (std::runtime_error e) {
		display.displayMessage("Denon Test Error: ");
		display.displayMessage(e.what());
	    }  
	    if (found.denon) { 
		denon = ips.at(i); 
		display.displayMessage("Denon found.");
		/* if (debugmode == DEBUGMODE::ON) { printw("%s\n", denon.c_str()); } */
	    }
	} 
    }
    display.displayMessage("Done testing.");
    display.clearMessages(3000);
    if (!found.roku) throw std::runtime_error("Couldn't find the Roku.");
    if (!found.denon) throw std::runtime_error("Couldn't find the AVR.");
}
