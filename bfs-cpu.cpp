#include "bfs-cpu.h"

#include <queue>

/**
 * Performs BFS push serially on CPU (single threaded).
 * Parameters:
 *   - G      <- push graph.
 *   - start  <- start node ID.
 *   - depths <- depths array (must all be initialized to INVALID_DEPTH).
 */
void bfs_cpu_push(const PushGraph &g, nid_t start, 
    std::vector<depth_t> &depths
) {
  depths[start] = 0;
  std::queue<nid_t> frontier;
  frontier.push(start);

  while (not frontier.empty()) {
    auto u = frontier.front();
    frontier.pop();

    for (offset_t off = g.index[u]; off < g.index[u + 1]; off++) {
      auto v = g.neighbors[off];

      // If unexplored, update.
      if (depths[v] == INVALID_DEPTH) {
        depths[v] = depths[u] + 1;
        frontier.push(v);
      }
    }
  }
}

/**
 * Performs BFS pull serially on CPU (single threaded).
 * Parameters:
 *   - G      <- pull graph.
 *   - start  <- start node ID.
 *   - depths <- depths array (must all be initialized to INVALID_DEPTH).
 */
void bfs_cpu_pull(const PullGraph &g, nid_t start, 
    std::vector<depth_t> &depths
) {
  depths[start] = 0;
  std::queue<nid_t> frontier;
  frontier.push(start);

  while (!frontier.empty()) {
    auto n = frontier.front();
    frontier.pop();

    // traverse all the nodes in the graph and update the ones whose parents is n.
    for (nid_t u = 0; u < g.num_nodes; u++) {
      if (depths[u] != INVALID_DEPTH) continue;
      for (offset_t off = g.index[u]; off < g.index[u + 1]; off++) {
        auto v = g.neighbors[off];

        if (v == n) {
          depths[u] = depths[v] + 1;
          frontier.push(u);
          break;
        }
      }
    }
  }
}

