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
#include <fstream>



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


size_t Curl::write__callback(void* contents, size_t size, size_t nmemb, void* userdata) {
    // Compute the real size of the incoming buffer
    size_t realSize = size * nmemb;

    // Convert the buffer pointer to a string pointer 
    JTB::Str* outStr = (JTB::Str*)userdata;
    /* note: you can't clear out the outStr before appending, 
     * since this callback will be called multiple times (potentially)
     * for a given http request; just have to be sure to send 
     * in a clean buffer with each call to curl_execute
     * <= 09/30/23 15:56:01 */ 
    /* outStr->append((char*)contents, realSize); */
    outStr->push((char *)contents, realSize);

    return realSize;
}

void Curl::curl_execute(JTB::Str& readBuffer,
		  JTB::Str& URL,
		  HTTP_MODE mode,
		  JTB::Str post_command, 
		  size_t (*write_callback)(void* contents, size_t size, size_t nmemb, void* userdata)) {

    curl_easy_setopt(curlpointer, CURLOPT_URL, URL.c_str());

    if (mode == HTTP_MODE::POST) {
	/* this ensures we send a post request <= 09/18/23 18:39:06 */ 
	curl_easy_setopt(curlpointer, CURLOPT_POST, 1L);

	/* this is what sets the posted data <= 09/18/23 18:39:16 */ 
	curl_easy_setopt(curlpointer, CURLOPT_POSTFIELDS, post_command.c_str());
    }     

    // Send all returned data to this function
    curl_easy_setopt(curlpointer, CURLOPT_WRITEFUNCTION, write_callback);

    // Pass our 'readBuffer' to the callback function
    curl_easy_setopt(curlpointer, CURLOPT_WRITEDATA, &readBuffer);

    curl_easy_setopt(curlpointer, CURLOPT_TIMEOUT, 3L);  // timeout after 3 seconds

    curl_easy_setopt(curlpointer, CURLOPT_CONNECTTIMEOUT, 10L);  // connection timeout after 5 seconds

    /* TURN THIS ON FOR DEBUGGING <= 09/18/23 13:25:46 */ 
    /* curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L); */

    // Perform the request, and check for errors
    CURLcode res = curl_easy_perform(curlpointer);

    if(res != CURLE_OK) {
	/* endwin(); */
	/* std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl; */
	throw std::runtime_error(curl_easy_strerror(res));
    } 

    std::regex re { R"([^\s])" };

    if (debugmode == DEBUGMODE::ON && std::regex_search(readBuffer.stdstr(), re)) 
	printw("%s\n", readBuffer.c_str());
}


bool IPs::testForRoku(JTB::Str& ip, Curl& curl) {
    JTB::Str buffer {};
    try {
	JTB::Str rokuQuery { "http://" + ip + ":8060/query/media-player" };
	curl.curl_execute(buffer, rokuQuery);
    } catch (std::runtime_error& e) {
	if (std::string(e.what()).find("Couldn't connect") == std::string::npos) throw e;
    }
    
    if (buffer.boolFind("player")) 
	return true;
    return false;
}

bool IPs::testForDenon(JTB::Str& ip, Curl& curl) {
    JTB::Str buffer {};
    try {
	JTB::Str denonTest { "http://" + ip + "/MainZone/index.html" };
	curl.curl_execute(buffer, denonTest);
    } catch (std::runtime_error& e) {
	if (std::string(e.what()).find("Couldn't connect") == std::string::npos) throw e;
    }

    if (buffer.boolFind("Volume")) 
	return true;
    return false;
}


void IPs::setIPs(Display& display, Curl& curl) {
    roku = "";
    denon = "";
    display.displayMessage("Getting ips...");
    JTB::Str pingOutput { getOutputFromShellCommand("timeout 7 ping -b 192.168.1.255 2> /dev/null") };
    pingOutput.push(getOutputFromShellCommand("nmap -sP 192.168.1.0/24 2> /dev/null"));
    display.displayMessage("Extracting ips from ping and nmap output...");
    pingOutput = pingOutput.filter([](const char c) -> bool {
	JTB::Str charsInIpAddresses { "123456789." };
	return charsInIpAddresses.boolFind(c);
    });
    JTB::Vec<JTB::Str> ips;
    std::size_t size = pingOutput.size();
    for (std::size_t i { pingOutput.find("192") }; i < size && i != JTB::Str::NPOS; ) {
	std::size_t endOfIP = pingOutput.find("192", i+1);
	endOfIP = endOfIP == JTB::Str::NPOS ? pingOutput.size() : endOfIP;
	ips.push(pingOutput.slice(i,endOfIP));
	i = endOfIP;
    }
    ips = ips.filter([](JTB::Str s) {
	return s.startsWith(IPs::ipstart);
    });
    ips = ips.map<JTB::Str>([](JTB::Str s) -> JTB::Str {
	return s.slice(0,13);
    });
    ips = ips.filter([](JTB::Str s) {
	return !s.endsWith(".");
    });
    ips.sort([](const JTB::Str s1, const JTB::Str s2) -> float {
	return strcmp(s1.c_str(), s2.c_str());
    });

    JTB::Vec<JTB::Str> ipsPruned;

    ips.forEach([&](JTB::Str s) {
	if (!ipsPruned.includes(s)) {
	    ipsPruned.push(s);
	}
    });

    display.displayMessage("Checking ips for roku & denon response...");

    for (int i = 0; !found() && i < ips.size(); ++i) {
	if (!rokuFound()) {
	    try {
		if (testForRoku(ips.at(i), curl)) { 
		    roku = ips.at(i); 
		    display.displayMessage("Roku found.");

		    const JTB::Str home { std::getenv("HOME") };
		    std::ofstream rokuDotEnvFile { home.concat("/.rokuip").c_str() };
		    rokuDotEnvFile << roku;
		}
	    } 
	    catch (std::runtime_error& e) {
		display.flashMessage("Roku Test Error for IP " + ips.at(i) + ": ");
		display.flashMessage(e.what());
	    }  
	} 
	if (!denonFound()) {
	    try {
		if (testForDenon(ips.at(i), curl)) { 
		    denon = ips.at(i); 
		    display.displayMessage("Denon found.");

		    const JTB::Str home { std::getenv("HOME") };
		    std::ofstream denonDotEnvFile { home.concat("/.denonip").c_str() };
		    denonDotEnvFile << denon;
		}
	    } 
	    catch (std::runtime_error& e) {
		display.flashMessage("Denon Test Error for IP " + ips.at(i) + ": ");
		display.flashMessage(e.what());
	    }  
	} 
    }
    display.displayMessage("Done testing.");
    display.clearMessages(3000);
    if (!roku.startsWith(IPs::ipstart)) throw std::runtime_error("Roku not found.");
    if (!denon.startsWith(IPs::ipstart)) throw std::runtime_error("Denon not found.");
}
