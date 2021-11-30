#ifndef BFS_CPU_PULL_H
#define BFS_CPU_PULL_H

#include <queue>

#include "graph.h"

void bfs_cpu_pull(PullGraph* const G, nid_t start, depth_t* depths) {
  depths[start] = 0;
  std::queue<nid_t> frontier;
  frontier.push(start);

  while (!frontier.empty()) {
    auto n = frontier.front();
    frontier.pop();

    // traverse all the nodes in the graph and update the ones whose parents is n.
    for (nid_t u = 0; u < G->num_nodes; u++) {
      if (depths[u] != -1) continue;
      for (offset_t off = G->index[u]; off < G->index[u + 1]; off++) {
        auto v = G->neighbors[off];

        if (v == n) {
          depths[u] = depths[v] + 1;
          frontier.push(u);
          break;
        }
      }
    }
  }
}

#endif  // BFS_CPU_PULL_H
