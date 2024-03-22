#include <string>
#include <map>
#include "io.cpp"
#include "semaphore.h"
#include "dsstructs.h"
#include "serialization.cpp"
#include <chrono>

class Network
{
private:
    struct Node master;
    struct Node self;
    IO *ioobject;
    bool isMaster;
    std::vector<Node> nodes;
    std::queue<Message> updaterequests;
    pthread_mutex_t mastermutex;
    sem_t new_messages;

public:
    Network(const Node &self, const Node &master, const std::vector<Node> &nodes)
    {
        this->self = self;
        this->master = master;
        this->nodes = nodes;
        this->ioobject = new IO(self);
        pthread_mutex_init(&mastermutex, nullptr);
        sem_init(&new_messages, 0, 0);
        if (self == master)
        {
            this->isMaster = true;
            std::thread t(&Network::masterbroadcastthread, this);
            t.detach();
        }
        else
        {
            this->isMaster = false;
        }
    }

    void broadcast(Message &m)
    {
        for (int i = 0; i < this->nodes.size(); i++)
        {
            if (!(this->nodes[i] == self))
            {
                sendMessage(tobytes(m), this->nodes[i]);
            }
        }
    }

    void masterbroadcastthread()
    {
        std::cout << "inside master broadcast uid::" << master.uid << std::endl;
        while (true)
        {
            std::vector<std::vector<unsigned char>> newmessages = this->ioobject->getMessages();
            // std::cout << ("after getMessages:" + newmessages.size()) << std::endl;
            if (newmessages.size() > 0)
            {
                pthread_mutex_lock(&mastermutex);
                for (int i = 0; i < newmessages.size(); i++)
                {
                    Message m = frombytes<Message>(newmessages[i]);
                    // std::cout << ("message from "+ m.from) <<std::endl;
                    updaterequests.push(m);
                }
                while (!updaterequests.empty())
                {
                    Message m = updaterequests.front();
                    this->broadcast(m);
                    updaterequests.pop();
                }
                pthread_mutex_unlock(&mastermutex);
            }
            else
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
    }

    void updatemaster(Message &m)
    {
        if (this->isMaster)
        {
            pthread_mutex_lock(&mastermutex);
            updaterequests.push(m);
            pthread_mutex_unlock(&mastermutex);
        }
        else
        {
            sendMessage(tobytes<Message>(m), master);
            std::cout << "sending from " << m.from << std::endl;
        }
    }
    int uid()
    {
        return this->self.uid;
    }
    std::vector<Message> getupdates()
    {
        std::vector<std::vector<unsigned char>> newmessages = this->ioobject->getMessages();
        std::vector<Message> rv(newmessages.size());
        for (int i = 0; i < newmessages.size(); i++)
        {
            rv.push_back(frombytes<Message>(newmessages[i]));
        }
        return rv;
    }
};


class ReplicatedKVS
{
private:
    std::map<std::string, KVSvalue> kvmap;
    Network *network;
    sem_t new_messages;
    pthread_mutex_t mutex;

public:
    ReplicatedKVS(Network *network) : network(network)
    {
        sem_init(&new_messages, 0, 0);
        pthread_mutex_init(&mutex, nullptr);
        std::thread t(&ReplicatedKVS::datasyncthread, this);
        t.detach();
    }

    ~ReplicatedKVS()
    {
        sem_destroy(&new_messages);
        pthread_mutex_destroy(&mutex);
    }

    KVSvalue get(std::string &key)
    {
        pthread_mutex_lock(&mutex);
        std::map<std::string, KVSvalue>::iterator it = kvmap.find(key);
        pthread_mutex_unlock(&mutex);
        
        if (it == kvmap.end())
        {
            KVSvalue kv = {"*", false};
            return kv;
        }

        while (!it->second.valid)
        {
            sem_wait(&new_messages);
            pthread_mutex_lock(&mutex);
            it = kvmap.find(key);
            pthread_mutex_unlock(&mutex);
            // std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        std::cout << "get " << std::endl;
        return it->second;
    }

    void set(std::string &key, std::string &value)
    {
        KVSvalue newval = {value, false};
        pthread_mutex_lock(&mutex);
        this->kvmap[key] = newval;
        pthread_mutex_unlock(&mutex);

        Message m = {this->network->uid(), key, value};
        this->network->updatemaster(m);
        std::cout << ("message sent" + m.value) << std::endl;
    }

    void datasyncthread()
    {
        while (true)
        {
            std::vector<Message> newupdates = this->network->getupdates();
            // std::cout << "datasyncthread" << newupdates.size() << std::endl;
            if (newupdates.size() > 0)
            {
                for (int i = 0; i < newupdates.size(); i++)
                {
                    KVSvalue value = {newupdates[i].value, true};
                    pthread_mutex_lock(&mutex);
                    this->kvmap[newupdates[i].key] = value;
                    pthread_mutex_unlock(&mutex);
                    sem_post(&new_messages);
                }
            }
            else
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
    }

    int uid()
    {
        return this->network->uid();
    }
};
