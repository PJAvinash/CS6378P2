#ifndef IO_H
#define IO_H

#include <string>
#include <vector>
#include <functional>
#include "dsstructs.h"

// Function to get the IPv4 address from a hostname
std::string getIPV4(const std::string &hostname);

// Function to send a message to a destination node
int sendMessage(const std::vector<unsigned char> &bytes, const Node &destination);

// Function to handle a client connection
void handleclient(int client, std::function<void(std::vector<unsigned char>)> onMessageEvent);

// Function to listen for incoming connections on a port and handle them asynchronously
void listenthread(int port, std::function<void(std::vector<unsigned char>)> onMessageEvent);
#endif // IO_H
