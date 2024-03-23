#include "dsstructs.h"
#include "serialization.h"
#include "algorithm.cpp"
#include <random>
#include <set>

#include <fstream>
#include <unistd.h>

template <typename T>
void printSet(const std::set<T> &s)
{
    std::cout << "Set contents:" << std::endl;
    for (const auto &element : s)
    {
        std::cout << element << " ";
    }
    std::cout << "\n";
}

std::string getHostname()
{
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0)
    {
        return std::string(hostname);
    }
    else
    {
        return "";
    }
}

void test0(const std::vector<Node> &nodes)
{
    std::vector< ReplicatedKVS<int, int> > replicatedKVS;
    for (int i = 0; i < nodes.size(); i++)
    {
        ReplicatedKVS<int, int> rkv(nodes[i], nodes[0], nodes);
        replicatedKVS.push_back(rkv);
        std::cout << rkv.uid() << std::endl;
    }
    int num_nodes = nodes.size();
    int num_keys = 16;
    for (int i = 0; i < num_keys; i++)
    {
        int replicauid = (i % num_nodes);
        std::cout << "replicauid" << replicauid << std::endl;
        replicatedKVS[replicauid].set(i, i*2);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10000));
    int mismatches = 0;
    for (int i = 0; i < num_keys; i++)
    {
        std::set<int> s;
        std::string key = std::to_string(i);
        for (int r = 0; r < num_nodes; r++)
        {
            int localcopy = replicatedKVS[r].get(i);
            std::cout << localcopy << "-";
            s.insert(localcopy);
        }
        if (s.size() > 1)
        {
            printSet(s);
            mismatches++;
        }
    }
    printf("Number mismatches for %d keys: %d\n", num_keys, mismatches);
}

// void test2(const std::vector<Node> &nodes)
// {
//     std::random_device rd;
//     std::mt19937 gen(rd());
//     int lower_bound = 1;
//     int upper_bound = 100000;
//     std::uniform_int_distribution<> distr(lower_bound, upper_bound);
//     int random_number = distr(gen);
//     std::vector<ReplicatedKVS> relicatedKVS;
//     for (int i = 0; i < nodes.size(); i++)
//     {
//         if (getHostname() == nodes[i].hostname)
//         {
//             Network *nw = new Network(nodes[i], nodes[0], nodes);
//             ReplicatedKVS rkv(nw);
//             relicatedKVS.push_back(rkv);
//             std::cout << rkv.uid() << std::endl;
//         }
//     }
//     int num_local_replicas = relicatedKVS.size();
//     int num_keys = 100;
//     for (int i = 0; i < num_keys; i++)
//     {
//         std::string key = std::to_string(i);
//         std::string value = std::to_string(distr(gen));
//         relicatedKVS[(i%num_local_replicas)].set(key, value);
//     }
//     std::this_thread::sleep_for(std::chrono::milliseconds(10000));

//     for (int i = 0; i < num_keys; i++)
//     {
//         std::set<std::string> s;
//         std::string key = std::to_string(i);
//         for (int r = 0; r < num_local_replicas; r++)
//         {
//             std::string localcopy = relicatedKVS[r].get(key).value;
//             std::cout<<"id:" << relicatedKVS[r].uid() << "k:" <<key << "v:"<<localcopy<<std::endl;
//         }
//     }
// }

std::vector<Node> readNodesFromFile(const std::string &filename)
{
    std::vector<Node> nodes;
    std::ifstream file(filename);
    if (!file.is_open())
    {
        std::cerr << "Error: Unable to open file " << filename << std::endl;
        return nodes;
    }

    std::string line;
    while (std::getline(file, line))
    {
        // Ignore lines starting with #
        if (line.empty() || line[0] == '#')
        {
            continue;
        }
        std::istringstream iss(line);
        int uid;
        std::string hostname;
        int port;
        if (!(iss >> uid >> hostname >> port))
        {
            std::cerr << "Error: Invalid line format in file " << filename << std::endl;
            continue;
        }
        nodes.push_back(Node{uid, hostname, port});
    }

    file.close();
    return nodes;
}

int main(int argc, char *argv[])
{
    std::vector<Node> nodes = readNodesFromFile(argv[1]);
    test0(nodes);
    return 0;
}