/* This is an ncurses program for controlling a roku device and a denon avr 
 * using the command line <= 09/18/23 19:32:38 */ 
#include <cstring>
#include <cstdlib>
#include <csignal>
#include <format>
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
#include "./curl_helpers.h"



struct Denon_control {
    Denon_control(std::string ip_input) {
	ip = ip_input;
	curl = curl_easy_init();
	if (!curl) { throw std::runtime_error("curl initialization failed"); }
	readBuffer = "";
    }

    ~Denon_control() {
	curl_easy_cleanup(curl);
    }

    void volumeUp() {
	volume(VOL::UP);
    }
    
    void volumeDown() {
	volume(VOL::DOWN);
    }

private: 
    CURL* curl;
    std::string readBuffer;
    std::string ip;

    enum class VOL { UP, DOWN };

    void volume(VOL vol) {

	std::string tailURL { "/MainZone/index.put.asp" };
	std::string URL = std::format("http://{}{}", ip, tailURL);

	char *commandbody;
	std::string command { "cmd0=" };

	/* these are the values for cmd0 that need to be posted to modify the volume <= 09/18/23 18:38:32 */ 
	if (vol == VOL::UP) { commandbody = curl_easy_escape(curl, "PutMasterVolumeBtn/>", 0); }
	else if (vol == VOL::DOWN) { commandbody = curl_easy_escape(curl, "PutMasterVolumeBtn/<", 0); }
	else throw std::runtime_error("passed a bad arg to volume method");

	if (commandbody) {

	    std::string post_command = command.append(commandbody);

	    curl_execute(curl, readBuffer, URL, HTTP_MODE::POST, post_command);

	    curl_free(commandbody);

	} else throw std::runtime_error("failed to allocate uri string");

    }
};


struct Roku_query {
    Roku_query(std::string ip_input) {
	ip = ip_input;
	curl = curl_easy_init();
	if (!curl) { throw std::runtime_error("curl initialization failed"); }
	readBuffer = "";
    }

    ~Roku_query() {
	curl_easy_cleanup(curl);
    }

    void rokucommand(const char * command) {

	std::string URL = std::format("http://{}:8060/keypress/", ip); 
	
	URL = URL.append(command).c_str();

	curl_execute(curl, readBuffer, URL, HTTP_MODE::POST);

    }

private: 
    CURL* curl;
    std::string readBuffer;
    std::string ip;

};

#include <cctype>
#include <iomanip>
#include <sstream>


std::string url_encode(const std::string value) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (std::string::const_iterator i = value.begin(), n = value.end(); i != n; ++i) {
        std::string::value_type c = (*i);

        // Keep alphanumeric and other accepted characters intact
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
            continue;
        }

        // Any other characters are percent-encoded
        escaped << std::uppercase;
        escaped << '%' << std::setw(2) << int((unsigned char) c);
        escaped << std::nouppercase;
    }

    return escaped.str();
}

struct LiteralMode {
    LiteralMode() { litmode = false; };

    enum class KeyType { COMMAND, NONCOMMAND };
    void toggle() { litmode = !litmode; };
    operator bool() { return litmode; };

    void handle(Roku_query& roku, const std::string literal, KeyType keytype = KeyType::NONCOMMAND, std::string command = "") {
	if (litmode) { 
	    if (command != "backspace") {
		command = "";
		command.append("Lit_");
		command.append(url_encode(literal));
	    } 
	    roku.rokucommand(command.c_str()); 
	}
	else { 
	    if (keytype == KeyType::COMMAND) { roku.rokucommand(command.c_str()); }
	    else {
		const char *skeleton = "You pressed the '%c' key!\n"; 
		char buf[std::strlen(skeleton)+1];
		sprintf(buf, skeleton, literal[0]); 
		flash_string(buf); 
	    }
	}
    }

    private: 
	bool litmode; 

};

void handle_keypress(LiteralMode& literalmode, char key, Roku_query& roku, Denon_control& denon) {

    switch (key) {
	case '': {
	    literalmode.toggle();
	    if (literalmode) flash_string("literal mode"); else flash_string("default mode");
	    break;
	}
	case '=': denon.volumeUp(); break;
	case '-': denon.volumeDown(); break;
	case 'a': literalmode.handle(roku, "a", LiteralMode::KeyType::COMMAND, "play"); break;
	case 's': literalmode.handle(roku, "s", LiteralMode::KeyType::COMMAND, "pause"); break;
	case 'h': literalmode.handle(roku, "h", LiteralMode::KeyType::COMMAND, "left"); break;
	case 'l': literalmode.handle(roku, "l", LiteralMode::KeyType::COMMAND, "right"); break;
	case 'k': literalmode.handle(roku, "k", LiteralMode::KeyType::COMMAND, "up"); break;
	case 'j': literalmode.handle(roku, "j", LiteralMode::KeyType::COMMAND, "down"); break;
	case 'd': literalmode.handle(roku, "d", LiteralMode::KeyType::COMMAND, "rev"); break;
	case 'f': literalmode.handle(roku, "f", LiteralMode::KeyType::COMMAND, "fwd"); break;
	case 'g': literalmode.handle(roku, "g", LiteralMode::KeyType::COMMAND, "home"); break;
	case 'b': literalmode.handle(roku, "b", LiteralMode::KeyType::COMMAND, "back"); break;
	case 'r': literalmode.handle(roku, "r", LiteralMode::KeyType::COMMAND, "instantreplay"); break;
	case '\n': literalmode.handle(roku, "\n", LiteralMode::KeyType::COMMAND, "enter"); break;
	case '': literalmode.handle(roku, "\b", LiteralMode::KeyType::COMMAND, "backspace"); break;
	case '\x07': literalmode.handle(roku, "\b", LiteralMode::KeyType::COMMAND, "backspace"); break;
	case 'i': literalmode.handle(roku, "i", LiteralMode::KeyType::COMMAND, "info"); break;
	case ' ': literalmode.handle(roku, " ", LiteralMode::KeyType::COMMAND, "select"); break;
	default: literalmode.handle(roku, std::format("{}", key));
    }
}



int main() {
    // Initialize ncurses
    initscr();
    cbreak();      // Line buffering disabled, pass everything to me
    noecho();      // Don't echo() while we do getch
    keypad(stdscr, TRUE);  // Enable special keys

    printw("%s", "a => play\n\
s => pause\n\
d => rev\n\
f => fwd\n\
g => home\n\
h => left\n\
l => right\n\
k => up\n\
j => down\n\
b => back\n\
r => instantreplay\n\
i => info\n\
enter => enter\n\
backspace => backspace\n\
spacebar => select\n\
= => volume up\n\
- => volume down\n\
Ctrl+l => toggle literal input\n");

    IPs ips;

    Roku_query roku(ips.getRoku());
    Denon_control denon(ips.getDenon());
    LiteralMode literalmode;

    char ch;
    while ((ch = getch()) != 'q') {
	try{
	    handle_keypress(literalmode, ch, roku, denon);
	} catch (std::runtime_error e) {
	    if (std::string(e.what()).find("Couldn't connect") != std::string::npos) {
		flash_string("Resetting IPs...");
		ips.setIPs();
	    }
	}
    }

    // Cleanup and close ncurses
    endwin();
    return 0;
}
