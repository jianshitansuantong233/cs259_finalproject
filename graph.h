#ifndef GRAPH_H
#define GRAPH_H

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>
#include <unordered_map>

using nid_t = uint32_t;
using offset_t = nid_t;
using edge_t = std::pair<nid_t, nid_t>;
using edge_list_t = std::vector<edge_t>;

struct CompressedGraph {
  offset_t *index;
  nid_t *neighbors;
  nid_t num_nodes;
  offset_t num_edges;
};

using CSRGraph = CompressedGraph;
using CSCGraph = CompressedGraph;

edge_list_t load_edgelist(std::ifstream &in) {
  edge_list_t edge_list;
  nid_t u, v;
  
  while (in >> u >> v)
    edge_list.push_back(std::make_pair(u, v));
  
  return edge_list;
}

void *build_graphs(edge_list_t edge_list, 
    CSRGraph * const csrG, CSCGraph * const cscG) {
  // Determine neighbors and remap nodes.
  std::unordered_map<nid_t, nid_t> node_rename;
  std::unordered_map<nid_t, std::vector<nid_t>> node_to_children;
  std::unordered_map<nid_t, std::vector<nid_t>> node_to_parents;
  nid_t rename_id = 0;
  offset_t num_edges = 0;
  for (const auto &edge : edge_list) {
    if (not node_rename.count(edge.first))
      node_rename[edge.first] = rename_id++;
    if (not node_rename.count(edge.second))
      node_rename[edge.second] = rename_id++;

    node_to_children[node_rename[edge.first]]
      .push_back(node_rename[edge.second]);
    node_to_parents[node_rename[edge.second]]
      .push_back(node_rename[edge.first]);
    num_edges++;
  }

  // Generate CSC and CSR graphs.
  csrG->index = new offset_t[rename_id + 1];
  csrG->neighbors = new nid_t[num_edges];
  cscG->index = new offset_t[rename_id + 1];
  cscG->neighbors = new nid_t[num_edges];

  csrG->num_nodes = cscG->num_nodes = rename_id;
  csrG->num_edges = cscG->num_edges = num_edges;
  csrG->index[0] = cscG->index[0] = 0;

  for (nid_t u = 0; u < rename_id; u++) {
    // CSR (push)
    {
      auto neighbors = node_to_children[u];
      csrG->index[u + 1] = csrG->index[u] + neighbors.size();
      for (offset_t off = 0; off < neighbors.size(); off++)
        csrG->neighbors[csrG->index[u] + off] = neighbors[off];
    }

    // CSC (pull)
    {
      auto neighbors = node_to_parents[u];
      cscG->index[u + 1] = cscG->index[u] + neighbors.size();
      for (offset_t off = 0; off < neighbors.size(); off++)
        cscG->neighbors[cscG->index[u] + off] = neighbors[off];
    }
  }
}

#endif // GRAPH_H
