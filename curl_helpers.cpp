module;
/* This is an ncurses program for controlling a roku device and a denon avr 
 * using the command line <= 09/18/23 19:32:38 */ 
#include <cstring>
#include <algorithm>
#include <cstdlib>
#include <csignal>
#include <unistd.h>
#include <iostream>
#include <curl/curl.h>
#include <curl/easy.h>
#include <ncurses.h>
#include <boost/regex.hpp>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <array>
#include <regex>

export module curl_helpers;

export enum class HTTP_MODE { GET, POST };


export template <typename T>
bool inVec(std::vector<T>& vec, T&& target) {
    for (T& x : vec) if (x == target) return true;
    return false;
}

export std::string getOutputFromShellCommand(const char* cmd) {
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

export void flash_string(std::string s) {
    printw("%s", s.c_str());
    refresh();
    napms(1500);
    move(17,0);
    clrtobot();
    refresh();
}

size_t write__callback(void* contents, size_t size, size_t nmemb, void* userdata) {
    // Compute the real size of the incoming buffer
    size_t realSize = size * nmemb;

    // Convert the buffer to a string and append it to the output string
    std::string* outStr = (std::string*)userdata;
    outStr->append((char*)contents, realSize);

    return realSize;
}

export void curl_execute(CURL *curl, 
		  std::string& readBuffer,
		  std::string& URL,
		  HTTP_MODE mode = HTTP_MODE::GET,
		  std::string post_command = "", 
		  size_t (*write_callback)(void* contents, size_t size, size_t nmemb, void* userdata) = write__callback) {

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

    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);  // connection timeout after 5 seconds

    /* TURN THIS ON FOR DEBUGGING <= 09/18/23 13:25:46 */ 
    /* curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L); */

    // Perform the request, and check for errors
    CURLcode res = curl_easy_perform(curl);

    if(res != CURLE_OK) {
	/* endwin(); */
	/* std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl; */
	throw std::runtime_error(curl_easy_strerror(res));
    } 

    boost::regex re { R"([^\s])" };

    if (boost::regex_search(readBuffer, re)) 
	flash_string(readBuffer);
}


export bool testForRoku(CURL *curl, std::string ip) {
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

export bool testForDenon(CURL *curl, std::string ip) {
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

export struct IPs {
    IPs() { setIPs(); };

    struct Found {
	Found() { roku = false; denon = false; }
	bool roku;
	bool denon;
	operator bool() { return roku && denon; }
    };

    void setIPs() {
	std::string pingOutput { getOutputFromShellCommand("timeout 7 ping -b 192.168.1.255") };
	pingOutput.append(getOutputFromShellCommand("nmap -sP 192.168.1.0/24"));
	std::regex re { R"(192\.168\.1\.[\d]{1,3})" };
	std::vector<std::string> ips;
	/* this works since regex_search returns true each time it finds a hit <= 09/25/23 23:44:40 */ 
	for (std::smatch sm; std::regex_search(pingOutput, sm, re);) {
	    if (!inVec(ips, std::string(sm[0]))) ips.push_back(sm[0]);
	    pingOutput = sm.suffix();
	}
	std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> curl(curl_easy_init(), curl_easy_cleanup);
	for (int i = 0; !found && i < ips.size(); i++) {
	    if (!found.roku && testForRoku(curl.get(), ips.at(i))) { roku = ips.at(i); found.roku = true; }
	    else if (!found.denon && testForDenon(curl.get(), ips.at(i))) { denon = ips.at(i); found.denon = true; }
	}
    }

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

int main(void) {
}