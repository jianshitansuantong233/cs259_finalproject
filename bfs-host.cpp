#include <cstdint>
#include <iostream>
#include <fstream>

#include "graph.h"

int main(int argc, char *argv[]) {
  // Load graph.
  std::ifstream ifs(argv[1]);
  auto edge_list = load_edgelist(ifs);

  for (auto &edge : edge_list)
    std::cout << edge.first << " " << edge.second << std::endl;

  // Bulid CSR (push) and CSC (pull) graphs.
  CSRGraph csrG;
  CSCGraph cscG;

  build_graphs(edge_list, &csrG, &cscG);

  // Validation.
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

  return EXIT_SUCCESS;
}
