#ifndef DSSTRUCTS_H
#define DSSTRUCTS_H
#include <string>
#include <atomic>
struct Node
{
    int uid;
    std::string hostname;
    int port;
    bool operator==(const Node& other) const
    {
        return (uid == other.uid) && (hostname == other.hostname) && (port == other.port);
    }
};
enum
{
    BROADCAST,
    UPDATE,
    NEWJOIN
};

template <typename T1,typename T2>
struct Message
{
    int from;
    T1 key;
    T2 value;
};

template<typename T1>
struct KVSvalue
{
    T1 value;
    std::atomic<bool> valid;
    KVSvalue() : value(T1()), valid(false) {}
    KVSvalue(const KVSvalue<T1>& other) : value(other.value), valid(other.valid.load()) {}
    KVSvalue<T1>& operator=(const KVSvalue<T1>& other) {
        if (this != &other) {
            value = other.value;
            valid.store(other.valid.load());
        }
        return *this;
    }
};
#endif // DSSTRUCTS_H
