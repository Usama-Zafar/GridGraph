/************************************************************************************************
*                                                                                               *
*   I N C L U D E    F I L E S                                                                  *
*                                                                                               *
************************************************************************************************/
/*----------------------------------------------------------------------------------------------|
|   Include System Header Files                                                                 |
|----------------------------------------------------------------------------------------------*/
#include <string> 
#include <iostream>
#include <fstream>

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
*   Community();                                                                                *
*                                                                                               *
*   Scope:                                                                                      *
*   External                                                                                    *
*                                                                                               *
*   Description:                                                                                *
*   This program is used to detect communities in a given graph partitioned by GridGraph.       *
*                                                                                               *
*   Usage:                                                                                      *
*   ./community [path] [communities] [iterations] [budget]                                      *
*                                                                                               *
*   Parameters:                                                                                 *
*   [path]          -   path of the GridGraph formatted graph                                   *
*   [communities]   -   total number of communities to form                                     *
*   [iterations]    -   total number of iterations for the algorithm                            *
*   [budget]        -   total memory budget in GB                                               *
*                                                                                               *
*   Return Values:                                                                              *
*   int  - execution status                                                                     *
*                                                                                               *
************************************************************************************************/
int main(int argc, char ** argv) {
    
    /*------------------------------------------------------------------------------------------|
    |   verify input is in correct format                                                       |
    |------------------------------------------------------------------------------------------*/
	if (argc<4) {
		fprintf(stderr, "usage: community [path] [Communities] [iterations] [memory budget in GB] \n");
		exit(-1);
	}
	
	/*------------------------------------------------------------------------------------------|
    |   create required internal variables                                                      |
    |------------------------------------------------------------------------------------------*/
	string path = argv[1];
	int LabelsNum = atoi(argv[2]);
	int IterationsNum = atoi(argv[3]);
	long memory_bytes = atol(argv[4])*1024l*1024l*1024l;
	
	printf("Number of Labels = %d\n",LabelsNum);
	printf("Number of Iterations = %d\n",IterationsNum);
	printf("Memory Budget = %d GB\n",atoi(argv[4]));	

	Graph graph(path);
	graph.set_memory_bytes(memory_bytes);
	BigVector<double> ProbabilityOfVertices(graph.path+"/ProbabilityOfVertices", graph.vertices);
	BigVector<double [20]> TempLabelGrid(graph.path+"/TempLabelGrid", graph.vertices);
	BigVector<VertexId> LabelsOfVertices(graph.path+"/LabelsOfVertices", graph.vertices);
	BigVector<VertexId> OutDegreesOfVertices(graph.path+"/OutDegreesOfVerticess", graph.vertices);
	long vertex_data_bytes = (long) graph.vertices * (sizeof(double) + sizeof(double) + sizeof(VertexId));
	graph.set_vertex_data_bytes(vertex_data_bytes);

	double begin_time = get_time();

	/*------------------------------------------------------------------------------------------|
    |   Out Degrees Calculation                                                                 |
    |------------------------------------------------------------------------------------------*/
	OutDegreesOfVertices.fill(0);
	graph.stream_edges<VertexId>(
		[&](Edge & e){
			write_add(&OutDegreesOfVertices[e.source], 1);
			return 0;
		}, nullptr, 0, 0
	);

	/*------------------------------------------------------------------------------------------|
    |   Probabilities calculation                                                               |
    |------------------------------------------------------------------------------------------*/
	graph.hint(ProbabilityOfVertices);
	graph.stream_vertices<VertexId>(
		[&](VertexId i){
			if(OutDegreesOfVertices[i] != 0)
				ProbabilityOfVertices[i] = 1.f / (OutDegreesOfVertices[i]);
			else
				ProbabilityOfVertices[i] = 0.f;
			return 0;
		}, nullptr, 0
	);
	printf("Degrees and Probability computation of each Vertex was completed in %.2f seconds\n", get_time() - begin_time);
	

	for (int i=0;i<graph.vertices;i++){
		for (int j=0;j<LabelsNum;j++){
			TempLabelGrid[i][j] = 0.f;
		}
	}

	double begin_time1 = get_time();

	/*------------------------------------------------------------------------------------------|
    |   Assigning every vertex in graph with label -999. -999 means not visited yet.            |
    |------------------------------------------------------------------------------------------*/
	LabelsOfVertices.fill(-999);
	srand((unsigned)time(0)); 
	for (int i=0;i<LabelsNum;i++){
		int RandomIndex = rand();
		RandomIndex = RandomIndex % graph.vertices;
		LabelsOfVertices[RandomIndex] = i;
	}

	printf("%d number of Labels were randomly allocated to vertices/nodes in %.2f seconds.\n",LabelsNum, get_time() - begin_time1);

	double begin_timeX = get_time();

	/*------------------------------------------------------------------------------------------|
    |   Preprocessing completed. Main Algorithm starts here.                                    |
    |------------------------------------------------------------------------------------------*/
	for (int k=0;k<IterationsNum;k++) {
		double begin_time2 = get_time();
		printf("Iteration no. %d in process...\t",k+1);

		graph.hint(TempLabelGrid);
		//process edges
		graph.stream_edges<VertexId>(
			[&](Edge & e){
				if ((LabelsOfVertices[e.source]>=0))
					if (LabelsOfVertices[e.target]<0)
						write_add(&TempLabelGrid[e.target][LabelsOfVertices[e.source]], TempLabelGrid[e.target][LabelsOfVertices[e.source]]+ProbabilityOfVertices[e.source]);
				return 0;
			}, nullptr, 0, 1
		);

		//proces vertices
		graph.stream_vertices<VertexId>(
			[&](VertexId i){
				if (LabelsOfVertices[i]<0){
					float maxCount = 0;
					int label = 0;
					for (int j=0;j<LabelsNum;j++){
						if (TempLabelGrid[i][j]>maxCount){
							maxCount = TempLabelGrid[i][j];
							label = j;
						} 
					}
					if (maxCount>0){
						write_add(&LabelsOfVertices[i], label+999);
					}
				}
				return 0;
			}
		);

		printf("Completed in %.2f seconds\n", get_time() - begin_time2);

	}


	printf("Time taken by %d number of iterations inside label propagation algorithm =  %.2f seconds\n", IterationsNum, get_time() - begin_timeX);
	
	double begin_time3 = get_time();
	
	/*------------------------------------------------------------------------------------------|
    |   Calculating Frequency of Labels assigned. The represent Communities                     |
    |------------------------------------------------------------------------------------------*/
	int FrequencyofLabels [LabelsNum];
	for (int j=0;j<LabelsNum;j++){
		FrequencyofLabels[j] = 0;
	}
	for (int j=0;j<graph.vertices;j++){
		if (LabelsOfVertices[j]>=0){
			FrequencyofLabels[LabelsOfVertices[j]]+=1;
		}
	}

	graph.stream_vertices<VertexId>(
		[&](VertexId i){
			return 0;
		}, nullptr, 0,
		[&](pair<VertexId,VertexId> vid_range){
			LabelsOfVertices.load(vid_range.first, vid_range.second);
		},
		[&](pair<VertexId,VertexId> vid_range){
			LabelsOfVertices.save();
		}
	);
	for (int j=0;j<LabelsNum;j++){
		printf("Number of vertices of Label %d = %d\n",j, FrequencyofLabels[j]);
	}
	printf("Time taken for calculating number of nodes/vertices of each label =  %.2f seconds\n",  get_time() - begin_time3);
	printf("Total time taken is whole execution =  %.2f seconds\n",  get_time() - begin_time);
	
	return 0;
}
 
