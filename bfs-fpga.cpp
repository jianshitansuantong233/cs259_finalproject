#define DEBUG_ON
#include "bfs-fpga.h"

#include <algorithm>
#include <assert.h>
#include <iostream>
#include "bitmap.h"
#include "util.h"

constexpr bool PUSH_OR_PULL = false; // true = PUSH, false = PULL

constexpr nid_t MAX_EPOCHS = 100;
constexpr nid_t MAX_NODES = 4500;
enum Mode { push = 0, pull = 1 };

struct Update {
  nid_t    num_nodes;
  offset_t num_edges_explored;
};

void Controller(nid_t start_nid, 
    tapa::ostream<nid_t> &config_q, tapa::istream<Update> &ir_q
) {
  config_q.write(start_nid);
  config_q.close();

  Mode mode = Mode::push;
  for (nid_t epoch = 0; epoch < MAX_EPOCHS; epoch++) {
    Update update;
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

    // Hardcode direction for now.
    config_q.write(PUSH_OR_PULL ? Mode::push : Mode::pull);
    config_q.close();
  }
}

void ProcessingElement(
    nid_t num_nodes, tapa::istream<nid_t> &config_q,
    tapa::ostream<nid_t> &update_q, tapa::ostream<Update> &ir_q,
    tapa::mmap<offset_t> push_index, tapa::mmap<nid_t> push_neighbors,
    tapa::mmap<offset_t> pull_index, tapa::mmap<nid_t> pull_neighbors,
    tapa::mmap<depth_t> depth
) {
  Bitmap::bitmap_t frontier[Bitmap::bitmap_size(MAX_NODES)];
  Bitmap::bitmap_t next_frontier[Bitmap::bitmap_size(MAX_NODES)];
  Bitmap::bitmap_t explored[Bitmap::bitmap_size(MAX_NODES)];
  for (nid_t i = 0; i < Bitmap::bitmap_size(MAX_NODES); i++)
    frontier[i] = next_frontier[i] = explored[i] = 0;

  // Setup starting node.
  TAPA_WHILE_NOT_EOT(config_q) {
    nid_t u = config_q.read(nullptr);
    Bitmap::set_bit(frontier, u);
    Bitmap::set_bit(explored, u);
    update_q.write(u); // Send depth update for starting node.
  }
  config_q.try_open(); // Reset stream.
  update_q.close(); // End update stream.

  // First epoch is always push.
  bool is_push = PUSH_OR_PULL;
  nid_t    num_nodes_updated;
  offset_t num_edges_explored;

  for (;;) {
    num_nodes_updated = 0;
    num_edges_explored = 0;

    if (is_push) { // PUSH
      for (nid_t u = 0; u < num_nodes; u++) {
        if (Bitmap::get_bit(frontier, u)) {
          DEBUG(std::cout << "[push] node " << u << ": ");
          for (offset_t off = push_index[u]; off < push_index[u + 1]; off++) {
#pragma HLS pipeline
            nid_t v = push_neighbors[off];
            if (not Bitmap::get_bit(explored, v)) { // If child not explored.
              DEBUG(std::cout << v << " ");
              Bitmap::set_bit(explored, v);
              Bitmap::set_bit(next_frontier, v);
              update_q.write(v);
              num_nodes_updated++;
            }
          }          
          DEBUG(std::cout << std::endl);
          num_edges_explored += push_index[u + 1] - push_index[u];
        }
      }
    } else { // PULL
      for (nid_t v = 0; v < num_nodes; v++) {
        if (not Bitmap::get_bit(explored, v)) { // If not explored.
          for (offset_t off = pull_index[v]; off < pull_index[v + 1]; off++) {
#pragma HLS pipeline
            nid_t u = pull_neighbors[off];
            if (Bitmap::get_bit(frontier, u)) {
              Bitmap::set_bit(explored, v);
              Bitmap::set_bit(next_frontier, v);
              update_q.write(v);
              num_nodes_updated++;
              DEBUG(std::cout << "[pull] node " << v << ": " << u << std::endl);
              break;
            }
          }
        }
      }
    }
    update_q.close(); // Inform DepthWriter the current epoch has ended.

    // Swap frontiers.
    std::swap(frontier, next_frontier);
    for (size_t i = 0; i < Bitmap::bitmap_size(MAX_NODES); i++)
      next_frontier[i] = 0;

    // Send update information to controller.
    ir_q.write({num_nodes_updated, num_edges_explored});
    ir_q.close();

    // Await configuration information.
    TAPA_WHILE_NOT_EOT(config_q) {
      auto dir = config_q.read(nullptr);
      if (dir == Mode::push)  is_push = true;
      else    /* Mode::pull */is_push = false;
      DEBUG(std::cout << "Next direction: " << is_push << std::endl);
    }
    config_q.try_open(); // Reset stream.
    DEBUG(std::cout << "Next epoch" << std::endl);
  }
}

void DepthWriter(tapa::istream<nid_t> &update_q, tapa::mmap<depth_t> depth) {
  depth_t cur_depth = 0;
  for (;;) {
    TAPA_WHILE_NOT_EOT(update_q) {
      auto u = update_q.read(nullptr);
      //DEBUG(std::cout << "Updating node " << u 
                      //<< " with depth " << cur_depth << std::endl);

      depth[u] = cur_depth;
    }
    update_q.try_open(); // Reset stream.

    cur_depth++; // Next depth.
  }
}

void bfs_fpga(
    nid_t start_nid, nid_t num_nodes,
    tapa::mmap<offset_t> push_index, tapa::mmap<nid_t> push_neighbors,
    tapa::mmap<offset_t> pull_index, tapa::mmap<nid_t> pull_neighbors,
    tapa::mmap<depth_t> depth
) {
  assert(num_nodes <= MAX_NODES);
  tapa::stream<nid_t, 1>  config_q;
  tapa::stream<nid_t, 8>  update_q;
  tapa::stream<Update, 1> ir_q;

  tapa::task()
    .invoke(Controller, start_nid, config_q, ir_q)
    .invoke<tapa::detach>(ProcessingElement, num_nodes, config_q, update_q, 
        ir_q, push_index, push_neighbors, pull_index, pull_neighbors, depth)
    .invoke<tapa::detach>(DepthWriter, update_q, depth);
}

