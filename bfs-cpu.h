#ifndef BFS_CPU_H
#define BFS_CPU_H

#include <queue>

#include "graph.h"

/**
 * Performs BFS push serially on CPU (single threaded).
 * Parameters:
 *   - G <- CSR graph (push).
 *   - start <- start node ID.
 *   - depths <- depths array (must all be initialized to INVALID_DEPTH).
 */
void bfs_cpu(CSRGraph * const G, nid_t start, depth_t * const depths) {
  depths[start] = 0;
  std::queue<nid_t> frontier;
  frontier.push(start);

  while (not frontier.empty()) {
    auto u = frontier.front();
    frontier.pop();

    for (offset_t off = G->index[u]; off < G->index[u + 1]; off++) {
      auto v = G->neighbors[off];

      // If unexplored, update.
      if (depths[v] == INVALID_DEPTH) {
        depths[v] = depths[u] + 1;
        frontier.push(v);
      }
    }
  }
}

#endif // BFS_CPU_H
