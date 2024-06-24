rokuctrl: rokuctrl.cpp curl_helpers.cpp
	g++ -std=c++20 -o rokuctrl curl_helpers.cpp rokuctrl.cpp -Wall -Wextra -lcurl -lncurses > errors.txt 2>&1
