/************************************************************************************************
*                                                                                               *
*   I N C L U D E    F I L E S                                                                  *
*                                                                                               *
************************************************************************************************/
/*----------------------------------------------------------------------------------------------|
|   Include System Header Files                                                                 |
|----------------------------------------------------------------------------------------------*/
#include <iostream>
#include <cmath>

/*----------------------------------------------------------------------------------------------|
|   graph.hpp                                                                                   |
|----------------------------------------------------------------------------------------------*/
#include "core/graph.hpp"

/*----------------------------------------------------------------------------------------------|
|   namespace                                                                                   |
|----------------------------------------------------------------------------------------------*/
using namespace std;

/************************************************************************************************
*                                                                                               *
*   Module:                                                                                     *
*   conductance();                                                                              *
*                                                                                               *
*   Scope:                                                                                      *
*   External                                                                                    *
*                                                                                               *
*   Description:                                                                                *
*   This program is used to compute conductance of a given graph partitioned by GridGraph.      *
*                                                                                               *
*   Usage:                                                                                      *
*   ./conductance [path] [budget]                                                               *
*                                                                                               *
*   Parameters:                                                                                 *
*   [path]          -   path of the GridGraph formatted graph                                   *
*   [budget]        -   total memory budget in GB                                               *
*                                                                                               *
*   Return Values:                                                                              *
*   int  - execution status                                                                     *
*                                                                                               *
************************************************************************************************/
int main(int argc, char const *argv[]) {
    
    /*------------------------------------------------------------------------------------------|
    |   verify input is in correct format                                                       |
    |------------------------------------------------------------------------------------------*/
    if (argc < 2) {
        fprintf(stderr, "usage: conduc [path] [memory budget in GB]\n");
        exit(-1);
    }

    /*------------------------------------------------------------------------------------------|
    |   create required internal variables                                                      |
    |------------------------------------------------------------------------------------------*/
    string path = argv[1];
    long memory_bytes = atol(argv[2])*1024l*1024l*1024l;

    Graph graph(path);
    graph.set_memory_bytes(memory_bytes);

    long black_vertex_count = 0, red_vertex_count = 0, cross_over_count = 0;
    
    /*------------------------------------------------------------------------------------------|
    |   create graph splitting criteria                                                         |
    |------------------------------------------------------------------------------------------*/
    auto lambda_IsBlack = [](VertexId i)-> bool {
        return i % 2;
    };
    
    /*------------------------------------------------------------------------------------------|
    |   compute conductance of the graph based on the splitted clusters                         |
    |------------------------------------------------------------------------------------------*/
    double begin_time = get_time();
    cross_over_count = graph.stream_edges<VertexId>([&](Edge& e){
        if (lambda_IsBlack(e.source)) {
            write_add(&black_vertex_count, 1l);
            if (!lambda_IsBlack(e.target)) {
                return 1;
            }
        } else {
            write_add(&red_vertex_count, 1l);
            if (lambda_IsBlack(e.target)) {
                return 1; 
            }
        }
        return 0;
    });

    double result = 1.0 * cross_over_count / min(black_vertex_count, red_vertex_count);    
    double end_time = get_time();
    
    /*------------------------------------------------------------------------------------------|
    |   display results.                                                                        |
    |------------------------------------------------------------------------------------------*/
    printf("Conductance was computed in %.2f seconds\n", end_time - begin_time);
	printf("conductance value is %lf\n", result);
    
    return 0;
}

