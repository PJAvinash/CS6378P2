#include "dsstructs.h"
#include "tobc.cpp"
int main(){
    Node n1 = {0, "JAYANTHs-MBP.lan", 5001};
    Node n2 = {1, "JAYANTHs-MBP.lan", 5002};
    Node n3 = {2, "JAYANTHs-MBP.lan", 5003};
    Node n4 = {2, "JAYANTHs-MBP.lan", 5004};
    std::vector<Node> nodes = {n1,n2,n3,n4};
    std::vector<Network> networkinstance(nodes.size());
    std::vector<ReplicatedKVS> relicatedKVS(nodes.size());



}