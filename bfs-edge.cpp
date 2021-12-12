#include <cassert>
#include <cstring>
#include <iostream>
#include "bfs-fpga.h"
#include "util.h"


/**
 * @details Contoller of bfs. Send out all active partitions
 * 
 * @param[in]  edges         - edges (preferrably in istream form)
 * @param[in]  vertices      - vertices, grabbed directly from DRAM. TODO: optimize it using prefetch & cache.
 * @param[out] updates       - temporary update tuples
 */
void Control(Pid num_partitions, Pid start_id, tapa::mmap<const Eid> num_edges, tapa::mmap<const Eid> edge_offsets, 
            tapa::mmap<VertexAttr> vertices, tapa::mmap<bits<Edge>> edges, 
            tapa::ostream<Task>& task_stream, tapa::istream<Resp>& resp_stream){
    int num_sent = 0;// sent partitions
    int num_done = 0;// processed partitions
    bool all_done = false;
    bool done[MAX_VER] = {};
    bool active[MAX_VER] = {};
    active[start_id] = true;
    while(num_done!=num_sent | !all_done){
#pragma HLS loop_tripcount max=MAX_VER
        all_done = true;
        //std::cout<<"num_sent "<<num_sent<<" num_done "<<num_done<<std::endl;
        // do scatter
        for(int i=0;i<num_partitions;i++){
#pragma HLS loop_tripcount max=MAX_VER
#pragma HLS unroll factor=2
            //std::cout<<"Processing partition "<<i<<std::endl;
            if(!done[i] & active[i]){
                Task t{edge_offsets[i], num_edges[i], vertices[i]};
                task_stream.write(t);
                //std::cout<<"Sending out "<<i<<" "<<num_edges[i]<<std::endl;
                done[i] = true;
                num_sent++;
                all_done = false;
            }           
        }     
        if(num_done==num_sent && all_done) break;   
        // collect response from gather
        TAPA_WHILE_NOT_EOT(resp_stream){
            Resp r = resp_stream.read(nullptr);
            active[r] = true;
            all_done = false;            
            //std::cout<<"Activate "<<r<<std::endl;
        }
        num_done++;
        resp_stream.open();
    }
    task_stream.close();
}
/**
 * @details Scatter stage of bfs. 
 * 
 * @param[in]  edges         - edges (preferrably in istream form)
 * @param[in]  task_stream   - information about updates to be made
 * @param[out] updates       - temporary update tuples
 */
void Scatter(tapa::mmap<bits<Edge>> edges, tapa::istream<Task>& task_stream, 
            tapa::ostream<Update_edge_version>& updates, tapa::ostream<Update_num>& update_num_stream){
    TAPA_WHILE_NOT_EOT(task_stream) {
        Task t = task_stream.read();
        //std::cout <<"Processing task: src "<<t.start_position<<", num "<<t.num_edges<<" source depth "<<t.depth<<std::endl;
        for(int i=0;i<t.num_edges;i++){        
#pragma HLS loop_tripcount max=MAX_EDGE   
#pragma HLS unroll factor=16
            auto e = tapa::bit_cast<Edge>(edges[t.start_position+i]);
            Update_edge_version u{ e.dst, t.depth+1};
            updates.write(u);
            //std::cout<<"Update: src "<<e.dst<<", depth "<<t.depth+1<<std::endl;
        }    
        //std::cout<<"Number of Update: "<<t.num_edges<<std::endl;
        update_num_stream.write(t.num_edges);    
    }
    update_num_stream.close();
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
void Gather(tapa::istream<Update_edge_version>& temp_updates, tapa::istream<Update_num>& update_num_stream, 
            tapa::mmap<VertexAttr> vertices, tapa::ostream<Resp>& resp_stream){
    TAPA_WHILE_NOT_EOT(update_num_stream) {
        Update_num num = update_num_stream.read(); 
        //std::cout<<"Will proceses "<<num<<" updates"<<std::endl;
        for(int i=0;i<num;i++){
#pragma HLS loop_tripcount max=MAX_EDGE
#pragma HLS unroll factor=16
            Update_edge_version u = temp_updates.read();
            //std::cout <<"Processing update "<<u.dst<<", "<<u.depth<<std::endl;
            if(vertices[u.dst]>u.depth){
                vertices[u.dst] = u.depth;
                Resp re = u.dst;
                resp_stream.write(re);
                //std::cout <<"Writing update "<<u.dst<<", "<<u.depth<<std::endl;
            }
        }
        resp_stream.close();
    }
}
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
void bfs_fpga_edge(Pid num_partitions, const Pid start_id, tapa::mmap<const Eid> num_edges, tapa::mmap<const Eid> edge_offsets, 
                    tapa::mmap<VertexAttr> vertices, tapa::mmap<bits<Edge>> edges) {
  tapa::stream<Task, MAX_VER> task_stream("task_stream");
  tapa::stream<Update_edge_version, MAX_VER*MAX_EDGE> update_stream("update_stream");
  tapa::stream<Update_num, MAX_VER> update_num_stream("update_num_stream");
  tapa::stream<Resp, MAX_VER*MAX_EDGE> resp_stream("resp_stream");
  tapa::task()
      .invoke(Control, num_partitions, start_id, num_edges, edge_offsets, vertices, edges, task_stream, resp_stream)
      .invoke(Scatter, edges, task_stream, update_stream, update_num_stream)
      .invoke(Gather, update_stream, update_num_stream, vertices, resp_stream);
}