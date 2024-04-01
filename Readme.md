# Distributed In Memory Key-Value store with Total Order Broadcast

This project implements a centralized algorithm for achieving total order with a single master in a distributed system. The algorithm is used to create a Replicated In-Memory KeyValue Store. The primary goal is to ensure total ordering of data and enforce causal ordering in a distributed environment and enable high availability of data for local reads

## Algorithm Overview

1. **Designation of Master Node:**
   - The first node in the configuration file is designated as the master node.

2. **Update Propagation:**
   - Any update made locally on a node is sent to the master node.
   - The master node enqueues the update request.

3. **Broadcasting:**
   - All update requests sent to the master node are broadcasted to all nodes in the network.
   - This ensures total ordering of updates and maintains causal consistency across all nodes.


# Future plans:
Reduce the delivery latency
Add support for different message types (CRUD) opetations
Add support for concurrent reads and writes with finegrained synchronization
Add support for nodes leaving and joining the network
Add support for node authentication
Add support for backups 

# Compilation:
make 
(or) g++ -std=c++11 -stdlib=libc++ io.cpp main.cpp serialization.cpp algorithm.cpp -o totalorder

# Execution:
./totalorder <ConfigurationPath> <Numer of keys>

On a cluster of nodes:
go to /Launcher in terminal run 'chmod +x launcher.sh && ./launcher.sh <ConfigurationPath> <Numer of keys> ' For cleaning up processes 'chmod +x cleanup.sh && ./cleanup.sh <ConfigurationPath>'

# Features
- 1.supports batch updates to handle TCP/IP connections overhead
- 2.supports asynchrouns updates

# Contributors
PJ Avinash









