#define DEBUG_ON
#include "bfs.h"

#include <algorithm>
#include <assert.h>
#include <iostream>
#include "bitmap.h"
#include "util.h"

constexpr bool PUSH_OR_PULL = false; // true = PUSH, false = PULL

constexpr nid_t MAX_NODES = 4500;
enum Mode { push = 0, pull = 1 };

void Controller(const nid_t start, 
    tapa::ostream<nid_t> config_q, tapa::istream<offset_t> ir_q
) {
  config_q.write(start);
  config_q.close();

  Mode mode = Mode::push;
  for (int iter = 0; iter < 10; iter++) {
    nid_t update = 0;
    TAPA_WHILE_NOT_EOT(ir_q) {
      update += ir_q.read(nullptr);
    }
    ir_q.try_open(); // Reset stream.

    DEBUG(std::cout << "Number of updates: " << update << std::endl);

    // If no updates, the kernel is done!
    if (update == 0) break;

    // Hardcode direction for now.
    config_q.write(PUSH_OR_PULL ? Mode::push : Mode::pull);
    config_q.close();
  }
}

void ProcessingElement(
    const nid_t num_nodes, tapa::istream<nid_t> config_q,
    tapa::ostream<nid_t> update_q, tapa::ostream<offset_t> ir_q,
    tapa::mmap<offset_t> push_index, tapa::mmap<nid_t> push_neighbors,
    tapa::mmap<offset_t> pull_index, tapa::mmap<nid_t> pull_neighbors,
    tapa::mmap<depth_t> depth,
    Bitmap::bitmap_t *frontier, Bitmap::bitmap_t *next_frontier
) {
  // Setup starting node.
  TAPA_WHILE_NOT_EOT(config_q) {
    nid_t u = config_q.read(nullptr);
    Bitmap::set_bit(frontier, u);
    update_q.write(u); // Send depth update for starting node.
  }
  config_q.try_open(); // Reset stream.
  update_q.close(); // End update stream.

  // First iteration is always push.
  bool is_push = PUSH_OR_PULL;
  offset_t update; // Update info for DO.

  for (;;) {
    update = 0; // Reset updates.

    if (is_push) { // PUSH
      for (nid_t u = 0; u < num_nodes; u++) {
        if (Bitmap::get_bit(frontier, u)) {
          DEBUG(std::cout << "[push] node " << u << ": ");
          for (offset_t off = push_index[u]; off < push_index[u + 1]; off++) {
            nid_t v = push_neighbors[off];
            if (depth[v] == INVALID_DEPTH) {
              DEBUG(std::cout << v << " ");
              Bitmap::set_bit(next_frontier, v);
              update_q.write(v);
              update++; // TEMP
            }
          }          
          DEBUG(std::cout << std::endl);
          // TODO: need no return the edges traversed for DO.
          //update += push_index[u + 1] - push_index[u];
        }
      }
    } else { // PULL
      for (nid_t v = 0; v < num_nodes; v++) {
        if (depth[v] == INVALID_DEPTH) {
          for (offset_t off = pull_index[v]; off < pull_index[v + 1]; off++) {
            nid_t u = pull_neighbors[off];
            if (Bitmap::get_bit(frontier, u)) {
              Bitmap::set_bit(next_frontier, v);
              update_q.write(v);
              update++;
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
    ir_q.write(update);
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

void DepthWriter(tapa::istream<nid_t> update_q, tapa::mmap<depth_t> depth) {
  depth_t cur_depth = 0;
  for (;;) {
    TAPA_WHILE_NOT_EOT(update_q) {
      auto u = update_q.read(nullptr);
      DEBUG(std::cout << "Updating node " << u 
                      << " with depth " << cur_depth << std::endl);

      depth[u] = cur_depth;
    }
    update_q.try_open(); // Reset stream.

    cur_depth++; // Next depth.
  }
}

void bfs_fpga(
    const nid_t start, const nid_t num_nodes,
    tapa::mmap<offset_t> push_index, tapa::mmap<nid_t> push_neighbors,
    tapa::mmap<offset_t> pull_index, tapa::mmap<nid_t> pull_neighbors,
    tapa::mmap<depth_t> depth
) {
  assert(num_nodes < MAX_NODES);
  // Shared frontiers.
  Bitmap::bitmap_t frontier1[Bitmap::bitmap_size(MAX_NODES)];
  Bitmap::bitmap_t frontier2[Bitmap::bitmap_size(MAX_NODES)];
  for (size_t i = 0; i < Bitmap::bitmap_size(MAX_NODES); i++)
    frontier1[i] = frontier2[i] = 0;

  tapa::stream<nid_t, 1>    config_q;
  tapa::stream<nid_t, 8>    update_q;
  tapa::stream<offset_t, 1> ir_q;

  tapa::task()
    .invoke(Controller, start, config_q, ir_q)
    .invoke<tapa::detach>(ProcessingElement, num_nodes, config_q, update_q, 
        ir_q, push_index, push_neighbors, pull_index, pull_neighbors,
        depth, frontier1, frontier2)
    .invoke<tapa::detach>(DepthWriter, update_q, depth);
}

