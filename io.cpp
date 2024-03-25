#include <string>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>
#include <iostream>
#include <netdb.h>
#include <pthread.h>
#include "dsstructs.h"
#include "serialization.h"
#include <functional>
#include "io.h"

std::string getIPV4(const std::string &hostname)
{
    struct hostent *hostInfo = gethostbyname(hostname.c_str());
    if (hostInfo == nullptr)
    {
        std::cerr << "gethostbyname error: " << hstrerror(h_errno) << std::endl;
        return "";
    }
    // Assuming the first address is IPv4
    struct in_addr *addr = reinterpret_cast<struct in_addr *>(hostInfo->h_addr);
    return inet_ntoa(*addr);
}

// const std::vector<unsigned char> &bytes, const Node &destination
int sendMessage2(const std::string &message, const Node &destination)
{ //,const std::string& hostname, int port) {
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == -1)
    {
        std::cerr << "Error creating socket" << std::endl;
        return -1;
    }

    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(destination.port);

    // Resolve hostname to IP address
    struct hostent *host = gethostbyname(destination.hostname.c_str());
    if (host == nullptr)
    {
        std::cerr << "Failed to resolve hostname" << std::endl;
        close(clientSocket);
        return -1;
    }
    memcpy(&serverAddr.sin_addr, host->h_addr, host->h_length);

    // Connect to the server
    if (connect(clientSocket, reinterpret_cast<struct sockaddr *>(&serverAddr), sizeof(serverAddr)) == -1)
    {
        std::cerr << "Error connecting to server" << std::endl;
        close(clientSocket);
        return -1;
    }

    // Send message
    ssize_t sentBytes = send(clientSocket, message.c_str(), message.length(), 0);
    if (sentBytes == -1)
    {
        std::cerr << "Error sending message" << std::endl;
        close(clientSocket);
        return -1;
    }

    std::cout << "Message sent successfully" << std::endl;

    // Close the socket
    close(clientSocket);
    return 0;
}

int sendMessage(const std::vector<unsigned char> &bytes, const Node &destination)
{
    int client = socket(AF_INET, SOCK_STREAM, 0);
    if (client < 0)
    {
        perror("Error creating socket");
        return -1;
    }
    struct sockaddr_in destAddr;
    destAddr.sin_family = AF_INET;
    destAddr.sin_port = htons(destination.port);
    if (inet_pton(AF_INET, getIPV4(destination.hostname).c_str(), &destAddr.sin_addr) <= 0)
    {
        perror("Invalid address");
        close(client);
        return -1;
    }
    
    struct hostent *host = gethostbyname(destination.hostname.c_str());
    if (host == nullptr)
    {
        perror("Failed to resolve hostname");
        close(client);
        return -1;
    }
    memcpy(&destAddr.sin_addr, host->h_addr, host->h_length);
    if (connect(client, reinterpret_cast<struct sockaddr *>(&destAddr), sizeof(destAddr)) < 0)
    {
        perror("Error connecting");
        std::cout << getIPV4(destination.hostname).c_str() << " destination name : " << destination.hostname << " port: " << destination.port << "\n";
        close(client);
        return -1;
    }
    if (send(client, bytes.data(), bytes.size(), 0) < 0)
    {
        perror("Error sending message");
        close(client);
        return -1;
    }
    close(client);
    return 1;
}

void handleclient(int client, std::function<void(std::vector<unsigned char>)> onMessageEvent)
{
    std::vector<unsigned char> buffer(1024);
    int bytesReceived = recv(client, buffer.data(), buffer.size(), 0);
    if (bytesReceived < 0)
    {
        perror("Error receiving data");
        close(client);
    }

    std::vector<unsigned char> bytestream(buffer.begin(), buffer.begin() + bytesReceived);
    onMessageEvent(bytestream);
    close(client);
}

void listenthread(int port, std::function<void(std::vector<unsigned char>)> onMessageEvent)
{
    int listenSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (listenSocket < 0)
    {
        perror("Error creating listen socket");
        return;
    }

    struct sockaddr_in listenAddr;
    listenAddr.sin_family = AF_INET;
    listenAddr.sin_addr.s_addr = INADDR_ANY;
    listenAddr.sin_port = htons(port);

    if (bind(listenSocket, reinterpret_cast<struct sockaddr *>(&listenAddr), sizeof(listenAddr)) < 0)
    {
        perror("Error binding socket");
        close(listenSocket);
        return;
    }

    if (listen(listenSocket, 10000) < 0)
    {
        perror("Error listening");
        close(listenSocket);
        return;
    }

    while (true)
    {
        int client = accept(listenSocket, nullptr, nullptr);
        if (client < 0)
        {
            perror("Error accepting connection");
            continue;
        }
        std::thread t(handleclient, client, onMessageEvent);
        t.detach();
    }
    close(listenSocket);
}