#include <cassert>
#include <cstring>
#include <iostream>
#include "graph.h"
#include "bfs.h"

using std::ostream;
const int MAX_EDGE = 1024;
/**
 * @details Bfs with Scatter-Gather. Input partitions are expected to have the same order
 *          as specified in ThunderGP.
 * 
 * @param[in]  num_partitions - number of partitions in a graph
 * @param[in]  num_vertices   - number of vertices in each partition
 * @param[in]  num_edges      - number of edges in each partition 
 * @param[in]  vertices       - vertices 
 * @param[in]  edges          - edges
 * @param[out] updates        - update tuples
 */
void Bfs(Pid num_partitions, tapa::mmap<const Vid> num_vertices,
         tapa::mmap<const Eid> num_edges, tapa::mmap<VertexAttr> vertices,
         tapa::mmap<bits<Edge>> edges, tapa::mmap<bits<Update>> updates) {
  tapa::stream<Edge, MAX_EDGE> edge_stream("edge_stream");
  tapa::stream<Update, MAX_EDGE> update_stream("update_stream");
  tapa::stream<Update, MAX_EDGE> gathered_stream("gathered_stream");
  Vid num_partition_vertices;
  Vid vertex_offset;
  tapa::task()
      .invoke(Control, num_partitions, num_vertices, num_edges, edge_stream)
      .invoke(Scatter, edge_stream, update_stream)
      .invoke(Gather, num_partition_vertices, vertex_offset, partition_vertices,
              update_stream, gathered_stream)
      .invoke(Apply, gathered_stream);
}

/**
 * @details Cache for vertices
 * TBD
 */
tapa::mmap<VertexAttr> GLOBAL_VER;


/**
 * @details Scatter stage of bfs
 * 
 * @param[in]  edges         - edges (preferrably in istream form)
 * @param[out] updates       - temporary update tuples
 */
void Scatter(tapa::istream<bits<Edge>>& edges, tapa::ostream<bits<Update>>& updates){
    TAPA_WHILE_NOT_EOT(edges) {
        auto edge = edges.read();
        std::cout <<"Processing edge"<<edge;
        Update u;
        u.dst = edge.dst;
        u.depth = GLOBAL_VER[edge.src]+1;
        updates.write(u);
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
void Gather(Vid num_vertices, Vid vertex_offset, tapa::mmap<VertexAttr> vertices,
            tapa::istream<bits<Update>>& temp_updates, tapa::ostream<bits<Update>>& updates){
    // Predefined max number of vertices in a partition
    assert(num_vertices<4);
    TAPA_WHILE_NOT_EOT(temp_updates) {
        auto u = temp_updates.read();
        std::cout <<"Processing update"<<u;
        auto v_temp = u.dst-vertex_offset;
        vertices[v_temp] = vertices[v_temp]>u.depth? u.depth:vertices[v_temp];
    }
    for(int i=0;i<num_vertices;i++){
        Update temp;
        temp.dst = i+vertex_offset;
        temp.depth = vertices[i];
        updates.write(temp);
    }
}
/**
 * @details Apply stage of bfs
 * 
 * @param[in] updates            - final update tuples
 */
void Apply(tapa::istream<bits<Update>>& updates){
    TAPA_WHILE_NOT_EOT(updates) {
        Update u = updates.read();
        GLOBAL_VER[u.dst] = u.depth;
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
        Update u;
        u.dst = edge.dst;
        u.depth = vertices[edge.src]+1;
        auto v_temp = u.dst;
        vertices[v_temp] = vertices[v_temp]>u.depth? u.depth:vertices[v_temp];
        
    }
}