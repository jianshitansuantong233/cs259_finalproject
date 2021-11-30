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
  CSRGraph csrG;
  CSCGraph cscG;

  build_graphs(edge_list, &csrG, &cscG);

  // Validation.
  if (csrG.num_nodes <= PRINT_MAX_NODES) {
    std::cout << "CSR Graph" << std::endl
              << "- num nodes: " << csrG.num_nodes << std::endl
              << "- num edges: " << csrG.num_edges << std::endl;
    for (nid_t u = 0; u < csrG.num_nodes; u++) {
      std::cout << "- " << u << ": ";
      for (offset_t off = csrG.index[u]; off < csrG.index[u + 1]; off++) {
        std::cout << csrG.neighbors[off] << " ";
      }
      std::cout << std::endl;
    }
  }

  if (cscG.num_nodes <= PRINT_MAX_NODES) {
    std::cout << "CSC Graph" << std::endl
              << "- num nodes: " << cscG.num_nodes << std::endl
              << "- num edges: " << cscG.num_edges << std::endl;
    for (nid_t u = 0; u < cscG.num_nodes; u++) {
      std::cout << "- " << u << ": ";
      for (offset_t off = cscG.index[u]; off < cscG.index[u + 1]; off++) {
        std::cout << cscG.neighbors[off] << " ";
      }
      std::cout << std::endl;
    }
  }

  // Get validation depths.
  depth_t *depths = new depth_t[csrG.num_nodes];
  for (nid_t u = 0; u < csrG.num_nodes; u++)
    depths[u] = INVALID_DEPTH;

  nid_t start = 0; // Arbitrary (needs to be random in the future).
  bfs_cpu(&csrG, start, depths);
  
  if (csrG.num_nodes <= PRINT_MAX_NODES) {
    for (nid_t u = 0; u < csrG.num_nodes; u++)
      std::cout << depths[u] << " ";
    std::cout << std::endl;
  }

  return EXIT_SUCCESS;
}
