#ifndef BFS_FPGA_H
#define BFS_FPGA_H

//#define DEBUG_ON
//#define CACHE_STATS

#include <ap_int.h>
#include <cassert>
#include <tapa.h>
#include "graph.h"

//There is a bug in Vitis HLS preventing fully pipelined read/write of struct
//via m_axi; using ap_uint can work-around this problem.
//template <typename T>
//using bits = ap_uint<tapa::widthof<T>()>;
void bfs_fpga(
    const nid_t start, const offset_t start_degree,
    const nid_t num_nodes, const offset_t num_edges,
    tapa::mmap<offset_t> push_index, tapa::mmap<nid_t> push_neighbors,
    tapa::mmap<offset_t> pull_index, tapa::mmap<nid_t> pull_neighbors,
    tapa::mmap<depth_t> depth,
#ifdef CACHE_STATS
    tapa::mmap<offset_t> stats,
#endif // CACHE_STATS
    const int alpha = 15, const int beta = 18
    );

void bfs_fpga_edge(Pid num_partitions, Pid start, tapa::mmap<const Eid> num_edges, tapa::mmap<const Eid> edge_offsets, 
                    tapa::mmap<VertexAttr> vertices, tapa::mmap<bits<Edge>> edges);
#endif  // BFS_FPGA_H
