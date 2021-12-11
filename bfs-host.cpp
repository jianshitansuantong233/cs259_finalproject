#define DEBUG_ON
#define EDGE_CENTRIC
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <tapa.h>

#include "graph.h"
#include "bfs-fpga.h"
#include "bfs-cpu.h"
#include "util.h"

constexpr int      ALPHA            = 15;
constexpr int      BETA             = 18;

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
  std::sort(edge_list.begin(), edge_list.end(), AscendingParentNode);  

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
#ifdef VERTEX_CENTRIC
  // Run and validate FPGA kernel.
  {
    nid_t start_nid = pullG.num_nodes / 8; // Arbitrary.
    std::vector<depth_t> fpga_depths(pullG.num_nodes, INVALID_DEPTH);

    std::string bitstream;
    if (const auto bitstream_ptr = getenv("TAPAB")) {
      bitstream = bitstream_ptr;
    }

#ifdef CACHE_STATS
    std::vector<offset_t> stats(2, 0);
#endif // CACHE_STATS
    tapa::invoke(
        bfs_fpga, bitstream, 
        start_nid, pushG.num_nodes, 
        tapa::read_only_mmap<offset_t>(pushG.index),
        tapa::read_only_mmap<nid_t>(pushG.neighbors),
        tapa::read_write_mmap<depth_t>(fpga_depths));

    //auto start_degree = pushG.index[start_nid + 1] - pushG.index[start_nid];
    //tapa::invoke(
        //bfs_fpga, bitstream, 
        //start_nid, start_degree, pushG.num_nodes, pushG.num_edges,
        //tapa::read_only_mmap<offset_t>(pushG.index),
        //tapa::read_only_mmap<nid_t>(pushG.neighbors),
        //tapa::read_only_mmap<offset_t>(pullG.index),
        //tapa::read_only_mmap<nid_t>(pullG.neighbors),
        //tapa::read_write_mmap<depth_t>(fpga_depths),
//#ifdef CACHE_STATS
        //tapa::read_write_mmap<nid_t>(stats),
//#endif // CACHE_STATS
        //ALPHA, BETA
        //);

#ifdef CACHE_STATS
    std::cout << "Cache hits: " << stats[0] << " misses: " << stats[1]
              << std::endl;
#endif // CACHE_STATS

    DEBUG(
    if (pushG.num_nodes <= PRINT_MAX_NODES) {
      for (nid_t u = 0; u < pushG.num_nodes; u++)
        std::cout << fpga_depths[u] << " ";
      std::cout << std::endl;
    });

    std::vector<depth_t> validation_depths(pushG.num_nodes, INVALID_DEPTH);
    bfs_cpu_push(pushG, start_nid, validation_depths);

    nid_t err_count = 0;
    for (nid_t u = 0; u < pushG.num_nodes; u++) {
      if (fpga_depths[u] != validation_depths[u]) {
        if (err_count < PRINT_MAX_ERRORS) {
          std::cerr << "[error] node " << u << " fpga depth (" 
                    << fpga_depths[u] << ") != oracle depth ("
                    << validation_depths[u] << ")" << std::endl;
        }
        err_count++;
      }
    }
    if (err_count >= PRINT_MAX_ERRORS) {
      std::cerr << "[error] ... " << std::endl
                << "[error] and " << (err_count - PRINT_MAX_ERRORS)
                << " more errors" << std::endl;
    }
    if (err_count != 0) return EXIT_FAILURE;
    else /* Success */  std::cout << "Validation success!" << std::endl;
  }
#else
  {
    nid_t start_nid = pullG.num_nodes / 8;
    std::vector<Edge> e;
    for(int i=0;i<edge_list.size();i++){
      Edge temp;
      temp.src = edge_list[i].first;
      temp.dst = edge_list[i].second;
      e.push_back(temp);
    }
    std::vector<VertexAttr> vertices;    
    // Start partition process
    int size_of_graph = edge_list.size();
    for(int i=0;i<size_of_graph;i++){
      VertexAttr source;
      VertexAttr dest;
      source = e[i].src;
      dest = e[i].dst;
      vertices.push_back(source);
      vertices.push_back(dest);
    }
    std::sort(vertices.begin(), vertices.end());  
    for(int i=1;i!=vertices.size();i++){
      if(vertices[i] == vertices[i-1]){
        vertices.erase(vertices.begin()+i);
        i--;
      }
    }
    for(int i=0;i!=vertices.size();i++){
      vertices[i]=0xFFFFFFFF;
    }
    vertices[pullG.num_nodes / 8] = 0;/*start index*/
    std::vector<uint32_t> num_edges;
    std::vector<uint32_t> edge_offsets;
    edge_offsets.push_back(0);
    for(int i=1;i!=size_of_graph;i++){
      if(e[i].src!=e[i-1].src){
        edge_offsets.push_back(i);
      }
    }
    for(int i=1;i<edge_offsets.size();i++){
      num_edges.push_back(edge_offsets[i]-edge_offsets[i-1]);
    }
    num_edges.push_back(size_of_graph-edge_offsets[edge_offsets.size()-1]);
    std::string bitstream;
    if (const auto bitstream_ptr = getenv("TAPAB")) {
      bitstream = bitstream_ptr;
    }
    tapa::invoke(
        bfs_fpga_edge, bitstream, vertices.size(), pullG.num_nodes / 8 /*start index*/, tapa::read_only_mmap<const Eid>(num_edges),
        tapa::read_only_mmap<const Eid>(edge_offsets), tapa::read_write_mmap<VertexAttr>(vertices), tapa::read_only_mmap<Edge>(e).reinterpret<bits<Edge>>());

    DEBUG(
    if (pushG.num_nodes <= PRINT_MAX_NODES) {
      for (nid_t u = 0; u < pushG.num_nodes; u++)
        std::cout << vertices[u] << " ";
      std::cout << std::endl;
    });

    std::vector<depth_t> validation_depths(pushG.num_nodes, INVALID_DEPTH);
    bfs_cpu_push(pushG, start_nid, validation_depths);

    nid_t err_count = 0;
    for (nid_t u = 0; u < pushG.num_nodes; u++) {
      if (vertices[u] != validation_depths[u]) {
        if (err_count < PRINT_MAX_ERRORS) {
          DEBUG(std::cout << "[error] node " << u << " fpga depth (" 
                          << vertices[u] << ") != oracle depth ("
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
#endif

  return EXIT_SUCCESS;
}
