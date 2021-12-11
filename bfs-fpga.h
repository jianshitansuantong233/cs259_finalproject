#ifndef BFS_FPGA_H
#define BFS_FPGA_H

//#define DEBUG_ON
//#define CACHE_STATS

#include <cstdint>
#include <ap_int.h>
#include <cassert>
#include <tapa.h>
#include "graph.h"

constexpr offset_t NEIGHBORS_CHUNK_SIZE = 1;

//There is a bug in Vitis HLS preventing fully pipelined read/write of struct
//via m_axi; using ap_uint can work-around this problem.
template <typename T>
using bits = ap_uint<tapa::widthof<T>()>;

using nid_vec_t = tapa::vec_t<nid_t, NEIGHBORS_CHUNK_SIZE>;

void bfs_fpga(
    const nid_t start, const nid_t num_nodes, 
    tapa::mmap<offset_t> push_index, tapa::mmap<bits<nid_vec_t>> push_neighbors,
    tapa::mmap<depth_t> depths);

void bfs_fpga_edge(Pid num_partitions, Pid start, tapa::mmap<const Eid> num_edges, tapa::mmap<const Eid> edge_offsets, 
                    tapa::mmap<VertexAttr> vertices, tapa::mmap<bits<Edge>> edges);
#endif  // BFS_FPGA_H
