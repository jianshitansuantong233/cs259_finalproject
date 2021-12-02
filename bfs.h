#ifndef BFS_H_
#define BFS_H_

#include <ap_int.h>
#include <tapa.h>
#include "graph.h"

//There is a bug in Vitis HLS preventing fully pipelined read/write of struct
//via m_axi; using ap_uint can work-around this problem.
//template <typename T>
//using bits = ap_uint<tapa::widthof<T>()>;

void bfs_fpga(
    const nid_t start, const nid_t num_nodes,
    tapa::mmap<offset_t> push_index, tapa::mmap<nid_t> push_neighbors,
    tapa::mmap<offset_t> pull_index, tapa::mmap<nid_t> pull_neighbors,
    tapa::mmap<depth_t> depth);

#endif  // BFS_H_
