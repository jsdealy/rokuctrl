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
#include <thread>
#include <chrono>
#include <array>
#include <regex>
#include "./curl_helpers.h"
#include "./display.h"
#include "jtb/jtbcli.h"



extern DEBUGMODE debugmode;

struct Denon_control {
    Denon_control(const JTB::Str& ip_input, Display& display): ip(ip_input), pow(false)  {
	if (ip.isEmpty()) display.displayMessage("No Denon IP!");
    }

    void volumeUp(Curl& curl) {
	cmd(CMD::VOLUP, curl);
    }

    void mute(Curl& curl) {
	cmd(CMD::MUTE, curl);
    }
    
    void volumeDown(Curl& curl) {
	cmd(CMD::VOLDOWN, curl);
    }

    void power(Curl& curl) {
	cmd(CMD::POWER, curl);
    }

    void switchTo(std::string source, Curl& curl) {
	denonPost("PutZone_InputFunction/" + url_encode(source), curl);
    }

    bool ipIsStored() const {
	return ip.startsWith(IPs::ipstart);
    }

    void setIP(JTB::Str inputIP) {
	if (!inputIP.startsWith(IPs::ipstart)) {
	    throw std::runtime_error("Tried to store improper IP in Denon control.");
	}
	ip = inputIP;
    }

private: 
    JTB::Str ip;
    enum class CMD { MUTE, VOLUP, VOLDOWN, POWER };
    bool pow; 

    void denonPost(JTB::Str commandbody, Curl& curl) {
	JTB::Str readBuffer {};

	JTB::Str URL { "http://" + ip + "/MainZone/index.put.asp"}; 

	JTB::Str post_command { "cmd0=" + commandbody };

	curl.curl_execute(readBuffer, URL, HTTP_MODE::POST, post_command);
    }

    void cmd(CMD com, Curl& curl) {
	std::string commandbody;
	/* these are the values for cmd0 that need to be posted to modify the volume <= 09/18/23 18:38:32 */ 
	if (com == CMD::VOLUP) { commandbody = url_encode("PutMasterVolumeBtn/>"); }
	else if (com == CMD::MUTE) { commandbody = url_encode("PutVolumeMute/TOGGLE"); }
	else if (com == CMD::VOLDOWN) { commandbody = url_encode("PutMasterVolumeBtn/<"); }
	else if (com == CMD::POWER) { 
	    commandbody = powerstate(curl) ? url_encode("PutZone_OnOff/OFF") : url_encode("PutZone_OnOff/ON");
	    if (debugmode == DEBUGMODE::ON) { printw("%s\n", commandbody.c_str()); };
	} else throw std::runtime_error("passed a bad arg to denon cmd method");

	denonPost(commandbody, curl);
    }

    bool powerstate(Curl& curl) {
	JTB::Str buff;
	JTB::Str tailURL { "/goform/formMainZone_MainZoneXml.xml" };
	JTB::Str URL { "http://" };
	URL += ip + tailURL;
	curl.curl_execute(buff, URL);
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
    Roku_query(const JTB::Str& ip_input, Display& display): ip(ip_input), readBuffer("") {
	if (ip.isEmpty()) display.displayMessage("No Roku IP!");
    }

    void setIP(const JTB::Str& ipInput) {
	if (!ipInput.startsWith(IPs::ipstart)) {
	    throw std::runtime_error("Tried to store improper IP in Roku_query.");
	}
	ip = ipInput;
    }

    void rokucommand(const char * command, Curl& curl) {

	JTB::Str URL { "http://" + ip + ":8060/keypress/" }; 
	
	URL += command;

	if (debugmode == DEBUGMODE::ON) { printw("%s", URL.c_str()); }

	curl.curl_execute(readBuffer, URL, HTTP_MODE::POST);

    }

    bool ipIsStored() const {
	return ip.startsWith(IPs::ipstart);
    }

private: 
    JTB::Str readBuffer;
    JTB::Str ip;

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
    LiteralMode(): litmode(false) {};

    enum class KeyType { BLURAYSOUNDON, BLURAYCOMMAND, ROKUCOMMAND, DENONCOMMAND, NONCOMMAND };

    void toggle(Display& display) { 
	litmode = !litmode; 
	if (litmode) display.displayMessage("literal mode"); 
	else { display.clearMessages(); display.flashMessage("default mode", 250); }
    };

    operator bool() { return litmode; };

    void handle(Curl& curl, 
		Roku_query& roku, 
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
	    roku.rokucommand(litstring.c_str(), curl); 
	} 
	else { 
	    if (keytype == KeyType::ROKUCOMMAND) { roku.rokucommand(std::get<std::string>(command).c_str(), curl); }
	    else if (keytype == KeyType::DENONCOMMAND) { std::get<const std::function<void ()>>(command)(); }
	    else if (keytype == KeyType::BLURAYCOMMAND) {
		JTB::ClCommand callToPythonBlurayC("blurayC.py " + std::get<std::string>(command));
		display.flashMessage(callToPythonBlurayC.getResult().first.stdstr());
	    }
	    else if (keytype == KeyType::BLURAYSOUNDON) {
		display.displayMessage("Is the audio on? [Y/n]");
		JTB::Str check {getch()};
		display.clearMessages();
		if (!check.lower().startsWith("n")) {
		    return;
		}
		JTB::ClCommand callToPythonBlurayC("blurayC.py back");
		display.flashMessage(callToPythonBlurayC.getResult().first.stdstr());
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		callToPythonBlurayC.sendCommand("blurayC.py right");
		display.flashMessage(callToPythonBlurayC.getResult().first.stdstr());
		std::this_thread::sleep_for(std::chrono::milliseconds(600));
		callToPythonBlurayC.sendCommand("blurayC.py right");
		display.flashMessage(callToPythonBlurayC.getResult().first.stdstr());
		std::this_thread::sleep_for(std::chrono::milliseconds(600));
		callToPythonBlurayC.sendCommand("blurayC.py right");
		display.flashMessage(callToPythonBlurayC.getResult().first.stdstr());
		std::this_thread::sleep_for(std::chrono::milliseconds(600));
		callToPythonBlurayC.sendCommand("blurayC.py okay");
		display.flashMessage(callToPythonBlurayC.getResult().first.stdstr());
		std::this_thread::sleep_for(std::chrono::milliseconds(600));
		callToPythonBlurayC.sendCommand("blurayC.py down");
		display.flashMessage(callToPythonBlurayC.getResult().first.stdstr());
		std::this_thread::sleep_for(std::chrono::milliseconds(600));
		callToPythonBlurayC.sendCommand("blurayC.py right");
		display.flashMessage(callToPythonBlurayC.getResult().first.stdstr());
		std::this_thread::sleep_for(std::chrono::milliseconds(600));
		callToPythonBlurayC.sendCommand("blurayC.py down");
		display.flashMessage(callToPythonBlurayC.getResult().first.stdstr());
		std::this_thread::sleep_for(std::chrono::milliseconds(600));
		callToPythonBlurayC.sendCommand("blurayC.py down");
		display.flashMessage(callToPythonBlurayC.getResult().first.stdstr());
		std::this_thread::sleep_for(std::chrono::milliseconds(600));
		callToPythonBlurayC.sendCommand("blurayC.py down");
		display.flashMessage(callToPythonBlurayC.getResult().first.stdstr());
		std::this_thread::sleep_for(std::chrono::milliseconds(600));
		callToPythonBlurayC.sendCommand("blurayC.py right");
		display.flashMessage(callToPythonBlurayC.getResult().first.stdstr());
		std::this_thread::sleep_for(std::chrono::milliseconds(600));
		callToPythonBlurayC.sendCommand("blurayC.py down");
		display.flashMessage(callToPythonBlurayC.getResult().first.stdstr());
		std::this_thread::sleep_for(std::chrono::milliseconds(600));
		callToPythonBlurayC.sendCommand("blurayC.py okay");
		display.flashMessage(callToPythonBlurayC.getResult().first.stdstr());
		std::this_thread::sleep_for(std::chrono::milliseconds(1500));
		callToPythonBlurayC.sendCommand("blurayC.py back");
		display.flashMessage(callToPythonBlurayC.getResult().first.stdstr());
		std::this_thread::sleep_for(std::chrono::milliseconds(800));
		callToPythonBlurayC.sendCommand("blurayC.py play");
		display.flashMessage(callToPythonBlurayC.getResult().first.stdstr());
	    } 
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

void handle_keypress(Curl& curl,
		     LiteralMode& literalmode,
		     char key,
		     Roku_query& roku,
		     Denon_control& denon,
		     IPs& ips,
		     Display& display) {

    switch (key) {
	case '': literalmode.toggle(display); break;
	case '\\': try { 
			ips.setIPs(display, curl); 
			roku.setIP(ips.getRoku()); 
			denon.setIP(ips.getDenon());
		    } 
		    catch (std::runtime_error& e) { 
			display.displayMessage(e.what()); 
		    } break;
	case 'X': literalmode.handle(curl, roku, "X", LiteralMode::KeyType::BLURAYSOUNDON, "", display); break;
	case '8': literalmode.handle(curl, roku, "8", LiteralMode::KeyType::BLURAYCOMMAND, "stripon", display); break;
	case '9': literalmode.handle(curl, roku, "9", LiteralMode::KeyType::BLURAYCOMMAND, "stripoff", display); break;
	case '0': literalmode.handle(curl, roku, "0", LiteralMode::KeyType::BLURAYCOMMAND, "yamahapower", display); break;
	case 'P': literalmode.handle(curl, roku, "P", LiteralMode::KeyType::BLURAYCOMMAND, "tvpower", display); break;
	case 'U': literalmode.handle(curl, roku, "U", LiteralMode::KeyType::BLURAYCOMMAND, "bluraypower", display); break;
	case 'A': literalmode.handle(curl, roku, "S", LiteralMode::KeyType::BLURAYCOMMAND, "play", display); break;
	case 'S': literalmode.handle(curl, roku, "S", LiteralMode::KeyType::BLURAYCOMMAND, "pause", display); break;
	case 'T': literalmode.handle(curl, roku, "T", LiteralMode::KeyType::BLURAYCOMMAND, "stop", display); break;
	case 'D': literalmode.handle(curl, roku, "D", LiteralMode::KeyType::BLURAYCOMMAND, "rewind", display); break;
	case 'F': literalmode.handle(curl, roku, "F", LiteralMode::KeyType::BLURAYCOMMAND, "fastforward", display); break;
	case 'H': literalmode.handle(curl, roku, "H", LiteralMode::KeyType::BLURAYCOMMAND, "left", display); break;
	case 'L': literalmode.handle(curl, roku, "L", LiteralMode::KeyType::BLURAYCOMMAND, "right", display); break;
	case 'K': literalmode.handle(curl, roku, "K", LiteralMode::KeyType::BLURAYCOMMAND, "up", display); break;
	case 'J': literalmode.handle(curl, roku, "J", LiteralMode::KeyType::BLURAYCOMMAND, "down", display); break;
	case 'B': literalmode.handle(curl, roku, "B", LiteralMode::KeyType::BLURAYCOMMAND, "back", display); break;
	case 'O': literalmode.handle(curl, roku, "O", LiteralMode::KeyType::BLURAYCOMMAND, "okay", display); break;
	case 'M': literalmode.handle(curl, roku, "M", LiteralMode::KeyType::BLURAYCOMMAND, "menu", display); break;
	case 'E': literalmode.handle(curl, roku, "E", LiteralMode::KeyType::BLURAYCOMMAND, "eject", display); break;
	case '=': literalmode.handle(curl, roku, "=", LiteralMode::KeyType::DENONCOMMAND, [&]() { denon.volumeUp(curl); }, display); break;
	case 'm': literalmode.handle(curl, roku, "m", LiteralMode::KeyType::DENONCOMMAND, [&]() { denon.mute(curl); }, display); break;
	case '1': literalmode.handle(curl, roku, "1", LiteralMode::KeyType::DENONCOMMAND, [&]() { denon.switchTo("MPLAY", curl); }, display); break;
	case '2': literalmode.handle(curl, roku, "2", LiteralMode::KeyType::DENONCOMMAND, [&]() { denon.switchTo("DVD", curl); }, display); break;
	case '3': literalmode.handle(curl, roku, "3", LiteralMode::KeyType::DENONCOMMAND, [&]() { denon.switchTo("GAME", curl); }, display); break;
	case '4': literalmode.handle(curl, roku, "4", LiteralMode::KeyType::DENONCOMMAND, [&]() { denon.switchTo("BD", curl); }, display); break;
	case '5': literalmode.handle(curl, roku, "5", LiteralMode::KeyType::DENONCOMMAND, [&]() { denon.switchTo("AUX1", curl); }, display); break;
	case '6': literalmode.handle(curl, roku, "6", LiteralMode::KeyType::DENONCOMMAND, [&]() { denon.switchTo("SAT/CBL", curl); }, display); break;
	case '7': literalmode.handle(curl, roku, "7", LiteralMode::KeyType::DENONCOMMAND, [&]() { denon.switchTo("TUNER", curl); }, display); break;
	case '-': literalmode.handle(curl, roku, "-", LiteralMode::KeyType::DENONCOMMAND, [&]() { denon.volumeDown(curl); }, display); break;
	case 'p': literalmode.handle(curl, roku, "p", LiteralMode::KeyType::DENONCOMMAND, [&]() { denon.power(curl); }, display); break;
	case 'a': literalmode.handle(curl, roku, "a", LiteralMode::KeyType::ROKUCOMMAND, "play", display); break;
	case '*': literalmode.handle(curl, roku, "*", LiteralMode::KeyType::ROKUCOMMAND, "search", display); break;
	case 's': literalmode.handle(curl, roku, "s", LiteralMode::KeyType::ROKUCOMMAND, "pause", display); break;
	case 'h': literalmode.handle(curl, roku, "h", LiteralMode::KeyType::ROKUCOMMAND, "left", display); break;
	case 'l': literalmode.handle(curl, roku, "l", LiteralMode::KeyType::ROKUCOMMAND, "right", display); break;
	case 'k': literalmode.handle(curl, roku, "k", LiteralMode::KeyType::ROKUCOMMAND, "up", display); break;
	case 'j': literalmode.handle(curl, roku, "j", LiteralMode::KeyType::ROKUCOMMAND, "down", display); break;
	case 'd': literalmode.handle(curl, roku, "d", LiteralMode::KeyType::ROKUCOMMAND, "rev", display); break;
	case 'f': literalmode.handle(curl, roku, "f", LiteralMode::KeyType::ROKUCOMMAND, "fwd", display); break;
	case 'g': literalmode.handle(curl, roku, "g", LiteralMode::KeyType::ROKUCOMMAND, "home", display); break;
	case 'b': literalmode.handle(curl, roku, "b", LiteralMode::KeyType::ROKUCOMMAND, "back", display); break;
	case 'r': literalmode.handle(curl, roku, "r", LiteralMode::KeyType::ROKUCOMMAND, "instantreplay", display); break;
	case '\n': literalmode.handle(curl, roku, "\n", LiteralMode::KeyType::ROKUCOMMAND, "enter", display); break;
	case '\x07': literalmode.handle(curl, roku, "\b", LiteralMode::KeyType::ROKUCOMMAND, "backspace", display); break;
	case 'i': literalmode.handle(curl, roku, "i", LiteralMode::KeyType::ROKUCOMMAND, "info", display); break;
	case 'o': literalmode.handle(curl, roku, "o", LiteralMode::KeyType::ROKUCOMMAND, "select", display); break;
	default: literalmode.handle(curl, roku, std::format("{}", key), LiteralMode::KeyType::NONCOMMAND, std::format("{}", key), display);
    }
}



int main(int argc, char **argv) {
    if (argc > 1 && std::string(*(argv+1)) == "--debug") { printw("%s\n", *(argv+1)); debugmode = DEBUGMODE::ON; };

    std::vector<std::string> staticDisplay {
	"a (A) => play",
	"s (S) => pause",
	"d (D) => rev",
	"f (F) => fwd",
	"h (H) => left",
	"l (L) => right",
	"k (K) => up",
	"j (J) => down",
	"b (B) => back",
	"M => menu",
	"E => eject",
	"T => stop",
	"X => activate blu-ray audio",
	"o (O) => okay",
	"p (P) (U) => power",
	"g => home",
	"r => instantreplay",
	"i => info",
	"enter => enter",
	"backspace => backspace",
	"* => search",
	"= => volume up",
	"- => volume down",
	"m => mute",
	"Ctrl+l => toggle literal input",
	"\\ => reset & confirm ips"
    };

    Display display(staticDisplay);

    /* note that the IPs constructor takes a reference to Display, as 
    otherwise we'll initiate a new display (a copy) when we pass display <= 01/13/24 16:29:25 */ 
    Curl curl;
    IPs ips(display, curl);
    Roku_query roku(ips.getRoku(), display);
    Denon_control denon(ips.getDenon(), display);
    if (roku.ipIsStored() && denon.ipIsStored()) display.displayMessage("Roku and denon IPs are set.");
    else display.displayMessage("Missing some IP.");
    display.clearMessages(1500);
    LiteralMode literalmode;
    char ch;
    while ((ch = getch()) != 'q') {
	try{
	    handle_keypress(curl, literalmode, ch, roku, denon, ips, display);
	} catch (std::runtime_error e) {
	    if (std::string(e.what()).find("Couldn't connect") != std::string::npos) {
		display.displayMessage("Resetting IPs...");
		ips.setIPs(display, curl);
	    }
	}
    }

    // Cleanup and close ncurses
    return 0;
}
