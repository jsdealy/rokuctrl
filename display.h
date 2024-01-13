#pragma once
#include <vector>
#include <ncurses.h>
#include <string>

class Display {
    int numberOfPersistentLines;
    int numberOfDisplayMessages;
public:
    Display(std::vector<std::string> persistentLines): numberOfPersistentLines(2), numberOfDisplayMessages(0) {
	// Initialize ncurses
	initscr();
	cbreak();      // Line buffering disabled, pass everything to me
	noecho();      // Don't echo() while we do getch
	keypad(stdscr, TRUE);  // Enable special keys
	int longestPersistentLine = 2;
	std::string topBottomBorder = "+-";
	for (std::string s : persistentLines) {
	    if (s.length() > longestPersistentLine) longestPersistentLine = s.length();
	    numberOfPersistentLines++;
	}
	for (auto i = 0; i < longestPersistentLine; i++) {
	    topBottomBorder.push_back('-');
	}
	topBottomBorder.append("-+");

	printw("%s\n", topBottomBorder.c_str());
	for (std::string s : persistentLines) {
	    std::string spacerAndBackBorder = "";
	    for (auto i = 0; i < longestPersistentLine - s.length(); i++) {
		spacerAndBackBorder.push_back(' ');
	    }
	    spacerAndBackBorder.push_back('|');
	    printw("| %s", s.c_str());
	    printw("%s", spacerAndBackBorder.c_str());
	}
	printw("%s\n", topBottomBorder.c_str());
	refresh();
    }

    ~Display() {
	printw("%s\n", "Ending ncurses session...");
	refresh();
	napms(2000);
	endwin();
    }

    void flashMessage(std::string s, int milliseconds = 250) {
	printw("%s\n", s.c_str());
	refresh();
	/* sleep for milliseconds <= 01/13/24 13:27:21 */ 
	napms(milliseconds);
	move(numberOfPersistentLines + numberOfDisplayMessages,0);
	clrtobot();
	refresh();
    }

    void displayMessage(std::string s) {
	numberOfDisplayMessages++;
	printw("%s\n", s.c_str());
	refresh();
    }

    void clearMessages(int wait = 0, bool debug = false) {
	move(numberOfPersistentLines, 0);
	if (debug) {
	    printw("%s", "About to clear messages...");
	    refresh();
	}
	napms(wait);
	clrtobot();
	refresh();
    }

};
