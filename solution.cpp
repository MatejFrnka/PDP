//
// Created by matfr on 28.02.2023.
//

#include <vector>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <chrono>
#include <omp.h>

#define color char
#define MULTITHREAD_LIMIT 10

using namespace std;
static const color NO_COLOR = 0;
static const color A_COLOR = 1;
static const color B_COLOR = -1;


int maxWeightAchieved = 0;
color *bestConfiguration = new color[0];

struct Edge {
    int a;
    int b;
    int weight;

    Edge() = default;

    Edge(int a, int b, int weight) : a(a), b(b), weight(weight) {};
};


void
maximumBipartite(const Edge *edges, int edgesLen, color *colors, int colorsLen, int weight, int i, int remainingWeight);

void handleEdge(const Edge *edges, int edgesLen, color *colors, int colorsLen, int weight, int i, int remainingWeight,
                int nodeAId,
                int nodeBId) {
    // save colors
    color colorA = colors[nodeAId];
    color colorB = colors[nodeBId];

    // set color A if not set already
    if (colors[nodeAId] == NO_COLOR) {
        colors[nodeAId] = A_COLOR;
    }

    int res = 0;
    // if graph bipartite after adding edge, add the edge and continue recursion
    if (colors[nodeAId] == A_COLOR && colors[nodeBId] != A_COLOR) {
        colors[nodeBId] = B_COLOR;
        maximumBipartite(edges, edgesLen, colors, colorsLen, weight + edges[i].weight, i + 1,
                         remainingWeight - edges[i].weight);
    }

    // revert colors to previous state
    colors[nodeAId] = colorA;
    colors[nodeBId] = colorB;
}

void maximumBipartite(const Edge *edges, int edgesLen, color *colors, int colorsLen, int weight, int i,
                      int remainingWeight) {
    if (i == edgesLen) {
        if (weight > maxWeightAchieved) {
            #pragma omp critical
            {
                // check again in critical section
                if (weight > maxWeightAchieved) {
                    maxWeightAchieved = weight;
                }
                delete[] bestConfiguration;
                bestConfiguration = new color[colorsLen];
                for (int j = 0; j < colorsLen; ++j) {
                    bestConfiguration[j] = colors[j];
                }
            }
        }
        return;
    }

    if (remainingWeight + weight <= maxWeightAchieved) {
        // impossible to improve best result, stop
        return;
    }


    if (i < MULTITHREAD_LIMIT) {
        auto *colorsNew1 = new color[colorsLen];
        auto *colorsNew2 = new color[colorsLen];
        auto *colorsNew3 = new color[colorsLen];

        for (int j = 0; j < colorsLen; ++j) {
            colorsNew1[j] = colors[j];
            colorsNew2[j] = colors[j];
            colorsNew3[j] = colors[j];
        }

        #pragma omp task default(none) shared(edges, edgesLen, colorsLen) firstprivate(colorsNew1, weight, i, remainingWeight)
        {
            handleEdge(edges, edgesLen, colorsNew1, colorsLen, weight, i, remainingWeight, edges[i].a, edges[i].b);
            delete[] colorsNew1;
        }

        #pragma omp task default(none) shared(edges, edgesLen) firstprivate(colorsNew2, colorsLen, weight, i, remainingWeight)
        {
            handleEdge(edges, edgesLen, colorsNew2, colorsLen, weight, i, remainingWeight, edges[i].b, edges[i].a);
            delete[] colorsNew2;
        }

        #pragma omp task default(none) shared(edges, edgesLen) firstprivate(colorsNew3, colorsLen, weight, i, remainingWeight)
        {
            // run recursion without adding the edge
            maximumBipartite(edges, edgesLen, colorsNew3, colorsLen, weight, i + 1, remainingWeight - edges[i].weight);
            delete[] colorsNew3;
        }
    } else {
        handleEdge(edges, edgesLen, colors, colorsLen, weight, i, remainingWeight, edges[i].a, edges[i].b);
        handleEdge(edges, edgesLen, colors, colorsLen, weight, i, remainingWeight, edges[i].b, edges[i].a);
        maximumBipartite(edges, edgesLen, colors, colorsLen, weight, i + 1, remainingWeight - edges[i].weight);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        std::cout << "Required 1 parameter with path to input file" << std::endl;
        exit(1);
    }
    char *filename = argv[1];

    ifstream inputFile(filename);

    string line;
    if (!getline(inputFile, line)) {
        std::cout << "Invalid file" << std::endl;
        exit(1);
    }
    int nodeCount = stoi(line);

    Edge *edges = new Edge[nodeCount * nodeCount];
    int edgesLen = 0;
    int maxWeight = 0;
    for (int i = 0; i < nodeCount; ++i) {
        for (int j = 0; j < nodeCount; ++j) {
            int weight;
            inputFile >> weight;
            if (j < i) {
                // skip edges going in the other direction
                continue;
            }
            maxWeight += weight;
            if (weight != 0) {
                edges[edgesLen++] = Edge(i, j, weight);
            }
        }
    }

    // sort edges descending
    sort(edges, edges + edgesLen, [](Edge &a, Edge &b) { return a.weight > b.weight; });

    auto *colors = new color[nodeCount];
    for (int i = 0; i < nodeCount; ++i) {
        colors[i] = NO_COLOR;
    }

    auto start = chrono::high_resolution_clock::now();
    #pragma omp parallel default(none) shared(edges, edgesLen, colors, nodeCount, maxWeight, bestConfiguration)
    {

        #pragma omp single
        {
            maximumBipartite(edges, edgesLen, colors, nodeCount, 0, 0, maxWeight);
        }
    }

    cout << "evalution time: "
         << chrono::duration_cast<std::chrono::milliseconds>(chrono::high_resolution_clock::now() - start).count()
         << "ms" << endl;

    delete[] edges;
    delete[] colors;
    for (int i = 0; i < nodeCount; ++i) {
        cout << i << " - " << (bestConfiguration[i] == A_COLOR) << endl;
    }
    delete[] bestConfiguration;
    cout << maxWeightAchieved << endl;
    return 0;
}