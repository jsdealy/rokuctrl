# rokuctrl
This is an ncurses app for controlling a roku device and denon avr using the command line.

# Installation
You'll need ncurses and libcurl. 

To build it using g++ you can execute `g++ -o rokuctrl ./rokuctrl.cpp -lncurses -lcurl`. 

# Configuration
It defaults to the URLs used by my home router. To change this, set the ROKU_URL and DENON_URL variables in your shell environment.
