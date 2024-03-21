#ifndef DSSTRUCTS_H
#define DSSTRUCTS_H
#include <string>
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
struct Message
{
    int from;
    std::string key;
    std::string value;
};

struct KVSvalue
{
    std::string value;
    bool valid;
};
#endif // DSSTRUCTS_H
