#ifndef BFS_FPGA_H
#define BFS_FPGA_H

//#define DEBUG_ON
//#define CACHE_STATS

#include <cstdint>
#include <ap_int.h>
#include <cassert>
#include <tapa.h>
#include "graph.h"

constexpr int V_NUM_PARTITIONS = 2;

//There is a bug in Vitis HLS preventing fully pipelined read/write of struct
//via m_axi; using ap_uint can work-around this problem.
//template <typename T>
//using bits = ap_uint<tapa::widthof<T>()>;

void bfs_fpga(
    const nid_t start, const nid_t num_nodes, 
    tapa::mmap<offset_t> push_index, tapa::mmap<nid_t> push_neighbors,
    tapa::mmap<depth_t> depth);

void bfs_switch(
    nid_t start, nid_t num_nodes, nid_t num_edges,
    tapa::mmap<offset_t> push_index, tapa::mmap<nid_t> push_neighbors,
    tapa::mmap<offset_t> pull_index, tapa::mmap<nid_t> pull_neighbors,
    tapa::mmap<depth_t> depth);

void bfs_fpga_edge(Pid num_partitions, const Pid start_id, tapa::mmap<const Eid> num_edges, tapa::mmap<const Eid> edge_offsets, 
                    tapa::mmap<VertexAttr> vertices, tapa::mmap<bits<Edge>> edges);
#endif  // BFS_FPGA_H
