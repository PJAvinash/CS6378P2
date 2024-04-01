#ifndef ALGORITHM_H
#define ALGORITHM_H

#include <map>
#include <queue>
#include <thread>
#include <iostream>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <atomic>
#include "dsstructs.h"

template <typename T1, typename T2>
class ReplicatedKVS
{
public:
    bool batchmode = true;
    bool async = (batchmode)&& true;
private:
    std::queue<std::vector<unsigned char>> inmessages;
    pthread_mutex_t inmessagebuffermutex;
    // sem_t inmessagebuffer_semaphore;

    std::queue<std::vector<unsigned char>> outmessages;
    pthread_mutex_t outmessagebuffermutex;
    // sem_t outmessagebuffer_semaphore;

    Node localnode;
    Node masternode;
    std::vector<Node> allnodes;
    std::map<T1, KVSvalue<T2>> kvmap;
    pthread_mutex_t keymutex;
    bool is_master;
    std::atomic<int> pendingupdates;
    int MAX_PENDING;

public:
    ReplicatedKVS(const Node &localnode, const Node &masternode, const std::vector<Node> &allnodes);
    void set(const T1 &key, const T2 &value);
    typename std::map<T1, KVSvalue<T2>>::iterator find(const T1 &key);
    T2 get(const T1 &key);

    void listen(ReplicatedKVS<T1, T2> *inputref);
    void sendupdates(ReplicatedKVS<T1, T2> *ref);
    bool keyexists(const T1 &key);
    std::vector<T1> getkeys();
    int uid();
    void stoplistening();
private:
    static void send_updates_slave(ReplicatedKVS<T1, T2> *ref);
    static void send_updates_master(ReplicatedKVS<T1, T2> *ref);
    static void sendupdatesthread(ReplicatedKVS<T1, T2> *ref);
    static void set_invalid(ReplicatedKVS<T1, T2> *ref, const T1 &key, const T2 &value);
    static void set_valid(ReplicatedKVS<T1, T2> *ref, const T1 &key, const T2 &value);
    static void enqueInMessagebytes(ReplicatedKVS<T1, T2> *ref, const std::vector<unsigned char> &messagebytes);
    static void enqueInMessage(ReplicatedKVS<T1, T2> *ref, const Message<T1, T2> &m);
    static void enqueOutMessagebytes(ReplicatedKVS<T1, T2> *ref, const std::vector<unsigned char> &messagebytes);
    static void enqueOutMessage(ReplicatedKVS<T1, T2> *ref, const Message<T1, T2> &m);
    static void updateKVbatch(ReplicatedKVS<T1, T2> *ref, const std::vector<unsigned char> &messagebytes);
    static void updateKV(ReplicatedKVS<T1, T2> *ref, const std::vector<unsigned char> &messagebytes);
    static void broadcast(ReplicatedKVS<T1, T2> *ref, const std::vector<unsigned char> &m);
    static std::vector< std::vector<unsigned char> > deque_andset_updates(ReplicatedKVS<T1, T2> *ref);
    static void deque_set_broadcast(ReplicatedKVS<T1, T2> *ref);
    static void master_set(ReplicatedKVS<T1, T2> *ref, const T1 &key, const T2 &value);
    static void master_listen(ReplicatedKVS<T1, T2> *ref, const std::vector<unsigned char> &newmessagebytes);
    static void slave_set(ReplicatedKVS<T1, T2> *ref, const T1 &key, const T2 &value);
    static void slave_listen(ReplicatedKVS<T1, T2> *ref, const std::vector<unsigned char> &newmessagebytes);
};

#include "../src/algorithm.cpp"
#endif //ALGORITHM_H