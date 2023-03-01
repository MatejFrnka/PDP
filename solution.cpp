//
// Created by matfr on 28.02.2023.
//

#include <vector>
#include <sstream>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <chrono>

#define color char

using namespace std;
static const color NO_COLOR = 0;
static const color A_COLOR = 1;
static const color B_COLOR = -1;

struct Edge {
    int a;
    int b;
    int weight;

    Edge() = default;

    Edge(int a, int b, int weight) : a(a), b(b), weight(weight) {};
};


int maximumBipartite(const Edge *edges, int edgesLen, color *colors, int weight, int i, int remainingWeight,
                     int &maxWeightAchieved);

int handleEdge(const Edge *edges, int edgesLen, color *colors, int weight, int i, int remainingWeight,
               int &maxWeightAchieved, int nodeAId, int nodeBId) {
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
        res = maximumBipartite(edges, edgesLen, colors, weight + edges[i].weight, i + 1,
                               remainingWeight - edges[i].weight, maxWeightAchieved);
    }

    // revert colors to previous state
    colors[nodeAId] = colorA;
    colors[nodeBId] = colorB;
    return res;
}

int maximumBipartite(const Edge *edges, int edgesLen, color *colors, int weight, int i, int remainingWeight,
                     int &maxWeightAchieved) {
    if (i == edgesLen) {
        if (weight > maxWeightAchieved) {
            maxWeightAchieved = weight;
        }
        return weight;
    }

    if (remainingWeight + weight <= maxWeightAchieved) {
        // impossible to improve best result, stop
        return weight;
    }

    int resA = handleEdge(edges, edgesLen, colors, weight, i, remainingWeight, maxWeightAchieved, edges[i].a,
                          edges[i].b);
    int resB = handleEdge(edges, edgesLen, colors, weight, i, remainingWeight, maxWeightAchieved, edges[i].b,
                          edges[i].a);
    // run recursion without adding the edge
    int dontAdd = maximumBipartite(edges, edgesLen, colors, weight, i + 1, remainingWeight - edges[i].weight,
                                   maxWeightAchieved);

    return max(dontAdd, max(resA, resB));
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
    int maxWeightTemp = 0;

    auto start = chrono::high_resolution_clock::now();
    int res = maximumBipartite(edges, edgesLen, colors, 0, 0, maxWeight, maxWeightTemp);
    cout << chrono::duration_cast<std::chrono::milliseconds>(chrono::high_resolution_clock::now() - start).count()
         << endl;

    delete[] edges;
    delete[] colors;

    cout << res << endl;
    return 0;
}
