#ifndef GRAPH_H
#define GRAPH_H

#include <cstdint>
#include <istream>
#include <vector>

// Base types.
using nid_t       = uint32_t;
using offset_t    = nid_t;
using edge_t      = std::pair<nid_t, nid_t>;
using edge_list_t = std::vector<edge_t>;
using depth_t     = int;

// Invalid depth.
constexpr depth_t INVALID_DEPTH = -1;

/**
 * Compressed graph format.
 * For a node u, it's neighbors' range is defined by 
 * [index[u], index[u + 1]).
 * To access the neighbors, use a loop similar to this
 * for off = index[u] until index[u + 1]: // Exclusive
 *    v = neighbors[off]
 */
struct CompressedGraph {
  std::vector<offset_t> index;     // offset array
  std::vector<nid_t>    neighbors; // edge array 
  nid_t                 num_nodes;
  offset_t              num_edges;
};

// Compressed graph formats.
using PushGraph = CompressedGraph;
using PullGraph = CompressedGraph;

/**
 * Loads in edge list from input stream.
 */
edge_list_t load_edgelist(std::istream &in);

/*
 * Implementing using tapa, uncomment these
#include <tapa.h>
void stream_ordered_edges(edge_list_t e, tapa::ostream<bits<Edge>> s){
  for(int i=0;i<e.length();i++){
    Edge temp;
    temp.src = e[i].first;
    temp.dst = e[i].second;
    s.write(temp);
  }
}

*/
/**
 * Constructs CSR and CSC graphs from an edge list.
 * Parmeters:
 *   - edge_list <- graph edge list (remap edge list too).
 *   - pushG     <- pointer to push graph.
 *   - pullG     <- pointer to pull graph.
 */
void build_graphs(edge_list_t &edge_list, 
    PushGraph * const pushG, PullGraph * const pullG);

// Sort functions.
struct {
  bool operator()(const edge_t &e1, const edge_t &e2) const {
    return e1.first < e2.first or 
      (e1.first == e2.first and e1.second < e2.second);
  }
} AscendingParentNode;

struct {
  bool operator()(const edge_t &e1, const edge_t &e2) const {
    return e1.second < e2.second or
      (e1.second == e2.second and e1.first < e2.first);
  }
} AscendingChildNode;

#endif // GRAPH_H
