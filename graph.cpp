#include "graph.h"

/**
 * Loads in edge list from input stream.
 */
edge_list_t load_edgelist(std::istream &in) {
  edge_list_t edge_list;
  nid_t u, v;
  
  while (in >> u >> v)
    edge_list.push_back(std::make_pair(u, v));
  
  return edge_list;
}

/**
 * Constructs CSR and CSC graphs from an edge list.
 * Parmeters:
 *   - edge_list <- graph edge list (remap edge list too).
 *   - pushG     <- pointer to push graph.
 *   - pullG     <- pointer to pull graph.
 */
void build_graphs(edge_list_t &edge_list, 
    PushGraph * const pushG, PullGraph * const pullG
) {
  // Determine neighbors and remap nodes.
  std::unordered_map<nid_t, nid_t> node_rename;
  std::unordered_map<nid_t, std::vector<nid_t>> node_to_children;
  std::unordered_map<nid_t, std::vector<nid_t>> node_to_parents;
  nid_t rename_id = 0;
  offset_t num_edges = 0;
  for (auto &edge : edge_list) {
    if (not node_rename.count(edge.first))
      node_rename[edge.first] = rename_id++;
    if (not node_rename.count(edge.second))
      node_rename[edge.second] = rename_id++;

    // Remap edge.
    edge.first = node_rename[edge.first];
    edge.second = node_rename[edge.second];

    // Push renamed edges to the appropriate neighbors list.
    node_to_children[edge.first].push_back(edge.second);
    node_to_parents[edge.second].push_back(edge.first);
    num_edges++;
  }

  // Generate CSC and CSR graphs.
  pushG->index = new offset_t[rename_id + 1];
  pushG->neighbors = new nid_t[num_edges];
  pullG->index = new offset_t[rename_id + 1];
  pullG->neighbors = new nid_t[num_edges];

  pushG->num_nodes = pullG->num_nodes = rename_id;
  pushG->num_edges = pullG->num_edges = num_edges;
  pushG->index[0] = pullG->index[0] = 0;

  for (nid_t u = 0; u < rename_id; u++) {
    // CSR (push)
    {
      auto neighbors = node_to_children[u];
      pushG->index[u + 1] = pushG->index[u] + neighbors.size();
      for (offset_t off = 0; off < neighbors.size(); off++)
        pushG->neighbors[pushG->index[u] + off] = neighbors[off];
    }

    // CSC (pull)
    {
      auto neighbors = node_to_parents[u];
      pullG->index[u + 1] = pullG->index[u] + neighbors.size();
      for (offset_t off = 0; off < neighbors.size(); off++)
        pullG->neighbors[pullG->index[u] + off] = neighbors[off];
    }
  }
}
