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
#include <chrono>

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
    // if (inet_pton(AF_INET, getIPV4(destination.hostname).c_str(), &destAddr.sin_addr) <= 0)
    // {
    //     perror("Invalid address");
    //     close(client);
    //     return -1;
    // }

    struct hostent *host = gethostbyname(destination.hostname.c_str());
    if (host == nullptr)
    {
        perror("Failed to resolve hostname");
        close(client);
        return -1;
    }
    memcpy(&destAddr.sin_addr, host->h_addr, host->h_length);
    int NUM_ATTEMPTS = 10;
    for (int attempt = 0; attempt < NUM_ATTEMPTS; attempt++)
    {
        if (connect(client, reinterpret_cast<struct sockaddr *>(&destAddr), sizeof(destAddr)) < 0)
        {
            int sleepDuration = 1 << attempt;
            std::this_thread::sleep_for(std::chrono::milliseconds(sleepDuration));
            if(NUM_ATTEMPTS == (attempt+1)){
                perror("Error connecting");
                close(client);
                return -1;
            }   
        }else{
            break;
        }
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

void listenthread(int* socketdecsriptor,int port, std::function<void(std::vector<unsigned char>)> onMessageEvent)
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