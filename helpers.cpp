#include <iostream>
#include <vector>
#include <random>
#include <climits>
#include <chrono>
#include <algorithm>
#include <stdexcept>
#include <mpi.h>

using namespace std;

/**
 * Helpers.
 * 
 */

vector<int> getRandomVector(int size);
void executeAndCalculateElapsedTime(vector<int> nums, void (*sortMethod)(vector<int>&, int, int));
void printVector(vector<int>& nums);


void normalQuicksort(vector<int>& nums, int low, int high);
int partition(vector<int>& nums, int low, int high);
void parallelQuicksortMPI(vector<int>& nums, int worldRank, int worldSize);

static void recursiveDistributedQuicksort(vector<int>& local, MPI_Comm comm);
static void exchangePartitions(
    vector<int>& keepPart,
    const vector<int>& sendPart,
    int partner,
    MPI_Comm comm
);




vector<int> getRandomVector(int size) {
    vector<int> nums;
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<int> dist(INT_MIN, INT_MAX);
    while(size--) nums.push_back(dist(gen));
    return nums;   
}

void printVector(vector<int>& nums) {
    for(const int& num : nums) cout << num << " ";
    cout << "\n";
}

void executeAndCalculateElapsedTime(vector<int> nums, void (*sortMethod)(vector<int>&, int, int)) {
    using Clock = std::chrono::steady_clock;
    using std::chrono::duration_cast;
    using std::chrono::milliseconds;

    // Capture the start time
    Clock::time_point start = Clock::now();

    sortMethod(nums, 0, nums.size()-1); 

    // Capture the end time
    Clock::time_point end = Clock::now();

    // Calculate the difference and cast to desired units
    milliseconds diff = duration_cast<milliseconds>(end - start);

    std::cout << "Operation took " << diff.count() << "ms" << std::endl;
}

int partition(vector<int>& nums, int low, int high) {
    int pivot = nums[high];
    int i = low - 1;

    for(int j=low;j<=high-1;j++){
        if(nums[j] < pivot) {
            i++;
            swap(nums[i], nums[j]);
        }
    }

    swap(nums[i+1], nums[high]);
    return i+1;
}

void normalQuicksort(vector<int>& nums, int low, int high) {
    if(low < high) {
        int index = partition(nums, low, high);

        normalQuicksort(nums, low, index - 1);
        normalQuicksort(nums, index+1, high);
    }
}

static void exchangePartitions(
    vector<int>& keepPart,
    const vector<int>& sendPart,
    int partner,
    MPI_Comm comm
) {
    if (partner == MPI_PROC_NULL) {
        return;
    }

    int sendCount = static_cast<int>(sendPart.size());
    int recvCount = 0;

    MPI_Sendrecv(
        &sendCount,
        1,
        MPI_INT,
        partner,
        0,
        &recvCount,
        1,
        MPI_INT,
        partner,
        0,
        comm,
        MPI_STATUS_IGNORE
    );

    vector<int> recvPart(recvCount);

    MPI_Sendrecv(
        sendCount > 0 ? const_cast<int*>(sendPart.data()) : nullptr,
        sendCount,
        MPI_INT,
        partner,
        1,
        recvCount > 0 ? recvPart.data() : nullptr,
        recvCount,
        MPI_INT,
        partner,
        1,
        comm,
        MPI_STATUS_IGNORE
    );

    keepPart.insert(keepPart.end(), recvPart.begin(), recvPart.end());
}

static void recursiveDistributedQuicksort(vector<int>& local, MPI_Comm comm) {
    int rank = 0;
    int size = 0;
    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &size);

    if (size == 1) {
        sort(local.begin(), local.end());
        return;
    }

    int localPivot = 0;
    int hasLocal = !local.empty();
    if (hasLocal) {
        localPivot = local[local.size() / 2];
    }

    vector<int> allPivots;
    vector<int> allHasData;
    if (rank == 0) {
        allPivots.resize(size);
        allHasData.resize(size);
    }

    MPI_Gather(&localPivot, 1, MPI_INT, rank == 0 ? allPivots.data() : nullptr, 1, MPI_INT, 0, comm);
    MPI_Gather(&hasLocal, 1, MPI_INT, rank == 0 ? allHasData.data() : nullptr, 1, MPI_INT, 0, comm);

    int pivot = 0;
    if (rank == 0) {
        vector<int> validPivots;
        validPivots.reserve(size);
        for (int i = 0; i < size; ++i) {
            if (allHasData[i]) {
                validPivots.push_back(allPivots[i]);
            }
        }

        if (!validPivots.empty()) {
            sort(validPivots.begin(), validPivots.end());
            pivot = validPivots[validPivots.size() / 2];
        }
    }

    MPI_Bcast(&pivot, 1, MPI_INT, 0, comm);

    vector<int> lower;
    vector<int> upper;
    lower.reserve(local.size());
    upper.reserve(local.size());

    for (int value : local) {
        if (value < pivot) {
            lower.push_back(value);
        } else {
            upper.push_back(value);
        }
    }

    int split = size / 2;
    int color = (rank < split) ? 0 : 1;
    int partner = MPI_PROC_NULL;

    if (rank < split) {
        partner = rank + split;
        if (partner >= size) {
            partner = MPI_PROC_NULL;
        }

        local = move(lower);
        exchangePartitions(local, upper, partner, comm);
    } else {
        partner = rank - split;
        if (partner >= split) {
            partner = MPI_PROC_NULL;
        }
        local = move(upper);
        exchangePartitions(local, lower, partner, comm);
    }

    MPI_Comm subComm = MPI_COMM_NULL;
    MPI_Comm_split(comm, color, rank, &subComm);
    recursiveDistributedQuicksort(local, subComm);
    MPI_Comm_free(&subComm);
}

void parallelQuicksortMPI(vector<int>& nums, int worldRank, int worldSize) {
    int n = 0;
    if (worldRank == 0) {
        n = static_cast<int>(nums.size());
    }
    MPI_Bcast(&n, 1, MPI_INT, 0, MPI_COMM_WORLD);

    vector<int> sendCounts(worldSize, 0);
    vector<int> displs(worldSize, 0);

    if (worldRank == 0) {
        int base = n / worldSize;
        int rem = n % worldSize;
        int offset = 0;
        for (int i = 0; i < worldSize; ++i) {
            sendCounts[i] = base + (i < rem ? 1 : 0);
            displs[i] = offset;
            offset += sendCounts[i];
        }
    }

    int localN = 0;
    MPI_Scatter(sendCounts.data(), 1, MPI_INT, &localN, 1, MPI_INT, 0, MPI_COMM_WORLD);

    vector<int> local(localN);
    MPI_Scatterv(
        worldRank == 0 ? nums.data() : nullptr,
        sendCounts.data(),
        displs.data(),
        MPI_INT,
        localN > 0 ? local.data() : nullptr,
        localN,
        MPI_INT,
        0,
        MPI_COMM_WORLD
    );

    recursiveDistributedQuicksort(local, MPI_COMM_WORLD);

    int localSortedCount = static_cast<int>(local.size());
    vector<int> recvCounts(worldRank == 0 ? worldSize : 0);

    MPI_Gather(
        &localSortedCount,
        1,
        MPI_INT,
        worldRank == 0 ? recvCounts.data() : nullptr,
        1,
        MPI_INT,
        0,
        MPI_COMM_WORLD
    );

    vector<int> recvDispls;
    int total = 0;
    if (worldRank == 0) {
        recvDispls.resize(worldSize);
        for (int i = 0; i < worldSize; ++i) {
            recvDispls[i] = total;
            total += recvCounts[i];
        }
        nums.assign(total, 0);
    }

    MPI_Gatherv(
        localSortedCount > 0 ? local.data() : nullptr,
        localSortedCount,
        MPI_INT,
        worldRank == 0 ? nums.data() : nullptr,
        worldRank == 0 ? recvCounts.data() : nullptr,
        worldRank == 0 ? recvDispls.data() : nullptr,
        MPI_INT,
        0,
        MPI_COMM_WORLD
    );
}
