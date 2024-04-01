
#include <random>
#include <set>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include "lib.h"

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

void test(const std::vector<Node> &nodes, size_t num_keys)
{
    try
    {
        printf("#\n");
        std::vector<ReplicatedKVS<int, int> *> replicatedKVS;
        std::string host = getHostname();
        for (size_t i = 0; i < nodes.size(); i++)
        {

            if (getHostname() == nodes[i].hostname)
            {
                ReplicatedKVS<int, int> *rkv = new ReplicatedKVS<int, int>(nodes[i], nodes[0], nodes);
                replicatedKVS.push_back(rkv);
                std::cout << rkv->uid() << std::endl;
            }
        }
        size_t num_nodes = replicatedKVS.size();
        std::cout << host << " num_nodes: " << num_nodes << "\n";
        sleep(2);
        for (size_t i = 0; i < num_keys; i++)
        {
            int replicauid = (i % num_nodes);
            int uid = replicatedKVS[replicauid]->uid();
            replicatedKVS[replicauid]->set(i, i * (uid + 1));
        }
        printf("#\n");
        for (size_t i = 0; i < replicatedKVS.size(); i++)
        {
            std::cout << replicatedKVS[i]->uid() << std::endl;
        }
        printf("#\n");
        int mismatches = 0;
        for (size_t i = 0; i < num_keys; i++)
        {
            printf("key : %d\n", (int)i);
            std::set<int> s;
            for (size_t r = 0; r < num_nodes; r++)
            {
                try
                {
                    int localcopy = replicatedKVS[r]->get(i);
                    s.insert(localcopy);
                }
                catch (const std::exception &e)
                {
                    std::cout << host << " Error at i = " << i << "\n";
                    std::cerr << host << e.what() << '\n';
                }
            }
            if (s.size() > 1)
            {
                printSet(s);
                mismatches++;
            }
        }
        printf("#\n");
        for (size_t i = 0; i < replicatedKVS.size(); i++)
        {
            std::vector<int> keys = replicatedKVS[i]->getkeys();
            //replicatedKVS[i]->stoplistening();
            for (int k : keys)
            {
                std::cout << "uid: " << replicatedKVS[i]->uid() << " k: " << k << " v: " << replicatedKVS[i]->get(k) << "\n";
            }
        }
        printf("%s: Number mismatches for %d keys: %d\n", host.c_str(), (int)num_keys, mismatches);
        sleep(2);
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
    }
}

std::vector<Node> readNodesFromFile(const std::string &filename)
{
    std::vector<Node> nodes;
    std::ifstream file(filename);

    if (!file.is_open())
    {
        throw std::runtime_error("Error: Unable to open file " + filename);
    }

    std::string line;
    while (std::getline(file, line))
    {
        // Ignore comments and empty lines
        if (line.empty() || line[0] == '#')
        {
            continue;
        }

        std::istringstream iss(line);
        int uid;
        std::string hostname;
        int port;

        // Extract and validate data
        if (!(iss >> uid >> hostname >> port))
        {
            std::cerr << "Error: Invalid line format in file " << filename << std::endl;
            continue;
        }

        // Basic port validation (adjust as needed)
        if (port <= 0 || port > 65535)
        {
            std::cerr << "Error: Invalid port number in file " << filename << std::endl;
            continue;
        }

        nodes.push_back(Node{uid, hostname, port});
    }
    file.close();
    return nodes;
}

int main(int argc, char *argv[])
{
    if (argc == 3)
    {
        std::vector<Node> nodes = readNodesFromFile(argv[1]);
        size_t num_keys = (size_t)atoi(argv[2]);
        test(nodes, num_keys);
    }
    else
    {
        perror("Invalid number of arguments format:'<executable> <config path> <num_keys>'");
        exit(1);
    }
    exit(0);
}