#include <string>
#include <map>
#include "io.cpp"
#include "semaphore.h"
#include "dsstructs.h"

class Network
{
private:
    struct Node master;
    struct Node self;
    IO *ioobject;
    bool isMaster;
    std::vector<Node> nodes;
    std::queue<Message> updaterequests;

public:
    Network(const Node &self, const Node &master, const std::vector<Node> &nodes)
    {
        this->self = self;
        this->master = master;
        this->nodes = nodes;
        this->ioobject = new IO(self);
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
        while (true)
        {
            if (!updaterequests.empty())
            {
                Message m = updaterequests.front();
                this->broadcast(m);
                updaterequests.pop();
            }
            std::vector<std::vector<unsigned char>> newmessages = this->ioobject->getMessages();
            for (int i = 0; i < newmessages.size(); i++)
            {
                updaterequests.push(frombytes<Message>(newmessages[i]));
            }
        }
    }

    void updatemaster(Message &m)
    {
        if (this->isMaster)
        {
            updaterequests.push(m);
        }
        else
        {
            sendMessage(tobytes<Message>(m), master);
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
    Network &network;

public:
    ReplicatedKVS(Network &network) : network(network) {}

    KVSvalue get(std::string &key)
    {
        std::map<std::string, KVSvalue>::iterator it = kvmap.find(key);
        if (it == kvmap.end())
        {
            KVSvalue kv = {"", false};
            return kv;
        }
        return it->second;
    }

    void set(std::string &key, std::string &value)
    {
        KVSvalue newval = {value, false};
        this->kvmap[key] = newval;
        Message m = {this->network.uid(), key, value};
        this->network.updatemaster(m);
    }
    void datasyncthread()
    {
        while (true)
        {
            std::vector<Message> newupdates = this->network.getupdates();
            for (int i = 0; i < newupdates.size(); i++)
            {
                KVSvalue value = {newupdates[i].value, true};
                this->kvmap[newupdates[i].key] = value;
            }
        }
    }
};