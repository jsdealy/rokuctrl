/* This is an ncurses program for controlling a roku device and a denon avr 
 * using the command line <= 09/18/23 19:32:38 */ 
#include <cstring>
#include <cstdlib>
#include <variant>
#include <functional>
#include <csignal>
#include <format>
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
#include "./curl_helpers.h"
#include "./display.h"

extern DEBUGMODE debugmode;

struct Denon_control {
    Denon_control(std::string ip_input) {
	ip = ip_input;
	if (ip.empty()) throw std::runtime_error("No denon IP.");
	curl = curl_easy_init();
	if (!curl) { throw std::runtime_error("curl initialization failed"); }
    }

    ~Denon_control() {
	curl_easy_cleanup(curl);
    }

    void volumeUp() {
	cmd(CMD::VOLUP);
    }
    
    void volumeDown() {
	cmd(CMD::VOLDOWN);
    }

    void power() {
	cmd(CMD::POWER);
    }

    void switchTo(std::string source) {
	denonPost("PutZone_InputFunction/" + url_encode(source));
    }

private: 
    CURL* curl;
    std::string ip;

    enum class CMD { VOLUP, VOLDOWN, POWER };

    void denonPost(std::string commandbody) {
	std::string readBuffer;
	std::string tailURL { "/MainZone/index.put.asp" };
	std::string URL = std::format("http://{}{}", ip, tailURL);

	std::string command { "cmd0=" };
	
	/* if (debugmode == DEBUGMODE::ON) { printw("URL: %s", URL.c_str()); } */

	std::string post_command = command.append(commandbody);

	/* if (debugmode == DEBUGMODE::ON) { printw("POST: %s", post_command.c_str()); } */

	curl_execute(curl, readBuffer, URL, HTTP_MODE::POST, post_command);
    }

    void cmd(CMD com) {


	std::string commandbody;
	/* these are the values for cmd0 that need to be posted to modify the volume <= 09/18/23 18:38:32 */ 
	if (com == CMD::VOLUP) { commandbody = url_encode("PutMasterVolumeBtn/>"); }
	else if (com == CMD::VOLDOWN) { commandbody = url_encode("PutMasterVolumeBtn/<"); }
	else if (com == CMD::POWER) { 
	    commandbody = powerstate() ? url_encode("PutZone_OnOff/OFF") : url_encode("PutZone_OnOff/ON");
	    if (debugmode == DEBUGMODE::ON) { printw("%s\n", commandbody.c_str()); };
	} else throw std::runtime_error("passed a bad arg to denon cmd method");

	denonPost(commandbody);


    }

    bool powerstate() {
	std::string buff;
	std::string tailURL { "/goform/formMainZone_MainZoneXml.xml" };
	std::string URL = std::format("http://{}{}", ip, tailURL);
	curl_execute(curl, buff, URL);
	if (buff.find("<Power><value>ON</value></Power>") != std::string::npos) {
	    if (debugmode == DEBUGMODE::ON) { printw("%s\n", "returning true"); };
	    return true;
	} else {
	    if (debugmode == DEBUGMODE::ON) { printw("%s\n", "returning false"); };
	    return false;
	} 
    }
};


struct Roku_query {
    Roku_query(std::string ip_input) {
	ip = ip_input;
	if (ip.empty()) throw std::runtime_error("No Roku IP.");
	curl = curl_easy_init();
	if (!curl) { throw std::runtime_error("curl initialization failed"); }
	readBuffer = "";
    }

    ~Roku_query() {
	curl_easy_cleanup(curl);
    }

    void rokucommand(const char * command) {

	std::string URL = std::format("http://{}:8060/keypress/", ip); 
	
	URL = URL.append(command);

	if (debugmode == DEBUGMODE::ON) { printw("%s", URL.c_str()); }

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

    enum class KeyType { ROKUCOMMAND, DENONCOMMAND, NONCOMMAND };
    void toggle(Display& display) { 
	litmode = !litmode; 
	/* if (litmode) display.displayMessage("literal mode"); */ 
	/* else { display.clearMessages(); display.flashMessage("default mode", 250); } */
    };
    operator bool() { return litmode; };

    void handle(Roku_query& roku, 
		const std::string literal, 
		KeyType keytype, 
		const std::variant<std::string, const std::function<void ()>>& command,
		Display& display) {
	if (litmode) { 
	    std::string litstring = "";
	    if ((std::holds_alternative<std::string>(command) && std::get<std::string>(command) != "backspace")
		|| (std::holds_alternative<const std::function<void ()>>(command))) {
		litstring.append("Lit_");
		litstring.append(url_encode(literal));
	    } 
	    roku.rokucommand(litstring.c_str()); 
	} 
	else { 
	    if (keytype == KeyType::ROKUCOMMAND) { roku.rokucommand(std::get<std::string>(command).c_str()); }
	    else if (keytype == KeyType::DENONCOMMAND) { std::get<const std::function<void ()>>(command)(); }
	    else {
		const char *skeleton = "You pressed the '%c' key!\n"; 
		char buf[std::strlen(skeleton)+1];
		sprintf(buf, skeleton, literal[0]); 
		display.flashMessage(buf); 
	    }
	}
    }

    private: 
	bool litmode; 

};

void handle_keypress(LiteralMode& literalmode,
		     char key,
		     Roku_query& roku,
		     Denon_control& denon,
		     IPs& ips,
		     Display& display) {

    switch (key) {
	case '': {
	    literalmode.toggle(display);
	    break;
	}
	case '\\': ips.setIPs(display);
	case '=': literalmode.handle(roku, "=", LiteralMode::KeyType::DENONCOMMAND, [&]() { denon.volumeUp(); }, display); break;
	case '1': literalmode.handle(roku, "1", LiteralMode::KeyType::DENONCOMMAND, [&]() { denon.switchTo("MPLAY"); }, display); break;
	case '2': literalmode.handle(roku, "2", LiteralMode::KeyType::DENONCOMMAND, [&]() { denon.switchTo("DVD"); }, display); break;
	case '3': literalmode.handle(roku, "3", LiteralMode::KeyType::DENONCOMMAND, [&]() { denon.switchTo("GAME"); }, display); break;
	case '4': literalmode.handle(roku, "4", LiteralMode::KeyType::DENONCOMMAND, [&]() { denon.switchTo("BD"); }, display); break;
	case '5': literalmode.handle(roku, "5", LiteralMode::KeyType::DENONCOMMAND, [&]() { denon.switchTo("AUX1"); }, display); break;
	case '6': literalmode.handle(roku, "6", LiteralMode::KeyType::DENONCOMMAND, [&]() { denon.switchTo("SAT/CBL"); }, display); break;
	case '7': literalmode.handle(roku, "7", LiteralMode::KeyType::DENONCOMMAND, [&]() { denon.switchTo("TUNER"); }, display); break;
	case '-': literalmode.handle(roku, "-", LiteralMode::KeyType::DENONCOMMAND, [&]() { denon.volumeDown(); }, display); break;
	case 'a': literalmode.handle(roku, "a", LiteralMode::KeyType::ROKUCOMMAND, "play", display); break;
	case '*': literalmode.handle(roku, "*", LiteralMode::KeyType::ROKUCOMMAND, "search", display); break;
	case 'P': literalmode.handle(roku, "P", LiteralMode::KeyType::ROKUCOMMAND, "poweroff", display); break;
	case 's': literalmode.handle(roku, "s", LiteralMode::KeyType::ROKUCOMMAND, "pause", display); break;
	case 'h': literalmode.handle(roku, "h", LiteralMode::KeyType::ROKUCOMMAND, "left", display); break;
	case 'l': literalmode.handle(roku, "l", LiteralMode::KeyType::ROKUCOMMAND, "right", display); break;
	case 'k': literalmode.handle(roku, "k", LiteralMode::KeyType::ROKUCOMMAND, "up", display); break;
	case 'j': literalmode.handle(roku, "j", LiteralMode::KeyType::ROKUCOMMAND, "down", display); break;
	case 'd': literalmode.handle(roku, "d", LiteralMode::KeyType::ROKUCOMMAND, "rev", display); break;
	case 'f': literalmode.handle(roku, "f", LiteralMode::KeyType::ROKUCOMMAND, "fwd", display); break;
	case 'g': literalmode.handle(roku, "g", LiteralMode::KeyType::ROKUCOMMAND, "home", display); break;
	case 'b': literalmode.handle(roku, "b", LiteralMode::KeyType::ROKUCOMMAND, "back", display); break;
	case 'r': literalmode.handle(roku, "r", LiteralMode::KeyType::ROKUCOMMAND, "instantreplay", display); break;
	case '\n': literalmode.handle(roku, "\n", LiteralMode::KeyType::ROKUCOMMAND, "enter", display); break;
	case '\x07': literalmode.handle(roku, "\b", LiteralMode::KeyType::ROKUCOMMAND, "backspace", display); break;
	case 'i': literalmode.handle(roku, "i", LiteralMode::KeyType::ROKUCOMMAND, "info", display); break;
	case 'p': literalmode.handle(roku, "p", LiteralMode::KeyType::DENONCOMMAND, [&]() { denon.power(); }, display); break;
	case ' ': literalmode.handle(roku, " ", LiteralMode::KeyType::ROKUCOMMAND, "select", display); break;
	default: literalmode.handle(roku, std::format("{}", key), LiteralMode::KeyType::NONCOMMAND, std::format("{}", key), display);
    }
}



int main(int argc, char **argv) {
    if (argc > 1 && std::string(*(argv+1)) == "--debug") { printw("%s\n", *(argv+1)); debugmode = DEBUGMODE::ON; };

    std::vector<std::string> staticDisplay {
	"a => play",
	"s => pause",
	"d => rev",
	"f => fwd",
	"g => home",
	"h => left",
	"l => right",
	"k => up",
	"j => down",
	"b => back",
	"r => instantreplay",
	"i => info",
	"enter => enter",
	"backspace => backspace",
	"spacebar => select",
	"* => search",
	"= => volume up",
	"- => volume down",
	"Ctrl+l => toggle literal input",
	"\\ => reset ips"
    };

    Display display(staticDisplay);

    IPs ips(display);

    Roku_query roku(ips.getRoku());
    Denon_control denon(ips.getDenon());
    display.displayMessage("Roku and denon control established.");
    display.clearMessages(2000);
    LiteralMode literalmode;
    char ch;
    while ((ch = getch()) != 'q') {
	try{
	    handle_keypress(literalmode, ch, roku, denon, ips, display);
	} catch (std::runtime_error e) {
	    if (std::string(e.what()).find("Couldn't connect") != std::string::npos) {
		display.displayMessage("Resetting IPs...");
		ips.setIPs(display);
	    }
	}
    }

    // Cleanup and close ncurses
    return 0;
}
