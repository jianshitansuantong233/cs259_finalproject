#define DEBUG_ON

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <tapa.h>

#include "graph.h"
#include "bfs.h"
#include "bfs-cpu.h"
#include "util.h"

constexpr nid_t    PRINT_MAX_NODES  = 10;
constexpr offset_t PRINT_MAX_EDGES  = 30;
constexpr nid_t    PRINT_MAX_ERRORS = 10;

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
    std::cout << "Push Graph" << std::endl
              << "- num nodes: " << pushG.num_nodes << std::endl
              << "- num edges: " << pushG.num_edges << std::endl;
    if (pushG.num_nodes <= PRINT_MAX_NODES) {
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
    std::cout << "Pull Graph" << std::endl
              << "- num nodes: " << pullG.num_nodes << std::endl
              << "- num edges: " << pullG.num_edges << std::endl;
    if (pullG.num_nodes <= PRINT_MAX_NODES) {
      for (nid_t u = 0; u < pullG.num_nodes; u++) {
        std::cout << "- " << u << ": ";
        for (offset_t off = pullG.index[u]; off < pullG.index[u + 1]; off++) {
          std::cout << pullG.neighbors[off] << " ";
        }
        std::cout << std::endl;
      }
    });
  }

  // Run and validate FPGA kernel.
  {
    nid_t start = 0; // Arbitrary.
    std::vector<depth_t> fpga_depths(pullG.num_nodes, INVALID_DEPTH);

    std::string bitstream;
    if (const auto bitstream_ptr = getenv("TAPAB")) {
      bitstream = bitstream_ptr;
    }

    tapa::invoke(
        bfs_fpga, bitstream, start, pushG.num_nodes,
        tapa::read_only_mmap<offset_t>(pushG.index),
        tapa::read_only_mmap<nid_t>(pushG.neighbors),
        tapa::read_only_mmap<offset_t>(pullG.index),
        tapa::read_only_mmap<nid_t>(pullG.neighbors),
        tapa::read_write_mmap<depth_t>(fpga_depths));

    DEBUG(
    if (pushG.num_nodes <= PRINT_MAX_NODES) {
      for (nid_t u = 0; u < pushG.num_nodes; u++)
        std::cout << fpga_depths[u] << " ";
      std::cout << std::endl;
    });

    std::vector<depth_t> validation_depths(pushG.num_nodes, INVALID_DEPTH);
    bfs_cpu_push(pushG, start, validation_depths);

    nid_t err_count = 0;
    for (nid_t u = 0; u < pushG.num_nodes; u++) {
      if (fpga_depths[u] != validation_depths[u]) {
        if (err_count < PRINT_MAX_ERRORS) {
          DEBUG(std::cout << "[error] node " << u << " fpga depth (" 
                          << fpga_depths[u] << ") != oracle depth ("
                          << validation_depths[u] << ")" << std::endl);
        }
        err_count++;
      }
    }
    if (err_count >= PRINT_MAX_ERRORS) {
      DEBUG(std::cout << "[error] ... " << std::endl
                      << "[error] and " << (err_count - PRINT_MAX_ERRORS)
                      << " more errors" << std::endl);
    }
    if (err_count != 0) return EXIT_FAILURE;
    else /* Success */  DEBUG(std::cout << "Validation success!" << std::endl);
  }

  return EXIT_SUCCESS;
}
