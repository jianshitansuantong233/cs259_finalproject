#include <cstdint>
#include <iostream>
#include <fstream>

#include "graph.h"
#include "bfs-cpu.h"

constexpr nid_t PRINT_MAX_NODES = 10;
constexpr offset_t PRINT_MAX_EDGES = 30;

int main(int argc, char *argv[]) {
  // Load graph.
  std::ifstream ifs(argv[1]);
  auto edge_list = load_edgelist(ifs);

  if (edge_list.size() <= PRINT_MAX_EDGES) {
    for (auto &edge : edge_list)
      std::cout << edge.first << " " << edge.second << std::endl;
  }

  // Bulid CSR (push) and CSC (pull) graphs.
  PushGraph pushG;
  PullGraph pullG;

  build_graphs(edge_list, &pushG, &pullG);

  // Validation.
  if (pushG.num_nodes <= PRINT_MAX_NODES) {
    std::cout << "CSR Graph" << std::endl
              << "- num nodes: " << pushG.num_nodes << std::endl
              << "- num edges: " << pushG.num_edges << std::endl;
    for (nid_t u = 0; u < pushG.num_nodes; u++) {
      std::cout << "- " << u << ": ";
      for (offset_t off = pushG.index[u]; off < pushG.index[u + 1]; off++) {
        std::cout << pushG.neighbors[off] << " ";
      }
      std::cout << std::endl;
    }
  }

  if (pullG.num_nodes <= PRINT_MAX_NODES) {
    std::cout << "CSC Graph" << std::endl
              << "- num nodes: " << pullG.num_nodes << std::endl
              << "- num edges: " << pullG.num_edges << std::endl;
    for (nid_t u = 0; u < pullG.num_nodes; u++) {
      std::cout << "- " << u << ": ";
      for (offset_t off = pullG.index[u]; off < pullG.index[u + 1]; off++) {
        std::cout << pullG.neighbors[off] << " ";
      }
      std::cout << std::endl;
    }
  }

  // Get validation depths.
  depth_t *depths = new depth_t[pushG.num_nodes];
  for (nid_t u = 0; u < pushG.num_nodes; u++)
    depths[u] = INVALID_DEPTH;

  nid_t start = 0; // Arbitrary (needs to be random in the future).
  bfs_cpu(&pushG, start, depths);
  
  if (pushG.num_nodes <= PRINT_MAX_NODES) {
    for (nid_t u = 0; u < pushG.num_nodes; u++)
      std::cout << depths[u] << " ";
    std::cout << std::endl;
  }

  return EXIT_SUCCESS;
}
