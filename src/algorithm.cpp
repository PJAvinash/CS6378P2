#ifndef _ALGORITHM_CPP
#define _ALGORITHM_CPP

#include <map>
#include <queue>
#include <thread>
#include <iostream>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <atomic>
#include "lib.h"

//                get                      set                                                 listen
// Master     wait&reply   set_invalid->enque inmessages ->set_valid-> broadcast -> deque      enque inmessages ->set_valid-> broadcast
// slave      wait&reply   set_invalid->enque outmessages ->sendMessage to master -> deque     enque inmessages ->set_valid

template <typename T1, typename T2>
ReplicatedKVS<T1, T2>::ReplicatedKVS(const Node &localnode, const Node &masternode, const std::vector<Node> &allnodes) : localnode(localnode), masternode(masternode), allnodes(allnodes)
{
    this->is_master = (this->masternode == this->localnode);
    pthread_mutex_init(&keymutex, nullptr);
    pthread_mutex_init(&inmessagebuffermutex, nullptr);
    pthread_mutex_init(&outmessagebuffermutex, nullptr);
    // sem_init(&inmessagebuffer_semaphore, 0, 0);
    // sem_init(&outmessagebuffer_semaphore, 0, 0);
    ReplicatedKVS<T1, T2>::listen(this);
    if (async)
    {
        ReplicatedKVS<T1, T2>::sendupdates(this);
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

template <typename T1, typename T2>
void ReplicatedKVS<T1, T2>::set(const T1 &key, const T2 &value)
{
    if (this->is_master)
    {
        ReplicatedKVS<T1, T2>::master_set(this, key, value);
    }
    else
    {
        ReplicatedKVS<T1, T2>::slave_set(this, key, value);
    }
}

template <typename T1, typename T2>
typename std::map<T1, KVSvalue<T2>>::iterator ReplicatedKVS<T1, T2>::find(const T1 &key)
{
    typename std::map<T1, KVSvalue<T2>>::iterator it = this->kvmap.find(key);
    return it;
}

template <typename T1, typename T2>
T2 ReplicatedKVS<T1, T2>::get(const T1 &key)
{
    typename std::map<T1, KVSvalue<T2>>::iterator it = this->kvmap.find(key);
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

template <typename T1, typename T2>
void ReplicatedKVS<T1, T2>::listen(ReplicatedKVS<T1, T2> *inputref)
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

template <typename T1, typename T2>
void ReplicatedKVS<T1, T2>::send_updates_slave(ReplicatedKVS<T1, T2> *ref)
{
    // sem_wait(&ref->outmessagebuffer_semaphore);
    // sem_post(&ref->outmessagebuffer_semaphore);
    std::vector<std::vector<unsigned char>> removedmessages;
    pthread_mutex_lock(&ref->outmessagebuffermutex);
    while (!ref->outmessages.empty())
    {
        // sem_wait(&ref->outmessagebuffer_semaphore);
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

template <typename T1, typename T2>
void ReplicatedKVS<T1, T2>::send_updates_master(ReplicatedKVS<T1, T2> *ref)
{
    ReplicatedKVS<T1, T2>::deque_set_broadcast(ref);
}

template <typename T1, typename T2>
void ReplicatedKVS<T1, T2>::sendupdatesthread(ReplicatedKVS<T1, T2> *ref)
{
    if (ref->is_master)
    {
        while (true)
        {
            ReplicatedKVS<T1, T2>::send_updates_master(ref);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    else
    {
        while (true)
        {
            ReplicatedKVS<T1, T2>::send_updates_slave(ref);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}

template <typename T1, typename T2>
void ReplicatedKVS<T1, T2>::sendupdates(ReplicatedKVS<T1, T2> *ref)
{
    std::thread t(ReplicatedKVS<T1, T2>::sendupdatesthread, ref);
    t.detach();
}

template <typename T1, typename T2>
bool ReplicatedKVS<T1, T2>::keyexists(const T1 &key)
{
    return (this->kvmap.find(key) != this->kvmap.end());
}

template <typename T1, typename T2>
std::vector<T1> ReplicatedKVS<T1, T2>::getkeys()
{
    std::vector<T1> keys;
    for (typename std::map<T1, KVSvalue<T2>>::iterator it = this->kvmap.begin(); it != this->kvmap.end(); it++)
    {
        keys.push_back(it->first);
    }
    return keys;
}

template <typename T1, typename T2>
int ReplicatedKVS<T1, T2>::uid()
{
    return this->localnode.uid;
}

template <typename T1, typename T2>
void ReplicatedKVS<T1, T2>::stoplistening()
{
    try
    {
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
    }
}

//********************************** common *****************************************/

template <typename T1, typename T2>
void ReplicatedKVS<T1, T2>::set_invalid(ReplicatedKVS<T1, T2> *ref, const T1 &key, const T2 &value)
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

template <typename T1, typename T2>
void ReplicatedKVS<T1, T2>::set_valid(ReplicatedKVS<T1, T2> *ref, const T1 &key, const T2 &value)
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
}

template <typename T1, typename T2>
void ReplicatedKVS<T1, T2>::enqueInMessagebytes(ReplicatedKVS<T1, T2> *ref, const std::vector<unsigned char> &messagebytes)
{
    pthread_mutex_lock(&ref->inmessagebuffermutex);
    ref->inmessages.push(messagebytes);
    ref->pendingupdates.fetch_add(1);
    pthread_mutex_unlock(&ref->inmessagebuffermutex);
    // sem_post(&ref->inmessagebuffer_semaphore);
}

template <typename T1, typename T2>
void ReplicatedKVS<T1, T2>::enqueInMessage(ReplicatedKVS<T1, T2> *ref, const Message<T1, T2> &m)
{
    ReplicatedKVS<T1, T2>::enqueInMessagebytes(ref, tobytes(m));
}

template <typename T1, typename T2>
void ReplicatedKVS<T1, T2>::enqueOutMessagebytes(ReplicatedKVS<T1, T2> *ref, const std::vector<unsigned char> &messagebytes)
{
    pthread_mutex_lock(&ref->outmessagebuffermutex);
    ref->outmessages.push(messagebytes);
    ref->pendingupdates.fetch_add(1);
    pthread_mutex_unlock(&ref->outmessagebuffermutex);
    // sem_post(&ref->outmessagebuffer_semaphore);
}

template <typename T1, typename T2>
void ReplicatedKVS<T1, T2>::enqueOutMessage(ReplicatedKVS<T1, T2> *ref, const Message<T1, T2> &m)
{
    ReplicatedKVS<T1, T2>::enqueOutMessagebytes(ref, tobytes(m));
}

// converts bytes into batch of messages and and updates the KV map
template <typename T1, typename T2>
void ReplicatedKVS<T1, T2>::updateKVbatch(ReplicatedKVS<T1, T2> *ref, const std::vector<unsigned char> &messagebytes)
{
    std::vector<Message<T1, T2>> messagebatch = bytestovec<Message<T1, T2>>(messagebytes);
    for (const Message<T1, T2> message : messagebatch)
    {
        ReplicatedKVS<T1, T2>::set_valid(ref, message.key, message.value);
    }
}

template <typename T1, typename T2>
void ReplicatedKVS<T1, T2>::updateKV(ReplicatedKVS<T1, T2> *ref, const std::vector<unsigned char> &messagebytes)
{
    Message<T1, T2> message = frombytes<Message<T1, T2>>(messagebytes);
    ReplicatedKVS<T1, T2>::set_valid(ref, message.key, message.value);
}

template <typename T1, typename T2>
void ReplicatedKVS<T1, T2>::broadcast(ReplicatedKVS<T1, T2> *ref, const std::vector<unsigned char> &m)
{
    for (size_t i = 0; i < ref->allnodes.size(); i++)
    {
        if (!(ref->allnodes[i] == ref->localnode))
        {
            sendMessage(m, ref->allnodes[i]);
        }
    }
}

template <typename T1, typename T2>
std::vector<std::vector<unsigned char> > ReplicatedKVS <T1, T2>::deque_andset_updates(ReplicatedKVS<T1, T2> *ref)
{
    std::vector<std::vector<unsigned char>> removedmessages; 
    pthread_mutex_lock(&ref->inmessagebuffermutex);
    while (!ref->inmessages.empty())
    {
        std::vector<unsigned char> element = ref->inmessages.front();
        if (ref->batchmode)
        {
            ReplicatedKVS<T1, T2>::updateKVbatch(ref, element);
        }
        else
        {
            ReplicatedKVS<T1, T2>::updateKV(ref, element);
        }
        // sem_wait(&ref->inmessagebuffer_semaphore);
        ref->pendingupdates.fetch_sub(1);
        ref->inmessages.pop();
        removedmessages.push_back(element);
    }
    pthread_mutex_unlock(&ref->inmessagebuffermutex);
    return removedmessages;
}

template <typename T1, typename T2>
void ReplicatedKVS <T1, T2>::deque_set_broadcast(ReplicatedKVS<T1, T2> *ref)
{
    std::vector<std::vector<unsigned char>> removedmessages = ReplicatedKVS<T1, T2>::deque_andset_updates(ref);
    if (ref->batchmode)
    {
        std::vector<unsigned char> removedmessagebytes = collapseVectors(removedmessages);
        broadcast(ref, removedmessagebytes);
    }
    else
    {
        for (size_t i = 0; i < removedmessages.size(); i++){
            broadcast(ref, removedmessages[i]);
        }
    }
}
//**********************************   Master   **************************************/
template <typename T1, typename T2>
void ReplicatedKVS <T1, T2>::master_set(ReplicatedKVS<T1, T2> *ref, const T1 &key, const T2 &value)
{
    set_invalid(ref, key, value);
    Message<T1, T2> m = {ref->uid(), key, value};
    enqueInMessage(ref, m);
    if (!ref->async)
    {
        ReplicatedKVS<T1, T2>::deque_set_broadcast(ref);
    }
    else
    {
        if (ref->pendingupdates.load() > ref->MAX_PENDING)
        {
            ReplicatedKVS<T1, T2>::send_updates_master(ref);
        }
    }
}

template <typename T1, typename T2>
void ReplicatedKVS <T1, T2>::master_listen(ReplicatedKVS<T1, T2> *ref, const std::vector<unsigned char> &newmessagebytes)
{
    enqueInMessagebytes(ref, newmessagebytes);
    if (!ref->async)
    {
        ReplicatedKVS<T1, T2>::deque_set_broadcast(ref);
    }
    else
    {
        if (ref->pendingupdates.load() > ref->MAX_PENDING)
        {
            ReplicatedKVS<T1, T2>::send_updates_master(ref);
        }
    }
}

//**********************************   Slave   ************************************** /
template <typename T1, typename T2>
void ReplicatedKVS <T1, T2>::slave_set(ReplicatedKVS<T1, T2> *ref, const T1 &key, const T2 &value)
{
    set_invalid(ref, key, value);
    Message<T1, T2> m = {ref->uid(), key, value};
    ReplicatedKVS<T1, T2>::enqueOutMessage(ref, m);
    // sendMessage(tobytes(m), ref->masternode);
    if (!ref->async)
    {
        ReplicatedKVS<T1, T2>::send_updates_slave(ref);
    }
    else
    {
        if (ref->pendingupdates.load() > ref->MAX_PENDING)
        {
            ReplicatedKVS<T1, T2>::send_updates_slave(ref);
        }
    }
}

template <typename T1, typename T2>
void ReplicatedKVS <T1, T2>::slave_listen(ReplicatedKVS<T1, T2> *ref, const std::vector<unsigned char> &newmessagebytes)
{
    ReplicatedKVS<T1, T2>::enqueInMessagebytes(ref, newmessagebytes);
    std::vector<std::vector<unsigned char>> removedmessages = ReplicatedKVS<T1, T2>::deque_andset_updates(ref);
}

#endif