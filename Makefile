rokuctrl: rokuctrl.cpp curl_helpers.cpp
	g++ -std=c++20 -o rokuctrl rokuctrl.cpp curl_helpers.cpp -lcurl -lncurses
