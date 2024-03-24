#include <map>
#include <queue>
#include <thread>
#include <iostream>
#include "io.h"
#include "dsstructs.h"
#include "serialization.cpp"

//                get                      set                                listen
// Master     wait&reply   set_invalid->enque->set_valid-> broadcast    enque->set_valid-> broadcast
// slave      wait&reply   set_invalid->sendMessage to master           enque->set_valid

template <typename T1, typename T2>
class ReplicatedKVS
{
private:
    std::queue<std::vector<unsigned char> > inmessages;
    Node localnode;
    Node masternode;
    std::vector<Node> allnodes;
    std::map<T1, KVSvalue<T2> > kvmap;
    pthread_mutex_t keymutex;
    pthread_mutex_t messagebuffermutex;
    bool is_master;

public:
    ReplicatedKVS(const Node &localnode, const Node &masternode, const std::vector<Node> &allnodes) : localnode(localnode), masternode(masternode), allnodes(allnodes)
    {
        this->is_master = (this->masternode == this->localnode);
        pthread_mutex_init(&keymutex, nullptr);
        pthread_mutex_init(&messagebuffermutex, nullptr);
        ReplicatedKVS<T1, T2>::listen(this);
    }

    void set(const T1 &key, const T2 &value)
    {
        if (this->is_master)
        {
            master_set(this, key, value);
        }
        else
        {
            slave_set(this, key, value);
        }
    }

    typename std::map<T1, KVSvalue<T2> >::iterator find(const T1 &key)
    {
        typename std::map<T1, KVSvalue<T2> >::iterator it = kvmap.find(key);
        return it;
    }

    T2 get(const T1 &key)
    {
        typename std::map<T1, KVSvalue<T2> >::iterator it = this->kvmap.find(key);
        int trials = 15;
        while (it == this->kvmap.end() && (trials--))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            it = this->kvmap.find(key);
        }
        if (it == kvmap.end())
        {
            throw std::runtime_error("Key not found");
        }
        while (!it->second.valid.load())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            it = this->kvmap.find(key);
        }
        return it->second.value;
    }

    void listen(ReplicatedKVS<T1, T2> *inputref)
    {
        if (inputref->is_master)
        {
            auto f = [](ReplicatedKVS<T1, T2> *ref)
            {
                return [=](const std::vector<unsigned char> &newmessagebytes)
                {
                    master_listen(ref, newmessagebytes);
                };
            };
            std::thread t(listenthread, inputref->localnode.port, f(inputref));
            t.detach();
        }
        else
        {
            auto f = [](ReplicatedKVS<T1, T2> *ref)
            {
                return [=](const std::vector<unsigned char> &newmessagebytes)
                {
                    slave_listen(ref, newmessagebytes);
                };
            };
            std::thread t(listenthread, inputref->localnode.port, f(inputref));
            t.detach();
        }
    }

    bool keyexists(const T1 &key){
        return (this->kvmap.find(key) != this->kvmap.end());
    }

    std:: vector <T1> getkeys(){
        std:: vector <T1> keys;
        for(typename std::map<T1, KVSvalue<T2> >::iterator it = this->kvmap.begin(); it != this->kvmap.end(); it++){
            keys.push_back(it->first);
        }
        return keys;
    }

    int uid()
    {
        return this->localnode.uid;
    }
    //********************************** common *****************************************/
    static void set_invalid(ReplicatedKVS<T1, T2> *ref, const T1 &key, const T2 &value)
    {
        pthread_mutex_lock(&ref->keymutex);
        typename std::map<T1, KVSvalue<T2> >::iterator it = ref->kvmap.find(key);
        if (it == ref->kvmap.end())
        {
            KVSvalue<T2> newval;
            newval.value = value;
            newval.valid.store(false);
            ref->kvmap[key] = newval;
        }
        else
        {
            it->second.value = value;
            it->second.valid.store(false);
        }
        pthread_mutex_unlock(&ref->keymutex);
    }
    static void set_valid(ReplicatedKVS<T1, T2> *ref, const T1 &key, const T2 &value)
    {
        pthread_mutex_lock(&ref->keymutex);
        typename std::map<T1, KVSvalue<T2> >::iterator it = ref->kvmap.find(key);
        if (it == ref->kvmap.end())
        {
            KVSvalue<T2> newval;
            newval.value = value;
            newval.valid.store(true);
            ref->kvmap[key] = newval;
        }
        else
        {
            it->second.value = value;
            it->second.valid.store(true);
        }
        pthread_mutex_unlock(&ref->keymutex);
    }

    static void enqueMessagebytes(ReplicatedKVS<T1, T2> *ref, const std::vector<unsigned char> &messagebytes)
    {
        pthread_mutex_lock(&ref->messagebuffermutex);
        ref->inmessages.push(messagebytes);
        pthread_mutex_unlock(&ref->messagebuffermutex);
    }
    static void enqueMessage(ReplicatedKVS<T1, T2> *ref, const Message<T1, T2> &m)
    {
        enqueMessagebytes(ref, tobytes(m));
    }

    static void updateKV(ReplicatedKVS<T1, T2> *ref, const std::vector<unsigned char> &messagebytes)
    {
        Message<T1, T2> message = frombytes<Message<T1, T2> >(messagebytes);
        set_valid(ref, message.key, message.value);
    }

    static void broadcast(ReplicatedKVS<T1, T2> *ref, const std::vector<unsigned char> &m)
    {
        for (int i = 0; i < ref->allnodes.size(); i++)
        {
            if (!(ref->allnodes[i] == ref->localnode))
            {
                sendMessage(m, ref->allnodes[i]);
            }
        }
    }
    static std::vector<std::vector<unsigned char> > deque_andset_updates(ReplicatedKVS<T1, T2> *ref)
    {
        std::vector<std::vector<unsigned char> > removedmessages;
        pthread_mutex_lock(&ref->messagebuffermutex);
        while (!ref->inmessages.empty())
        {
            std::vector<unsigned char> element = ref->inmessages.front();
            updateKV(ref, element);
            ref->inmessages.pop();
            removedmessages.push_back(element);
        }
        pthread_mutex_unlock(&ref->messagebuffermutex);
        return removedmessages;
    }
    //**********************************   Master   **************************************/

    static void master_set(ReplicatedKVS<T1, T2> *ref, const T1 &key, const T2 &value)
    {
        set_invalid(ref, key, value);
        Message<T1, T2> m = {ref->uid(), key, value};
        enqueMessage(ref, m);
        std::vector<std::vector<unsigned char> > removedmessages = deque_andset_updates(ref);
        for (int i = 0; i < removedmessages.size(); i++)
        {
            broadcast(ref, removedmessages[i]);
        }
    }
    static void master_listen(ReplicatedKVS<T1, T2> *ref, const std::vector<unsigned char> &newmessagebytes)
    {
        enqueMessagebytes(ref, newmessagebytes);
        std::vector<std::vector<unsigned char> > removedmessages = deque_andset_updates(ref);
        for (int i = 0; i < removedmessages.size(); i++)
        {
            broadcast(ref, removedmessages[i]);
        }
    }

    //**********************************   Slave   ************************************** /

    static void slave_set(ReplicatedKVS<T1, T2> *ref, const T1 &key, const T2 &value)
    {
        set_invalid(ref, key, value);
        Message<T1, T2> m = {ref->uid(), key, value};
        sendMessage(tobytes(m), ref->masternode);
    }
    static void slave_listen(ReplicatedKVS<T1, T2> *ref, const std::vector<unsigned char> &newmessagebytes)
    {
        Message<T1, T2> m = frombytes<Message<T1, T2> >(newmessagebytes);
        enqueMessagebytes(ref, newmessagebytes);
        std::vector<std::vector<unsigned char> > removedmessages = deque_andset_updates(ref);
    }
};
