#include "graph.h"

#include <unordered_map>

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
  pushG->index = std::vector<offset_t>(rename_id + 1);
  pushG->neighbors = std::vector<nid_t>(num_edges);
  pullG->index = std::vector<offset_t>(rename_id + 1);
  pullG->neighbors = std::vector<nid_t>(num_edges);

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

std::tuple<std::vector<offset_vec_t>, std::vector<nid_vec_t>, std::vector<nid_t>>
partition_edges(PushGraph * const g, int num_partitions) {
  offset_t avg_edges = (g->num_edges + num_partitions - 1) / num_partitions;

  std::vector<nid_t> partition_nodes(num_partitions);
  nid_t    cur_node       = 0;
  offset_t cur_edges      = 0;
  offset_t expected_edges = avg_edges;

  for (int i = 0; i < num_partitions; i++) {
    while (cur_node < g->num_nodes) {
      offset_t new_edges = cur_edges + 
                           (g->index[cur_node + 1] - g->index[cur_node]);

      bool done = false;
      if (new_edges >= expected_edges) {
        if (new_edges - expected_edges < expected_edges - cur_edges)
          partition_nodes[i] = ++cur_node;
        else
          partition_nodes[i] = cur_node;
        done = true;
      }

      cur_node++;
      cur_edges = new_edges;

      if (done) {
        expected_edges += avg_edges;
        break;
      }
    }
  }
  partition_nodes.back() = g->num_nodes;

  std::vector<offset_vec_t> index_es(num_partitions);
  std::vector<nid_vec_t> neighbors_es(num_partitions);
  nid_t start = 0;
  for (int i = 0; i < num_partitions; i++) {
    nid_t end = partition_nodes[i];

    for (nid_t u = start; u < end; u++) {
      index_es[i].push_back(g->index[u]);
      for (offset_t off = g->index[u]; off < g->index[u + 1]; off++) {
        neighbors_es[i].push_back(g->neighbors[off]);
      }
    }
    index_es[i].push_back(g->index[end]);

    start = end;
  }

  std::vector<nid_t> num_nodes(num_partitions + 1);
  num_nodes[0] = 0;
  for (int i = 0; i < num_partitions; i++)
    num_nodes[i + 1] = num_nodes[i] + index_es[i].size() - 1;

  return std::make_tuple(index_es, neighbors_es, num_nodes);
}
