/* This is an ncurses program for controlling a roku device and a denon avr 
 * using the command line <= 09/18/23 19:32:38 */ 
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <curl/curl.h>
#include <curl/easy.h>
#include <ncurses.h>
#include <regex>


size_t write_callback(void* contents, size_t size, size_t nmemb, void* userdata) {
    // Compute the real size of the incoming buffer
    size_t realSize = size * nmemb;

    // Convert the buffer to a string and append it to the output string
    std::string* outStr = (std::string*)userdata;
    outStr->append((char*)contents, realSize);

    return realSize;
}

void curl_execute(CURL *curl,
		  std::string& readBuffer,
		  std::string& URL,
		  size_t (*write_callback)(void* contents, size_t size, size_t nmemb, void* userdata),
		  std::string post_command = "") {

    curl_easy_setopt(curl, CURLOPT_URL, URL.c_str());

    /* this ensures we send a post request <= 09/18/23 18:39:06 */ 
    curl_easy_setopt(curl, CURLOPT_POST, 1L);

    /* this is what sets the posted data <= 09/18/23 18:39:16 */ 
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_command.c_str());

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
	endwin();
	std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
	throw std::runtime_error("curl performance error");
    } 

    std::regex re { R"([^\s])" };

    if (std::regex_match(readBuffer, re)) {
	printw("%s", readBuffer.c_str());
	readBuffer = "";
    }
}

struct Denon_control {
    Denon_control() {
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

    enum class VOL { UP, DOWN };

    void volume(VOL vol) {

	const char *baseURL = getenv("DENON_URL");
	baseURL = baseURL ? baseURL : "http://192.168.1.149";
	std::string tailURL { "/MainZone/index.put.asp" };
	std::string URL { baseURL }; URL.append(tailURL);

	char *commandbody;
	std::string command { "cmd0=" };

	/* these are the values for cmd0 that need to be posted to modify the volume <= 09/18/23 18:38:32 */ 
	if (vol == VOL::UP) { commandbody = curl_easy_escape(curl, "PutMasterVolumeBtn/>", 0); }
	else if (vol == VOL::DOWN) { commandbody = curl_easy_escape(curl, "PutMasterVolumeBtn/<", 0); }
	else throw std::runtime_error("passed a bad arg to volume method");

	if (commandbody) {

	    std::string post_command = command.append(commandbody);

	    curl_execute(curl, readBuffer, URL, write_callback, post_command);

	    curl_free(commandbody);

	} else throw std::runtime_error("failed to allocate uri string");
	
    }
};


struct Roku_query {
    Roku_query() {
	curl = curl_easy_init();
	if (!curl) { throw std::runtime_error("curl initialization failed"); }
	readBuffer = "";
    }

    ~Roku_query() {
	curl_easy_cleanup(curl);
    }

    void rokucommand(const char * command) {

	const char *baseURL = getenv("ROKU_URL");
	baseURL = baseURL ? baseURL : "http://192.168.1.107:8060/keypress/";
	std::string URL { baseURL }; 
	
	URL = URL.append(command).c_str();

	curl_execute(curl, readBuffer, URL, write_callback);

    }

private: 
    CURL* curl;
    std::string readBuffer;

};


void handle_keypress(char key, Roku_query& roku, Denon_control& denon) {
    switch (key) {
	case '+': denon.volumeUp(); break;
	case '-': denon.volumeDown(); break;
	case 'a': roku.rokucommand("play"); break;
	case 's': roku.rokucommand("pause"); break;
	case 'h': roku.rokucommand("left"); break;
	case 'l': roku.rokucommand("right"); break;
	case 'k': roku.rokucommand("up"); break;
	case 'j': roku.rokucommand("down"); break;
	case 'd': roku.rokucommand("rev"); break;
	case 'f': roku.rokucommand("fwd"); break;
	case 'g': roku.rokucommand("home"); break;
	case 'b': roku.rokucommand("back"); break;
	case 'r': roku.rokucommand("instantreplay"); break;
	case '\n': roku.rokucommand("enter"); break;
	case '\b': roku.rokucommand("backspace"); break;
	case 'i': roku.rokucommand("info"); break;
	case ' ': roku.rokucommand("select"); break;
	default: printw("You pressed the '%c' key!\n", key); break;
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
+ => volume up\n\
- => volume down\n");

    Roku_query roku;
    Denon_control denon;

    char ch;
    while ((ch = getch()) != 'q') {
	handle_keypress(ch, roku, denon);
    }

    // Cleanup and close ncurses
    endwin();
    return 0;
}
