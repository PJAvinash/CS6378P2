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
    std::cout<<"getIPV4 before cast" <<"\n";
    struct in_addr *addr = reinterpret_cast<struct in_addr *>(hostInfo->h_addr);
    std::cout<<"getIPV4 after cast" <<"\n";
    return inet_ntoa(*addr);
}

int sendMessage(const std::vector<unsigned char> &bytes, const Node &destination)
{
    int client = socket(AF_INET, SOCK_STREAM, 0);
    printf("32 ");
    if (client < 0)
    {
        perror("Error creating socket");
        return -1;
    }
    struct sockaddr_in destAddr;
    destAddr.sin_family = AF_INET;
    destAddr.sin_port = htons(destination.port);
    printf("41 ");
    if (inet_pton(AF_INET, getIPV4(destination.hostname).c_str(), &destAddr.sin_addr) <= 0)
    {
        perror("Invalid address");
        close(client);
        return -1;
    }
    printf("48 ");
    if (connect(client, reinterpret_cast<struct sockaddr *>(&destAddr), sizeof(destAddr)) < 0)
    {
        perror("Error connecting");
        printf("host: %s port: %d", destination.hostname.c_str(), destination.port);
        close(client);
        return -1;
    }
    printf("56 ");
    if (send(client, bytes.data(), bytes.size(), 0) < 0)
    {
        perror("Error sending message");
        close(client);
        return -1;
    }
    printf("63 \n");
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