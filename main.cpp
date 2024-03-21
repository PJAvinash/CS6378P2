#include "dsstructs.h"
#include "algorithm.cpp"
#include <random>
#include <set>
#include "serialization.h"

void printSet(const std::set<std::string>& s) {
    std::cout << "Set contents:" << std::endl;
    for (const auto& element : s) {
        std::cout << element << " ";
    }
    std::cout <<"\n";
}

void test()
{
    std::random_device rd;
    std::mt19937 gen(rd());
    int lower_bound = 1;
    int upper_bound = 100000;
    // Create a distribution
    std::uniform_int_distribution<> distr(lower_bound, upper_bound);

    // Generate a random number within the specified range
    int random_number = distr(gen);
    Node n1 = {0, "JAYANTHs-MBP.lan", 6001};
    Node n2 = {1, "JAYANTHs-MBP.lan", 6002};
    Node n3 = {2, "JAYANTHs-MBP.lan", 6003};
    Node n4 = {3, "JAYANTHs-MBP.lan", 6004};
    std::vector<Node> nodes;
    nodes.push_back(n1);
    nodes.push_back(n2);
    nodes.push_back(n3);
    nodes.push_back(n4);
    std::vector<ReplicatedKVS> relicatedKVS;
    for (int i = 0; i < nodes.size(); i++)
    {
        Network nw = Network(nodes[i], n1, nodes);
        relicatedKVS.push_back(ReplicatedKVS(nw));
    }
    int num_nodes = nodes.size();
    int num_keys = 10;
    for (int i = 0; i < num_keys; i++)
    {
        std::string key = std::to_string(i);
        std::string value = std::to_string(distr(gen));
        int replicauid = (distr(gen) % num_nodes);
        relicatedKVS[replicauid].set(key, value);
    }

    // testing

    int mismatches = 0;
    for (int i = 0; i < num_keys; i++)
    {
        std::set<std::string> s;
        std::string key = std::to_string(i);
        for (int r = 0; r < num_nodes; r++)
        {
            std::string localcopy = relicatedKVS[r].get(key).value;
            std::cout <<localcopy << "-";
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

int main()
{
    test();
    return 0;
}