//
// Created by matfr on 28.02.2023.
//

#include <vector>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <mpi.h>

#define color char
#define SEQUENTIAL_LIMIT 10
#define MULTITHREAD_LIMIT 15

#define NODE_COUNT 150

#define TAG_TASK_EDGES 0
#define TAG_DONE 1
#define TAG_STOP 2
#define TAG_TASK_INIT 0


using namespace std;
static const color NO_COLOR = 0;
static const color A_COLOR = 1;
static const color B_COLOR = -1;


int maxWeightAchieved = 0;
color bestConfiguration[NODE_COUNT];

struct Outcome {
    int maxWeight;
    color colorsConfiguration[NODE_COUNT];
};

struct Edge {
    int a;
    int b;
    int weight;

    Edge() = default;

    Edge(int a, int b, int weight) : a(a), b(b), weight(weight) {};
};

struct State {
    int colorsLen;
    int weight;
    int i;
    int remainingWeight;
    int bestAchievedWeight;
    color colors[NODE_COUNT];

    State() = default;

    State(const color *colors, int colorsLen, int weight, int i, int remainingWeight) :
            colorsLen(colorsLen), weight(weight), i(i), remainingWeight(remainingWeight), bestAchievedWeight(0) {
        for (int j = 0; j < colorsLen; ++j) {
            this->colors[j] = colors[j];
        }
    }
};

void maximumBipartite(const Edge *edges, int edgesLen, color *colors, int colorsLen, int weight, int i,
                      int remainingWeight);

void maximumBipartiteInit(const Edge *edges, int edgesLen, color *colors, int colorsLen, int weight, int i,
                          int remainingWeight, vector<State> &states);

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

void handleEdgeInit(const Edge *edges, int edgesLen, color *colors, int colorsLen, int weight, int i,
                    int remainingWeight, int nodeAId, int nodeBId, vector<State> &states) {
    // save colors
    color colorA = colors[nodeAId];
    color colorB = colors[nodeBId];

    // set color A if not set already
    if (colors[nodeAId] == NO_COLOR) {
        colors[nodeAId] = A_COLOR;
    }

    // if graph bipartite after adding edge, add the edge and continue recursion
    if (colors[nodeAId] == A_COLOR && colors[nodeBId] != A_COLOR) {
        colors[nodeBId] = B_COLOR;
        maximumBipartiteInit(edges, edgesLen, colors, colorsLen, weight + edges[i].weight, i + 1,
                             remainingWeight - edges[i].weight, states);
    }

    // revert colors to previous state
    colors[nodeAId] = colorA;
    colors[nodeBId] = colorB;
}

void maximumBipartiteInit(const Edge *edges, int edgesLen, color *colors, int colorsLen, int weight, int i,
                          int remainingWeight, vector<State> &states) {
    if (i > SEQUENTIAL_LIMIT) {
        auto *cpEdges = new Edge[edgesLen];
        std::copy(edges, edges + edgesLen, cpEdges);

        auto *cpColors = new color[colorsLen];
        std::copy(colors, colors + colorsLen, cpColors);
        states.emplace_back(cpColors, colorsLen, weight, i, remainingWeight);
        return;
    }

    handleEdgeInit(edges, edgesLen, colors, colorsLen, weight, i, remainingWeight, edges[i].a, edges[i].b, states);
    handleEdgeInit(edges, edgesLen, colors, colorsLen, weight, i, remainingWeight, edges[i].b, edges[i].a, states);
    maximumBipartiteInit(edges, edgesLen, colors, colorsLen, weight, i + 1, remainingWeight - edges[i].weight, states);
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
                    for (int j = 0; j < colorsLen; ++j) {
                        bestConfiguration[j] = colors[j];
                    }
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


void handleIncomingMessage(Outcome &maximum, MPI_Status &status) {
    Outcome outcome{};
    MPI_Recv(&outcome, sizeof(Outcome), MPI_CHAR, MPI_ANY_SOURCE, TAG_DONE, MPI_COMM_WORLD, &status);
    if (outcome.maxWeight > maximum.maxWeight) {
        maximum = outcome;
    }
}

int main(int argc, char *argv[]) {
    MPI_Init(&argc, &argv);

    int processRank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &processRank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (processRank == 0) {
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

        int edgesSize = static_cast<int>(sizeof(Edge)) * edgesLen;
        // send edges to slave processes
        for (int i = 1; i < size; ++i) {
            MPI_Send(&edgesLen, 1, MPI_INT, i, TAG_STOP, MPI_COMM_WORLD);
            MPI_Send(edges, edgesSize, MPI_CHAR, i, TAG_TASK_EDGES, MPI_COMM_WORLD);
        }

        auto *colors = new color[nodeCount];
        for (int i = 0; i < nodeCount; ++i) {
            colors[i] = NO_COLOR;
        }

        auto start = chrono::high_resolution_clock::now();
        auto states = vector<State>();
        maximumBipartiteInit(edges, edgesLen, colors, nodeCount, 0, 0, maxWeight, states);

        Outcome maximum{};

        for (int i = 1; i <= states.size(); ++i) {
            int target = i;
            if (i >= size) {
                MPI_Status status;
                handleIncomingMessage(maximum, status);
                target = status.MPI_SOURCE;
            }
            State *toSend = &states[i];
            toSend->bestAchievedWeight = maximum.maxWeight;
            MPI_Send(&states[i], sizeof(State), MPI_CHAR, target, TAG_TASK_INIT, MPI_COMM_WORLD);
        }
        // finish
        for (int i = 1; i < size; ++i) {
            MPI_Status status;
            handleIncomingMessage(maximum, status);
            int tmp = 0;
            MPI_Send(&tmp, 1, MPI_INT, status.MPI_SOURCE, TAG_STOP, MPI_COMM_WORLD);
        }

        cout << "evalution time: "
             << chrono::duration_cast<std::chrono::milliseconds>(chrono::high_resolution_clock::now() - start).count()
             << "ms" << endl;

        cout << "result: " << maximum.maxWeight << endl;
        for (int i = 0; i < nodeCount; ++i) {
            cout << (int) maximum.colorsConfiguration[i] << ", ";
        }
        cout << endl;

    } else {
        // receive edges from master
        int edgesLen;
        MPI_Recv(&edgesLen, 1, MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        int bufferSize = static_cast<int>(sizeof(Edge)) * edgesLen;
        char *buffer = new char[bufferSize];
        MPI_Recv(buffer, bufferSize, MPI_CHAR, 0, MPI_ANY_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        Edge *edges = (Edge *) buffer;

        while (true) {
            MPI_Status status;
            State state{};
            MPI_Recv(&state, sizeof(State), MPI_CHAR, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
            if (status.MPI_TAG == TAG_STOP) {
                break;
            }
            // share best weight
            maxWeightAchieved = state.bestAchievedWeight;

            #pragma omp parallel default(none) shared(edges, edgesLen, state, bestConfiguration) num_threads(6)
            {
                #pragma omp single
                {
                    maximumBipartite(edges, edgesLen, state.colors, state.colorsLen, state.weight, state.i,
                                     state.remainingWeight);
                }
            }

            Outcome outcome{maxWeightAchieved};
            for (int i = 0; i < state.colorsLen; ++i) {
                outcome.colorsConfiguration[i] = bestConfiguration[i];
            }

            MPI_Send(&outcome, sizeof(Outcome), MPI_CHAR, 0, TAG_DONE, MPI_COMM_WORLD);
        }
    }

    MPI_Finalize();
    return 0;
}


C:\Windows\system32\wsl.exe --distribution Ubuntu --exec /bin/bash -c "cd /mnt/c/Users/matfr/CLionProjects/NI_PDP && /usr/bin/mpirun -np 4 ./cmake-build-wsl/NI_PDP graf_bpo/graf_10_6.txt"
evalution time: 3024ms
result: 2000
-1, 1, 1, 1, 1, -1, -1, -1, -1, 1,

Process finished with exit code 0