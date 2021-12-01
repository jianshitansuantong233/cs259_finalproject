#ifndef BFS_CPU_H
#define BFS_CPU_H

#include <vector>

#include "graph.h"

void bfs_cpu_push(const PushGraph &g, nid_t start, 
    std::vector<depth_t> &depths);
void bfs_cpu_pull(const PullGraph &g, nid_t start, 
    std::vector<depth_t> &depths);

#endif // BFS_CPU_H
