#include "bfs-fpga.h"

#include <algorithm>
#include <assert.h>
#include <iostream>
#include "bitmap.h"
#include "util.h"
#include "limits.h"

constexpr nid_t MAX_NODES = 1 << 13; // 2^{13} = 8,192

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

    DEBUG(std::cout << "Next epoch" << std::endl);

    push:
    for (nid_t u = 0; u < num_nodes; u++) {
#pragma HLS unroll
      if (Bitmap::get_bit(frontier, u)) {
        DEBUG(std::cout << "[Push] node " << u << ": ");

        push_neis:
        for (offset_t off = push_index[u]; off < push_index[u + 1]; off++) {
#pragma HLS pipeline
          nid_t v = push_neighbors[off];
          if (not Bitmap::get_bit(explored, v)) { // If child not explored.
            DEBUG(std::cout << v << " ");
            Bitmap::set_bit(explored, v);
            Bitmap::set_bit(next_frontier, v);
            update_q.write(v);
            num_updates++;
          }
        }          
        DEBUG(std::cout << std::endl);
      }
    }
    update_q.close(); // Inform DepthWriter the current epoch has ended.

    // Swap frontiers.
    std::swap(frontier, next_frontier);
    for (size_t i = 0; i < Bitmap::bitmap_size(MAX_NODES); i++)
      next_frontier[i] = 0;
  } while (num_updates != 0);
}

void DepthWriter(tapa::istream<nid_t> &update_q, tapa::mmap<depth_t> depth) {
  depth_t cur_depth = 0;
  for (;;) {
    TAPA_WHILE_NOT_EOT(update_q) {
      auto u = update_q.read(nullptr);
      depth[u] = cur_depth;
    }
    update_q.try_open(); // Reset stream.

    cur_depth++; // Next depth.
  }
}

void bfs_fpga(
    const nid_t start_nid, const nid_t num_nodes, 
    tapa::mmap<offset_t> push_index, tapa::mmap<nid_t> push_neighbors,
    tapa::mmap<depth_t> depths
) {
  assert(num_nodes <= MAX_NODES);
  tapa::stream<nid_t, 128> update_q;

  tapa::task()
    .invoke(ProcessingElement, num_nodes, start_nid, update_q, 
        push_index, push_neighbors)
    .invoke<tapa::detach>(DepthWriter, update_q, depths);
}
