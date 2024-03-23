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
public:
    Node localnode;
    Node masternode;
    std::vector<Node> allnodes;
    std::map<T1, KVSvalue<T2>> kvmap;

    pthread_mutex_t keymutex;
    pthread_mutex_t messagebuffermutex;
    std::queue<std::vector<unsigned char>> inmessages;
    bool is_master;

public:
    ReplicatedKVS(const Node &localnode, const Node &masternode, const std::vector<Node> &allnodes)
    {
        this->localnode = localnode;
        this->masternode = masternode;
        this->allnodes = allnodes;
        this->is_master = (masternode == localnode);
        pthread_mutex_init(&keymutex, nullptr);
        pthread_mutex_init(&messagebuffermutex, nullptr);
        this->listen();
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

    typename std::map<T1, KVSvalue<T2>>::iterator find(const T1 &key)
    {
        typename std::map<T1, KVSvalue<T2>>::iterator it = kvmap.find(key);
        return it;
    }

    T2 get(const T1 &key)
    {
        typename std::map<T1, KVSvalue<T2>>::iterator it;
        while (true)
        {
            pthread_mutex_lock(&this->keymutex);
            it = kvmap.find(key);
            pthread_mutex_unlock(&this->keymutex);
            if (it == kvmap.end())
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                it = kvmap.find(key);
                if (it == kvmap.end()){
                    throw std::runtime_error("Key not found");
                }  
            }
            if (it->second.valid.load())
            {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return it->second.value;
    }

    void listen()
    {
        if (this->is_master)
        {
            auto master_listen_lambda = [this](const std::vector<unsigned char> &newmessagebytes)
            {
                master_listen(this, newmessagebytes);
            };
            std::thread t(listenthread, this->localnode.port, master_listen_lambda);
            t.detach();
        }
        else
        {
            auto slave_listen_lambda = [this](const std::vector<unsigned char> &newmessagebytes)
            {
                slave_listen(this, newmessagebytes);
            };
            std::thread t(listenthread, this->localnode.port, slave_listen_lambda);
            t.detach();
        }
    }

    int uid()
    {
        return this->localnode.uid;
    }
    //********************************** common *****************************************/
    static void set_invalid(ReplicatedKVS<T1, T2> *ref, const T1 &key, const T2 &value)
    {
        pthread_mutex_lock(&ref->keymutex);
        typename std::map<T1, KVSvalue<T2>>::iterator it = ref->kvmap.find(key);
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
        typename std::map<T1, KVSvalue<T2>>::iterator it = ref->kvmap.find(key);
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

        std::cout << "k" << key << " " << ref->kvmap[key].value  <<std::endl;
    }

    static void enqueMessagebytes(ReplicatedKVS<T1, T2> *ref, const std::vector<unsigned char> &messagebytes)
    {

        // // if(ref->uid() == 0){
        //     Message<T1, T2> m = frombytes< Message<T1, T2> >(messagebytes);
        //     std::cout<< m.from << " " << m.value <<std::endl;
        // // }
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
        Message<T1, T2> message = frombytes<Message<T1, T2>>(messagebytes);
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
    static std::vector<std::vector<unsigned char>> deque_andset_updates(ReplicatedKVS<T1, T2> *ref)
    {
        std::vector<std::vector<unsigned char>> removedmessages;
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
        std::vector<std::vector<unsigned char>> removedmessages = deque_andset_updates(ref);
        for (int i = 0; i < removedmessages.size(); i++)
        {
            broadcast(ref, removedmessages[i]);
        }
    }
    static void master_listen(ReplicatedKVS<T1, T2> *ref, const std::vector<unsigned char> &newmessagebytes)
    {
        // Message <T1,T2> m = frombytes < Message <T1,T2> > (newmessagebytes);
        // std::cout << "master_listen" << m.from << std::endl;
        enqueMessagebytes(ref, newmessagebytes);
        std::vector<std::vector<unsigned char>> removedmessages = deque_andset_updates(ref);
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
        // Message <T1,T2> m = frombytes < Message <T1,T2> > (newmessagebytes);
        // std::cout << "slave_listen" << m.from << std::endl;
        enqueMessagebytes(ref, newmessagebytes);
        std::vector<std::vector<unsigned char>> removedmessages = deque_andset_updates(ref);
    }
};
