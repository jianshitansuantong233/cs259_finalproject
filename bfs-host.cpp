#define DEBUG_ON

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <fstream>
#include <vector>

#include "graph.h"
#include "bfs.h"
#include "bfs-cpu.h"
#include "util.h"

constexpr nid_t PRINT_MAX_NODES = 10;
constexpr offset_t PRINT_MAX_EDGES = 30;

int main(int argc, char *argv[]) {
  // Load graph.
  std::ifstream ifs(argv[1]);
  auto edge_list = load_edgelist(ifs);

  DEBUG(
  if (edge_list.size() <= PRINT_MAX_EDGES) {
    std::cout << "Edge list (before rename):" << std::endl;
    for (auto &edge : edge_list)
      std::cout << edge.first << " " << edge.second << std::endl;
  });

  // Build push and pull graphs.
  PushGraph pushG;
  PullGraph pullG;

  // @Feiqian rename edge list.
  build_graphs(edge_list, &pushG, &pullG);

  // @Feiqian sort by parent node (i.e., edge (u, v) is sorted by ascending u).
  std::sort(edge_list.begin(), edge_list.end(), AscendingChildNode);  

  // Validate constructed edge list and graph.
  {
    // Validate edge list has been renamed.
    DEBUG(
    if (edge_list.size() <= PRINT_MAX_EDGES) {
      std::cout << "Edge list (after rename):" << std::endl;
      for (auto &edge : edge_list)
        std::cout << edge.first << " " << edge.second << std::endl;
    });

    // Validate push graph has been constructed correctly.
    DEBUG(
    if (pushG.num_nodes <= PRINT_MAX_NODES) {
      std::cout << "Push Graph" << std::endl
                << "- num nodes: " << pushG.num_nodes << std::endl
                << "- num edges: " << pushG.num_edges << std::endl;
      for (nid_t u = 0; u < pushG.num_nodes; u++) {
        std::cout << "- " << u << ": ";
        for (offset_t off = pushG.index[u]; off < pushG.index[u + 1]; off++) {
          std::cout << pushG.neighbors[off] << " ";
        }
        std::cout << std::endl;
      }
    });

    // Validate pull graph has been constructed correctly.
    DEBUG(
    if (pullG.num_nodes <= PRINT_MAX_NODES) {
      std::cout << "Pull Graph" << std::endl
                << "- num nodes: " << pullG.num_nodes << std::endl
                << "- num edges: " << pullG.num_edges << std::endl;
      for (nid_t u = 0; u < pullG.num_nodes; u++) {
        std::cout << "- " << u << ": ";
        for (offset_t off = pullG.index[u]; off < pullG.index[u + 1]; off++) {
          std::cout << pullG.neighbors[off] << " ";
        }
        std::cout << std::endl;
      }
    });
  }

  // Validate depths.
  {
    // Get validation depths for push.
    {
      std::vector<depth_t> depths(pushG.num_nodes, INVALID_DEPTH);
      nid_t start = 0; // Arbitrary (needs to be random in the future).
      bfs_cpu_push(pushG, start, depths);
      
      DEBUG(
      if (pushG.num_nodes <= PRINT_MAX_NODES) {
        for (nid_t u = 0; u < pushG.num_nodes; u++)
          std::cout << depths[u] << " ";
        std::cout << std::endl;
      });
    }

    // Get validation depths for pull.
    {
      std::vector<depth_t> depths(pushG.num_nodes, INVALID_DEPTH);
      nid_t start = 0; // Arbitrary (needs to be random in the future).
      bfs_cpu_pull(pullG, start, depths);
      
      DEBUG(
      if (pullG.num_nodes <= PRINT_MAX_NODES) {
        for (nid_t u = 0; u < pullG.num_nodes; u++)
          std::cout << depths[u] << " ";
        std::cout << std::endl;
      });
    }
  }

  return EXIT_SUCCESS;
}
