#include <map>
#include <queue>
#include <thread>
#include <iostream>
#include "io.h"
#include "dsstructs.h"
#include "serialization.cpp"
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <atomic>

//                get                      set                                                 listen
// Master     wait&reply   set_invalid->enque inmessages ->set_valid-> broadcast -> deque      enque inmessages ->set_valid-> broadcast
// slave      wait&reply   set_invalid->enque outmessages ->sendMessage to master -> deque     enque inmessages ->set_valid

template <typename T1, typename T2>
class ReplicatedKVS
{
private:
    bool batchmode = true;
    bool async = batchmode && (true);
    std::queue<std::vector<unsigned char> > inmessages;
    pthread_mutex_t inmessagebuffermutex;
    sem_t inmessagebuffer_semaphore;

    std::queue<std::vector<unsigned char> > outmessages;
    pthread_mutex_t outmessagebuffermutex;
    sem_t outmessagebuffer_semaphore;

    Node localnode;
    Node masternode;
    std::vector<Node> allnodes;
    std::map<T1, KVSvalue<T2> > kvmap;
    pthread_mutex_t keymutex;
    int listenfd;
    bool is_master;
    std::atomic<int> pendingupdates{0};
    int MAX_PENDING = 10;

public:
    ReplicatedKVS(const Node &localnode, const Node &masternode, const std::vector<Node> &allnodes) : localnode(localnode), masternode(masternode), allnodes(allnodes)
    {
        this->listenfd = -1;
        this->is_master = (this->masternode == this->localnode);
        pthread_mutex_init(&keymutex, nullptr);
        pthread_mutex_init(&inmessagebuffermutex, nullptr);
        pthread_mutex_init(&outmessagebuffermutex, nullptr);
        sem_init(&inmessagebuffer_semaphore, 0, 0);
        sem_init(&outmessagebuffer_semaphore, 0, 0);
        ReplicatedKVS<T1, T2>::listen(this);
        if(async){ReplicatedKVS<T1,T2>::sendupdates(this);}
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
        int trials = 40;
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
            std::thread t(listenthread,&(inputref->listenfd), inputref->localnode.port, f(inputref));
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
            std::thread t(listenthread,&(inputref->listenfd), inputref->localnode.port, f(inputref));
            t.detach();
        }
    }

    static void send_updates_slave(ReplicatedKVS<T1, T2> *ref)
    {
        // sem_wait(&ref->outmessagebuffer_semaphore);
        // sem_post(&ref->outmessagebuffer_semaphore);
        std::vector<std::vector<unsigned char>> removedmessages;
        pthread_mutex_lock(&ref->outmessagebuffermutex);
        while (!ref->outmessages.empty())
        {
            //sem_wait(&ref->outmessagebuffer_semaphore);
            ref->pendingupdates.fetch_sub(1);
            std::vector<unsigned char> element = ref->outmessages.front();
            removedmessages.push_back(element);
            ref->outmessages.pop();
        }
        pthread_mutex_unlock(&ref->outmessagebuffermutex);
        if (ref->batchmode)
        {
            sendMessage(collapseVectors(removedmessages), ref->masternode);
        }
        else
        {
            for (std::vector<unsigned char> messagebytes : removedmessages)
            {
                sendMessage(messagebytes, ref->masternode);
            }
        }
    }

    static void send_updates_master(ReplicatedKVS<T1, T2> *ref){
        // sem_wait(&ref->inmessagebuffer_semaphore);
        // sem_post(&ref->inmessagebuffer_semaphore);
        deque_set_broadcast(ref);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }


    static void sendupdatesthread(ReplicatedKVS<T1, T2> *ref)
    {
        if (ref->is_master)
        {
            while(true){
                send_updates_master(ref);
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }else{
            while(true){
                send_updates_slave(ref);
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
    }
    void sendupdates(ReplicatedKVS<T1, T2> *ref){
        std::thread t(sendupdatesthread, ref);
        t.detach();
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

    void stoplistening(){
        try
        {
           close(this->listenfd);
        }
        catch(const std::exception& e)
        {
            std::cerr << e.what() << '\n';
        }
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

    static void enqueInMessagebytes(ReplicatedKVS<T1, T2> *ref, const std::vector<unsigned char> &messagebytes)
    {
        pthread_mutex_lock(&ref->inmessagebuffermutex);
        ref->inmessages.push(messagebytes);
        ref->pendingupdates.fetch_add(1);
        pthread_mutex_unlock(&ref->inmessagebuffermutex);
        //sem_post(&ref->inmessagebuffer_semaphore);
    }
    static void enqueInMessage(ReplicatedKVS<T1, T2> *ref, const Message<T1, T2> &m)
    {
        enqueInMessagebytes(ref, tobytes(m));
    }
    static void enqueOutMessagebytes(ReplicatedKVS<T1, T2> *ref, const std::vector<unsigned char> &messagebytes)
    {
        pthread_mutex_lock(&ref->outmessagebuffermutex);
        ref->outmessages.push(messagebytes);
        ref->pendingupdates.fetch_add(1);
        pthread_mutex_unlock(&ref->outmessagebuffermutex);
        //sem_post(&ref->outmessagebuffer_semaphore);
    }
    static void enqueOutMessage(ReplicatedKVS<T1, T2> *ref, const Message<T1, T2> &m)
    {
        enqueOutMessagebytes(ref, tobytes(m));
    }

    //converts bytes into batch of messages and and updates the KV map
    static void updateKVbatch(ReplicatedKVS<T1, T2> *ref, const std::vector<unsigned char> &messagebytes)
    {
        std::vector< Message<T1,T2> > messagebatch = bytestovec< Message<T1,T2> >(messagebytes);
        for(const Message<T1, T2> message: messagebatch){
            set_valid(ref, message.key, message.value);
        }
    }
    static void updateKV(ReplicatedKVS<T1, T2> *ref, const std::vector<unsigned char> &messagebytes){
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
    template <typename T>
    static std::vector<T> collapseVectors(const std::vector<std::vector<T> > &vecOfVecs)
    {
        std::vector<T> collapsedVec;
        size_t totalSize = 0;
        for (const auto &vec : vecOfVecs)
        {
            totalSize += vec.size();
        }
        collapsedVec.reserve(totalSize);
        for (const auto &vec : vecOfVecs)
        {
            collapsedVec.insert(collapsedVec.end(), vec.begin(), vec.end());
        }
        return collapsedVec;
    }

    static std::vector<std::vector<unsigned char> > deque_andset_updates(ReplicatedKVS<T1, T2> *ref)
    {
        std::vector<std::vector<unsigned char> > removedmessages;
        pthread_mutex_lock(&ref->inmessagebuffermutex);
        while (!ref->inmessages.empty())
        {
            std::vector<unsigned char> element = ref->inmessages.front();
            if(ref->batchmode){
                updateKVbatch(ref,element);
            }else{
                updateKV(ref, element);
            }
            //sem_wait(&ref->inmessagebuffer_semaphore);
            ref->pendingupdates.fetch_sub(1);
            ref->inmessages.pop();
            removedmessages.push_back(element);
        }
        pthread_mutex_unlock(&ref->inmessagebuffermutex);
        return removedmessages;
    }

    static void deque_set_broadcast(ReplicatedKVS<T1, T2> *ref){
        std::vector<std::vector<unsigned char> > removedmessages = deque_andset_updates(ref);
        if(ref->batchmode){
            std::vector<unsigned char> removedmessagebytes = collapseVectors(removedmessages);
            broadcast(ref, removedmessagebytes);
        }else{
            for (int i = 0; i < removedmessages.size(); i++)
            {
                broadcast(ref, removedmessages[i]);
            }
        }
    }
    //**********************************   Master   **************************************/

    static void master_set(ReplicatedKVS<T1, T2> *ref, const T1 &key, const T2 &value)
    {
        set_invalid(ref, key, value);
        Message<T1, T2> m = {ref->uid(), key, value};
        enqueInMessage(ref, m);
        if(!ref->async) {
            deque_set_broadcast(ref);
        }
        else{
            if(ref->pendingupdates.load() > ref->MAX_PENDING){
                send_updates_master(ref);
            }
        }
    }
    static void master_listen(ReplicatedKVS<T1, T2> *ref, const std::vector<unsigned char> &newmessagebytes)
    {
        enqueInMessagebytes(ref, newmessagebytes);
        if(!ref->async){
            deque_set_broadcast(ref);
        }
        else{
            if(ref->pendingupdates.load() > ref->MAX_PENDING){
                send_updates_master(ref);
            }
        }
    }

    //**********************************   Slave   ************************************** /

    static void slave_set(ReplicatedKVS<T1, T2> *ref, const T1 &key, const T2 &value)
    {
        set_invalid(ref, key, value);
        Message<T1, T2> m = {ref->uid(), key, value};
        enqueOutMessage(ref,m);
        //sendMessage(tobytes(m), ref->masternode);
        if(!ref->async){
            send_updates_slave(ref);
        }
        else{
            if(ref->pendingupdates.load() > ref->MAX_PENDING){
                send_updates_slave(ref);
            }
        }

    }
    static void slave_listen(ReplicatedKVS<T1, T2> *ref, const std::vector<unsigned char> &newmessagebytes)
    {
        enqueInMessagebytes(ref, newmessagebytes);
        std::vector<std::vector<unsigned char> > removedmessages = deque_andset_updates(ref);
    }
};
