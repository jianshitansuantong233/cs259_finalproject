//#define DEBUG_ON
//#define CACHE_STATS
#ifdef CACHE_STATS
#define DEBUG_CACHE(block) do { block; } while(false)
#else
#define DEBUG_CACHE(block)
#endif // CACHE_STATS

#include "bfs-fpga.h"

#include <algorithm>
#include <assert.h>
#include <iostream>
#include "bitmap.h"
#include "util.h"
#include "limits.h"

constexpr offset_t EDGE_CACHE_SIZE = 1 << 20; // 2^{20} = 1M
constexpr nid_t    CACHE_LINE_SIZE = 4; // Number of nodes in each cache-line.
constexpr nid_t    MAX_EPOCHS      = 100;
constexpr nid_t    MAX_NODES       = 1 << 13; // 2^{13} = 8,192
enum Mode { push = 0, pull = 1 };

template <typename T>
inline
T min(const T &v1, const T &v2) { return v1 < v2 ? v1 : v2; }

struct Update {
  nid_t    num_nodes;
  offset_t num_edges_explored;
};

void Controller(const nid_t start_nid, const offset_t start_edge_count,
    const nid_t num_nodes, offset_t edges_to_check,
    tapa::ostream<nid_t> &config_q, tapa::istream<Update> &ir_q,
    int alpha = 15, int beta = 18
) {
  config_q.write(start_nid);
  config_q.close();

  Update update = {1, start_edge_count};
  nid_t prev_num_nodes = 0;

  Mode mode = Mode::push;
  for (nid_t epoch = 0; epoch < MAX_EPOCHS; epoch++) {
    // Switch logic.
    if (mode == Mode::pull and
        (update.num_nodes > prev_num_nodes or update.num_nodes > num_nodes / beta)) {
      // Remain in pull.
      DEBUG(std::cout << "[Controller] stay on pull" << std::endl);
    } else if (update.num_edges_explored > edges_to_check / alpha) {
      DEBUG(std::cout << "[Controller] switch to pull" << std::endl);
      mode = Mode::pull;
    } else {
      DEBUG(std::cout << "[Controller] switch to push" << std::endl);
      mode = Mode::push;
    }
    //mode = Mode::pull;
    config_q.write(mode);
    config_q.close();

    // Update prev num node count.
    prev_num_nodes = update.num_nodes;

    // Get updates.
    TAPA_WHILE_NOT_EOT(ir_q) {
      update = ir_q.read(nullptr);
    }
    ir_q.try_open(); // Reset stream.

    DEBUG(
    std::cout << "Number nodes updated:  " << update.num_nodes << std::endl
              << "Number edges explored: " << update.num_edges_explored 
              << std::endl);

    // If no nodes have been updated, the kernel is done!
    if (update.num_nodes == 0) break;

    // Update the number of edges explored this epoch.
    if (mode == Mode::push)
      edges_to_check -= update.num_edges_explored;
  }
}

struct CacheLine {
  offset_t addr;
  nid_t values[CACHE_LINE_SIZE];
};

#define CL_HASH(addr) (addr & (EDGE_CACHE_SIZE - 1))
#ifdef CACHE_STATS
#define CL_FETCH_OR_MISS(cl, cl_addr, num_edges, neighbors_cache, pull_neighbors) ({\
    offset_t entry = (cl_addr) & (EDGE_CACHE_SIZE - 1);\
    if ((cl = neighbors_cache[entry]).addr != (cl_addr)) {\
      miss++;\
      cl.addr = (cl_addr);\
      for (offset_t i = (cl_addr); \
          i < min((cl_addr) + CACHE_LINE_SIZE, (num_edges)); i++)\
        cl.values[i - (cl_addr)] = pull_neighbors[i];\
      neighbors_cache[entry] = cl;\
    } else {\
      hit++;\
    }\
})
#else
#define CL_FETCH_OR_MISS(cl, cl_addr, num_edges, neighbors_cache, pull_neighbors) ({\
    offset_t entry = (cl_addr) & (EDGE_CACHE_SIZE - 1);\
    if ((cl = neighbors_cache[entry]).addr != (cl_addr)) {\
      cl.addr = (cl_addr);\
      for (offset_t i = (cl_addr); \
          i < min((cl_addr) + CACHE_LINE_SIZE, (num_edges)); i++)\
        cl.values[i - (cl_addr)] = pull_neighbors[i];\
      neighbors_cache[entry] = cl;\
    }\
})
#endif // CACHE_STATS

void PullNeighborsCache(
    const offset_t num_edges,
    tapa::istream<nid_t> &nodes_q, tapa::istream<bool> &early_term_q, 
    tapa::ostream<nid_t> &neighbors_q,
    tapa::mmap<offset_t> pull_index, tapa::mmap<nid_t> pull_neighbors
) {
  constexpr offset_t NUM_CACHE_LINES = EDGE_CACHE_SIZE / CACHE_LINE_SIZE;
  CacheLine neighbors_cache[NUM_CACHE_LINES];
#pragma HLS resource variable neighbors_cache core=XPM_MEMORY uram

  for (offset_t i = 0; i < NUM_CACHE_LINES; i++)
    neighbors_cache[i].addr = UINT_MAX;

#ifdef CACHE_STATS
  size_t hit, miss;
  hit = miss = 0;
#endif // CACHE_STATS

  for (;;) {
  TAPA_WHILE_NOT_EOT(nodes_q) {
    nid_t v = nodes_q.read(nullptr);
    std::cout << v << std::endl;

    offset_t start    = pull_index[v];
    offset_t end      = pull_index[v + 1];
    offset_t cl_start = start / CACHE_LINE_SIZE * CACHE_LINE_SIZE;
    offset_t cl_end   = (end + CACHE_LINE_SIZE - 1) 
                        / CACHE_LINE_SIZE * CACHE_LINE_SIZE;
    DEBUG(std::cout << "[PullNeighborsCache] getting parents for node " << v
                    << std::endl);
    DEBUG(std::cout << "[PullNeighborsCache] start: " << start 
                    << " end: " << end << std::endl
                    << "                       cl_start: " << cl_start
                    << " cl_end: " << cl_end << std::endl);
    
    // If there's no neighbors, no work to be done.
    if (start != end) {
      // For first entry.
      {
        CacheLine cl;
        CL_FETCH_OR_MISS(cl, cl_start, num_edges, neighbors_cache, pull_neighbors);
        nid_t cl_start_idx = start - cl_start;
        for (nid_t i = cl_start_idx; 
            i < min(CACHE_LINE_SIZE, cl_start_idx + end - start); i++) {
          DEBUG(std::cout << "[PullNeighborsCache] " << i << ": ");
          DEBUG(std::cout << cl.values[i] << std::endl);
          neighbors_q.write(cl.values[i]);
        }
      }

      for (offset_t addr = cl_start + CACHE_LINE_SIZE; 
          addr < cl_end - CACHE_LINE_SIZE; addr += CACHE_LINE_SIZE
      ) {
#pragma HLS pipeline
        CacheLine cl;
        CL_FETCH_OR_MISS(cl, addr, num_edges, neighbors_cache, pull_neighbors);
        for (nid_t i = 0; i < CACHE_LINE_SIZE; i++) {
          DEBUG(std::cout << "[PullNeighborsCache] " << i << ": ");
          DEBUG(std::cout << cl.values[i] << std::endl);
          neighbors_q.write(cl.values[i]);
        }
      }

      // For last entry.
      if (cl_start + CACHE_LINE_SIZE != cl_end) {
        CacheLine cl;
        CL_FETCH_OR_MISS(cl, cl_end - CACHE_LINE_SIZE, 
            num_edges, neighbors_cache, pull_neighbors);
        for (nid_t i = 0; i < CACHE_LINE_SIZE - (cl_end - end); i++) {
          DEBUG(std::cout << "[PullNeighborsCache] " << i << ": ");
          DEBUG(std::cout << cl.values[i] << std::endl);
          neighbors_q.write(cl.values[i]);
        }
      }
    }

    DEBUG(std::cout << "[PullNeighborsCache] done!" << std::endl);
    neighbors_q.close(); // End stream.

    DEBUG_CACHE(std::cout << "[PullNeighborsCache] hits: " << hit 
                          << " misses: " << miss << std::endl);
  }
  nodes_q.try_open();
  }
  //
}

void ProcessingElement(
    nid_t num_nodes, tapa::istream<nid_t> &config_q,
    tapa::ostream<nid_t> &update_q, tapa::ostream<Update> &ir_q,
    tapa::ostream<nid_t> &pull_req_q, tapa::istream<nid_t> &pull_resp_q,
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
  bool     is_push;
  nid_t    num_nodes_updated;
  offset_t num_edges_explored;

  for (;;) {
    num_nodes_updated = 0;
    num_edges_explored = 0;

    // Await configuration information.
    TAPA_WHILE_NOT_EOT(config_q) {
      auto dir = config_q.read(nullptr);
      if (dir == Mode::push)  is_push = true;
      else    /* Mode::pull */is_push = false;
      DEBUG(std::cout << "Next direction: "
                      << (is_push ? "push" : "pull") << std::endl);
    }
    config_q.try_open(); // Reset stream.
    DEBUG(std::cout << "Next epoch" << std::endl);

    if (is_push) { // PUSH
      push:
      for (nid_t u = 0; u < num_nodes; u++) {
        if (Bitmap::get_bit(frontier, u)) {
          DEBUG(std::cout << "[Push] node " << u << ": ");
          push_neighbors:
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
      num_edges_explored = 1;

      pull:
      for (nid_t v = 0; v < num_nodes; v++) {
        if (not Bitmap::get_bit(explored, v)) { // If not explored.
          pull_req_q.write(v);
          pull_neighbors:
          bool set_bit = false;
          TAPA_WHILE_NOT_EOT(pull_resp_q) {
#pragma HLS pipeline
            nid_t u = pull_resp_q.read(nullptr);
            DEBUG(
            if(set_bit) 
              std::cout << "[Pull] excess node " << u << std::endl);
            if (not set_bit and Bitmap::get_bit(frontier, u)) {
              set_bit = true;
              Bitmap::set_bit(explored, v);
              Bitmap::set_bit(next_frontier, v);
              update_q.write(v);
              num_nodes_updated++;
              DEBUG(std::cout << "[Pull] node " << v << ": " << u << std::endl);
            }
          }
          pull_resp_q.try_open(); // Reset stsream.
        }
      }
      pull_req_q.close();
    }
    update_q.close(); // Inform DepthWriter the current epoch has ended.

    // Swap frontiers.
    std::swap(frontier, next_frontier);
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
    const nid_t start_nid, const offset_t start_degree, 
    const nid_t num_nodes, const offset_t num_edges,
    tapa::mmap<offset_t> push_index, tapa::mmap<nid_t> push_neighbors,
    tapa::mmap<offset_t> pull_index, tapa::mmap<nid_t> pull_neighbors,
    tapa::mmap<depth_t> depth
) {
  assert(num_nodes <= MAX_NODES);
  tapa::stream<nid_t, 1>   config_q;
  tapa::stream<nid_t, 16>  update_q;
  tapa::stream<Update, 1>  ir_q;

  tapa::stream<nid_t, 1> pull_nodes_q;
  tapa::stream<bool, 1> early_term_q;
  tapa::stream<nid_t, 16> pull_neighbors_q;

  tapa::task()
    .invoke(Controller, start_nid, start_degree, num_nodes, num_edges, 
        config_q, ir_q, 15, 18)
    .invoke<tapa::detach>(PullNeighborsCache, num_edges, pull_nodes_q, 
        early_term_q, pull_neighbors_q, pull_index, pull_neighbors)
    .invoke<tapa::detach>(ProcessingElement, num_nodes, config_q, update_q, 
        ir_q, pull_nodes_q, pull_neighbors_q, 
        push_index, push_neighbors, pull_index, pull_neighbors, depth)
    .invoke<tapa::detach>(DepthWriter, update_q, depth);
}

