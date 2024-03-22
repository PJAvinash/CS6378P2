#include <string>
#include <vector>
#include <queue>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>
#include <iostream>
#include <netdb.h>
#include <pthread.h>
#include <semaphore.h>
#include "dsstructs.h"
#include "serialization.h"

static std::string getIPV4(const std::string &hostname)
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

static int sendMessage(const std::vector<unsigned char> &bytes, const Node &destination)
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

    if (connect(client, reinterpret_cast<struct sockaddr *>(&destAddr), sizeof(destAddr)) < 0)
    {
        perror("Error connecting");
        printf("host: %s port: %d", destination.hostname.c_str(), destination.port);
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

static void handleclient(int client, std::queue<std::vector<unsigned char>> &messagesqueue, pthread_mutex_t &mutexlock, sem_t *new_messages, pthread_cond_t *message_cond)
{
    std::vector<unsigned char> buffer(1024);
    int bytesReceived = recv(client, buffer.data(), buffer.size(), 0);
    if (bytesReceived < 0)
    {
        perror("Error receiving data");
        close(client);
    }

    std::vector<unsigned char> bytestream(buffer.begin(), buffer.begin() + bytesReceived);
    pthread_mutex_lock(&mutexlock);
    messagesqueue.push(bytestream);
    pthread_mutex_unlock(&mutexlock);
    sem_post(new_messages);
    pthread_cond_signal(message_cond); // Notify waiting threads
    close(client);
}

static void listenthread(int port, std::queue<std::vector<unsigned char>> &messagesqueue, pthread_mutex_t &mutexlock, sem_t *new_messages, pthread_cond_t *message_cond)
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
        std::thread t(handleclient, client, std::ref(messagesqueue), std::ref(mutexlock), new_messages, message_cond);
        t.detach();
    }
    close(listenSocket);
}


class IO
{
private:
    Node self;
    std::queue<std::vector<unsigned char>> messages;
    pthread_mutex_t mutex;
    sem_t new_messages;
    pthread_cond_t message_cond;

public:
    IO(const Node &self) : self(self)
    {
        pthread_mutex_init(&mutex, nullptr);
        sem_init(&new_messages, 0, 0);
        pthread_cond_init(&message_cond, nullptr);
        listenMessages();
    }

    ~IO()
    {
        pthread_mutex_destroy(&mutex);
        sem_destroy(&new_messages);
        pthread_cond_destroy(&message_cond);
    }

    void listenMessages()
    {
        std::thread t(&IO::listenthread, this, self.port);
        t.detach();
    }

    std::vector<std::vector<unsigned char>> getMessages()
    {
        std::vector<std::vector<unsigned char>> newmessages;
        pthread_mutex_lock(&mutex);
        while (messages.empty())
        {
            pthread_cond_wait(&message_cond, &mutex);
        }
        while (!messages.empty())
        {
            std::vector<unsigned char> element = messages.front();
            messages.pop();
            newmessages.push_back(element);
        }
        pthread_mutex_unlock(&mutex);
        return newmessages;
    }

private:
    void listenthread(int port)
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
            std::thread t(&IO::handleclient, this, client);
            t.detach();
        }
        close(listenSocket);
    }

    void handleclient(int client)
    {
        std::vector<unsigned char> buffer(1024);
        int bytesReceived = recv(client, buffer.data(), buffer.size(), 0);
        if (bytesReceived < 0)
        {
            perror("Error receiving data");
            close(client);
            return;
        }

        std::vector<unsigned char> bytestream(buffer.begin(), buffer.begin() + bytesReceived);
        pthread_mutex_lock(&mutex);
        messages.push(bytestream);
        pthread_mutex_unlock(&mutex);
        pthread_cond_signal(&message_cond);
        sem_post(&new_messages);
        close(client);
    }
};

// std::vector<unsigned char> stringToVector(const char *str)
// {
//     std::vector<unsigned char> result;
//     for (int i = 0; str[i] != '\0'; ++i)
//     {
//         result.push_back(static_cast<unsigned char>(str[i]));
//     }
//     return result;
// }

// int main()
// {
//     Node n1 = {0, "JAYANTHs-MBP.lan", 5007};
//     Node n2 = {1, "JAYANTHs-MBP.lan", 5004};
//     Node n3 = {2, "JAYANTHs-MBP.lan", 5005};
//     printf("0");
//     IO io1(n1);
//     IO io2(n2);
//     IO io3(n3);
//     printf("1");
//     io1.listenMessages();
//     printf("2");
//     sendMessage(stringToVector("HiHIIIII"), n1);
//     sendMessage(stringToVector("haha"), n1);
//     printf("3");
//     int k = 1;
//     while(true){
//         std::vector < std::vector<unsigned char> >v = io1.getMessages();
//         k = v.size();
//         for(auto t:v){
//             std::string s = std::string(t.begin(), t.end());
//             std::cout << s << std::endl;
//         }
//     }
// }