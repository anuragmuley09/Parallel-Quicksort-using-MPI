#include <iostream>
#include <string>
#include <mpi.h>
#include "helpers.cpp"

using namespace std;



int main(int argc, char* argv[]) {
    MPI_Init(&argc, &argv);

    int worldRank = 0;
    int worldSize = 0;
    MPI_Comm_rank(MPI_COMM_WORLD, &worldRank);
    MPI_Comm_size(MPI_COMM_WORLD, &worldSize);

    int size = 0;
    int showVector = 0;
    vector<int> nums;
    int ok = 1;
    if (worldRank == 0) {
        if (argc != 3) {
            ok = 0;
        } else {
            string flag = argv[1];
            if (flag != "-y" && flag != "-n") {
                ok = 0;
            } else {
                showVector = (flag == "-y") ? 1 : 0;
                try {
                    size = stoi(argv[2]);
                } catch (...) {
                    ok = 0;
                }
                if (size <= 0) {
                    ok = 0;
                }
            }
        }

        if (ok) {
            cout << "Processes: " << worldSize << endl;
            cout << "Flag: " << (showVector ? "-y" : "-n") << endl;
            cout << "Size: " << size << endl;

            nums = getRandomVector(size);

            if(showVector) {
                cout << "Unsorted:\n";
                printVector(nums);
            }
        }
    }

    MPI_Bcast(&ok, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&size, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&showVector, 1, MPI_INT, 0, MPI_COMM_WORLD);

    if (!ok) {
        if (worldRank == 0) {
            cerr << "USAGE: mpirun -np <processes> ./out <show_vector_flag> <number_of_elements>" << endl;
        }
        MPI_Finalize();
        return 1;
    }

    MPI_Barrier(MPI_COMM_WORLD);
    double start = MPI_Wtime();
    parallelQuicksortMPI(nums, worldRank, worldSize);
    MPI_Barrier(MPI_COMM_WORLD);
    double end = MPI_Wtime();

    if (worldRank == 0) {
        cout << "Parallel MPI quicksort took " << ((end - start) * 1000.0) << "ms" << endl;
        if (showVector) {
            cout << "Sorted:\n";
            printVector(nums);
        }
    }

    MPI_Finalize();
    return 0;
}
