
The following is the implementation of Totally Ordered Braodcast in a distributed system (written in c++)


Algorithm: (Centralized algorithm for total order with 1 master)

We designate a node as master, (first in the config file)
Any update locally will be sent to master, master enques the update request
All the requests to master are broadcasted to all the nodes in the network.
This enforces total ordering on the data an well as causal ordering.

Here use total ordering for creating a Replicated In Memory keyvalue store. Due to delay in network, we will have some delay in the network but eventually we will end up up with the same key-value pairs on all nodes , this can be verified by print statment in the console.

Future plans: 
Reduce the delivery latency
Add support for nodes leaving and joining the network
Add support for node authentication
Add support for backups 



Compilation: 
g++ -std=c++14 -stdlib=libc++ io.cpp main.cpp serialization.cpp algorithm.cpp -o totalorder

Execution:
./totalorder <ConfigurationPath>

On a cluster of nodes:
go to /Launcher in terminal run 'chmod +x launcher.sh && ./launcher.sh config.txt' For cleaning up processes 'chmod +x cleanup.sh && ./cleanup.sh config.txt'











