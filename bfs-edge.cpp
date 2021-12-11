#include <cassert>
#include <cstring>
#include <iostream>
#include "graph.h"
#include "util.h"
#include <cstdint>

using std::ostream;
/**
 * @details Bfs with Scatter-Gather. Input partitions are expected to have the same order
 *          as specified in ThunderGP.
 * 
 * @param[in]    num_partitions - number of partitions in a graph
 * @param[in]    num_edges      - number of edges in each partition 
 * @param[in]    edge_offsets   - start index for the starting edge in each partition
 * @param[inout] vertices       - vertices 
 * @param[in]    edges          - edges
 */
void bfs_fpga_edge(Pid num_partitions, Pid start, tapa::mmap<const Eid> num_edges, tapa::mmap<const Eid> edge_offsets, 
                    tapa::mmap<bits<VertexAttr>> vertices, tapa::mmap<bits<Edge>> edges) {
  tapa::stream<Task, MAX_VER> task_stream("task_stream");
  tapa::stream<Edge, MAX_EDGE> edge_stream("non_active_edge_stream");
  tapa::stream<Update_edge_version, MAX_EDGE> update_stream("update_stream");
  tapa::stream<Update_edge_version, MAX_EDGE> gathered_stream("gathered_stream");
  Vid num_partition_vertices;
  Vid vertex_offset;
  tapa::task()
      .invoke(Control, num_partitions, start, num_edges, edge_offsets, vertices, edges, task_stream);
      .invoke(Scatter, edges, task_stream, update_stream)
      .invoke(Gather, num_partition_vertices, vertex_offset, partition_vertices,
              update_stream, gathered_stream)
      .invoke(Apply, gathered_stream);
}
/**
 * @details Contoller of bfs. Send out all active partitions
 * 
 * @param[in]  edges         - edges (preferrably in istream form)
 * @param[in]  vertices      - vertices, grabbed directly from DRAM. TODO: optimize it using prefetch & cache.
 * @param[out] updates       - temporary update tuples
 */
void Control(Pid num_partitions, Pid start, tapa::mmap<const Eid> num_edges, tapa::mmap<const Eid> edge_offsets, 
            tapa::mmap<bits<VertexAttr>> vertices, tapa::mmap<bits<Edge>> edges, 
            tapa::ostream<bits<Task>>& task_stream){
    int num_done = 0;// processed partitions
    bool done[MAX_VER] = {};
    bool active[MAX_VER] = {};
    active[start] = true;
    while(num_done!=num_partitions){
        // do scatter
        for(int i=0;i<num_partitions;i++){
            if(!done[i] & active[i]){
                Task t;
                t.start_position = edge_offsets[i];
                t.num_edges = num_edges[i];
                t.depth = vertices[i];
                task_stream.write(t);
                for(int j=0;j<t.num_edges;j++){
                    active[edges[t.start_position+j].dst] = true;
                }
                done[i] = true;
                num_done++;
            }           
        }        
    }
}
/**
 * @details Scatter stage of bfs. 
 * 
 * @param[in]  edges         - edges (preferrably in istream form)
 * @param[in]  task_stream   - information about updates to be made
 * @param[out] updates       - temporary update tuples
 */
void Scatter(tapa::mmap<bits<Edge>> edges, tapa::istream<bits<Task>>& task_stream, tapa::ostream<bits<Update_edge_version>>& updates){
    TAPA_WHILE_NOT_EOT(task_stream) {
        Task t = task_stream.read();
        std::cout <<"Processing task: src "<<t.start_position<<", num "<<t.num_edges<<" source depth "<<t.depth<<std::endl;
        for(int i=0;i<t.num_edges;i++){
            Update_edge_version u;
            Edge e = edges[t.start_position+i];
            u.dst = e.dst;
            u.depth = vertices[t.depth]+1;
            updates.write(u);
        }        
    }
}
/**
 * @details Gather stage of bfs
 * 
 * @param[in] num_vertices       - number of vertices in this partition
 * @param[in] vertex_offset      - starting vertex id of all vertices in this partition
 * @param[in] vertices           - vertices in this partition
 * @param[in] temp_updates       - temporary update tuples
 * @param[out] updates           - final update tuples
 */
void Gather(tapa::istream<bits<Update_edge_version>>& temp_updates, tapa::mmap<bits<VertexAttr>> vertices){
    TAPA_WHILE_NOT_EOT(temp_updates) {
        Update_edge_version u = temp_updates.read();
        std::cout <<"Processing update "<<u.dst<<", "<<u.depth<<std::endl;
        vertices[u.dst] = vertices[u.dst]>u.depth? u.depth:vertices[u.dst];
    }
}
/**
 * @details Bfs with Scatter-Gather. Input partitions are expected to have the same order
 *          as specified in ThunderGP. CPU version.
 * 
 * @param[in]  num_vertices   - number of vertices in graph
 * @param[in]  num_edges      - number of edges in graph
 * @param[in]  vertices       - vertices 
 * @param[in]  edges          - edges
 * @param[out] updates        - update tuples
 */
void bfs_edge_cpu(const Vid num_vertices, const Eid num_edges, std::vector<VertexAttr>& vertices,
         edge_list_t edges) {
    for(int i=0;i<num_edges;i++){
        Update_edge_version u;
        u.dst = edge.dst;
        u.depth = vertices[edge.src]+1;
        auto v_temp = u.dst;
        vertices[v_temp] = vertices[v_temp]>u.depth? u.depth:vertices[v_temp];
        
    }
}