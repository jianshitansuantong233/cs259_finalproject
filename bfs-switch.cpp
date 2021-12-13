#include "bfs-fpga.h"

#include <algorithm>
#include <assert.h>
#include <iostream>
#include "bitmap.h"
#include "util.h"
#include "limits.h"

bool PUSH_OR_PULL = false; // true = PUSH, false = PULL

nid_t nodes_explored = 1;

constexpr nid_t MAX_EPOCHS = 100;
constexpr nid_t MAX_NODES = 4500;
enum Mode { push = 0, pull = 1 };

struct Update {
  nid_t    num_nodes;
  offset_t num_edges_explored;
};

void Controller(nid_t start_nid, nid_t num_nodes, nid_t num_edges,
    tapa::ostream<nid_t> &config_q, tapa::istream<Update> &ir_q
) {
  config_q.write(start_nid);
  config_q.close();

  Update update = {1, num_edges};

  Mode mode = Mode::push;
  for (nid_t epoch = 0; epoch < MAX_EPOCHS; epoch++) {
#pragma HLS unroll factor=4
    /*
      For each epoch,
        - Average # of Push traversal edges(PushTr) 
          = Total edges / Total vertices * Frontier vertices
        - Average # of Pull traversal edges(PullTr) 
          = Total edges / Total vertices * Unseen vertices 
          + Total vertices / Frontier vertices * Seen vertices
        - 
        - Hardcode Seen vertices here to be min(#frontier * 3, #remaining nodes / 3)
                   Unseen vertices = total vertices - traversed - frontier - seen vertices
      
      Switching logic: PushTr > PullTr? Pull : Push;

      - Seen vertices = Next Frontier, 
      - `update` represents the Frontier for this epoch
    */
    nid_t remaining_node = num_nodes - nodes_explored;
    nid_t Seen = std::min(update.num_nodes * 3, remaining_node / 3);
    nid_t Unseen =  remaining_node - Seen;
    int threshold = (int)(num_edges/num_nodes * (update.num_nodes - Unseen) - num_nodes/update.num_nodes * Seen);
    PUSH_OR_PULL = (threshold > 0 ? false : true); 
    DEBUG(
      std::cout << "nodes_explored: " << nodes_explored << std::endl;
      std::cout << "remaining_node: " << remaining_node << std::endl;
      std::cout << "update.num_nodes: " << update.num_nodes << std::endl;
      std::cout << "Seen: " << Seen << std::endl;
      std::cout << "Unseen: " << Unseen << std::endl;
      std::cout << "PUSH_OR_PULL: " << PUSH_OR_PULL << std::endl;
      std::cout << "threshold: " << threshold << std::endl;
      std::cout << std::endl;
    );
    config_q.write(PUSH_OR_PULL ? Mode::push : Mode::pull);
    config_q.close();

    // Update update;
    TAPA_WHILE_NOT_EOT(ir_q) {
      update = ir_q.read(nullptr);
    }
    ir_q.try_open(); // Reset stream.
    DEBUG(
    std::cout << "Number nodes updated:  " << update.num_nodes << std::endl
              << "Number edges explored: " << update.num_edges_explored 
              << std::endl);

    // If no updates, the kernel is done!
    if (update.num_nodes == 0) break;
  }
}

void ProcessingElement(
    nid_t num_nodes, const nid_t start_nid,
    tapa::ostream<nid_t> &update_q,
    tapa::mmap<offset_t> push_index, tapa::mmap<nid_t> push_neighbors
) {
  Bitmap::bitmap_t frontier[Bitmap::bitmap_size(MAX_NODES)];
  Bitmap::bitmap_t next_frontier[Bitmap::bitmap_size(MAX_NODES)];
  Bitmap::bitmap_t explored[Bitmap::bitmap_size(MAX_NODES)];
  for (nid_t i = 0; i < Bitmap::bitmap_size(MAX_NODES); i++)
    frontier[i] = next_frontier[i] = explored[i] = 0;
//#pragma HLS array_partition variable=frontier complete
//#pragma HLS array_partition variable=next_frontier complete
//#pragma HLS array_partition variable=explored complete

  // Setup starting node.
  Bitmap::set_bit(frontier, start_nid);
  Bitmap::set_bit(explored, start_nid);
  update_q.write(start_nid); // Send depth update for starting node.
  update_q.close();

  nid_t num_updates;
  do {
    num_updates = 0;

  for (;;) {
#pragma HLS loop_tripcount max=2048
    num_nodes_updated = 0;
    num_edges_explored = 0;

    // Await configuration information.
    TAPA_WHILE_NOT_EOT(config_q) {
      auto dir = config_q.read(nullptr);
      if (dir == Mode::push)  is_push = true;
      else    /* Mode::pull */is_push = false;
      DEBUG(std::cout << "Next direction: " << is_push << std::endl);
    }
    config_q.try_open(); // Reset stream.
    DEBUG(std::cout << "Next epoch" << std::endl);
    
    if (is_push) { // PUSH
      for (nid_t u = 0; u < num_nodes; u++) {
#pragma HLS pipeline II=1
        if (Bitmap::get_bit(frontier, u)) {
          DEBUG(std::cout << "[push] node " << u << ": ");
          for (offset_t off = push_index[u]; off < push_index[u + 1]; off++) {
#pragma HLS pipeline II=1
            nid_t v = push_neighbors[off];
            if (not Bitmap::get_bit(explored, v)) { // If child not explored.
              DEBUG(std::cout << v << " ");
              Bitmap::set_bit(explored, v);
              Bitmap::set_bit(next_frontier, v);
              update_q.write(v);
              num_nodes_updated++;
              nodes_explored++;
            }
          }          
          DEBUG(std::cout << std::endl);
          num_edges_explored += push_index[u + 1] - push_index[u];
        }
      }
    } else { // PULL
      num_edges_explored = 1;
      for (nid_t v = 0; v < num_nodes; v++) {
#pragma HLS pipeline II=1
        if (not Bitmap::get_bit(explored, v)) { // If not explored.
          for (offset_t off = pull_index[v]; off < pull_index[v + 1]; off++) {
#pragma HLS pipeline II=1
            nid_t u = pull_neighbors[off];
            if (Bitmap::get_bit(frontier, u)) {
              Bitmap::set_bit(explored, v);
              Bitmap::set_bit(next_frontier, v);
              update_q.write(v);
              num_nodes_updated++;
              nodes_explored++;
              DEBUG(std::cout << "[pull] node " << v << ": " << u << std::endl);
              break;
            }
          }
        }          
        DEBUG(std::cout << std::endl);
      }
    }
    update_q.close(); // Inform DepthWriter the current epoch has ended.

    // Swap frontiers.
    std::swap(frontier, next_frontier);
    std::cout << "next frontier update" << std::endl;
    for (size_t i = 0; i < Bitmap::bitmap_size(MAX_NODES); i++)
      next_frontier[i] = 0;

    // Send update information to controller.
    ir_q.write({num_nodes_updated, num_edges_explored});
    ir_q.close();
  }
}

void DepthWriter(tapa::istream<nid_t> &update_q, tapa::mmap<depth_t> depth) {
  depth_t cur_depth = 0;
  for (;;) {
#pragma HLS loop_tripcount max=2048
    TAPA_WHILE_NOT_EOT(update_q) {
      auto u = update_q.read(nullptr);
      depth[u] = cur_depth;
    }
    update_q.try_open(); // Reset stream.

    cur_depth++; // Next depth.
  }
}

void bfs_switch(
    nid_t start_nid, nid_t num_nodes, nid_t num_edges,
    tapa::mmap<offset_t> push_index, tapa::mmap<nid_t> push_neighbors,
    tapa::mmap<depth_t> depths
) {
  assert(num_nodes <= MAX_NODES);
  tapa::stream<nid_t, 128> update_q;

  tapa::task()
    .invoke(Controller, start_nid, num_nodes, num_edges, config_q, ir_q)
    .invoke<tapa::detach>(ProcessingElement, num_nodes, config_q, update_q, 
        ir_q, push_index, push_neighbors, pull_index, pull_neighbors, depth)
    .invoke<tapa::detach>(DepthWriter, update_q, depth);
}
