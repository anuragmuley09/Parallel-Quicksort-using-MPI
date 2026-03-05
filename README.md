# HPC Mini Project - Parallel Quicksort using MPI

## Table of Contents
1. [Group Members](#group-members)
2. [Project Goal](#project-goal)
3. [What is MPI?](#what-is-mpi)
4. [How This Project Works](#how-this-project-works)
5. [Code Walkthrough](#code-walkthrough)
6. [Requirements](#requirements)
7. [Build and Run (Windows + MS-MPI)](#build-and-run-windows--ms-mpi)
8. [Sample Runs](#sample-runs)
9. [Troubleshooting](#troubleshooting)
10. [FAQ](#faq)

---

## Group Members

- Anurag Muley (22120087)
- Arya Lingayat (22120084)
- Rohan Mali (22120080)
- Divija Arjunwadkar (22000029)

---

## Project Goal

This mini project compares sorting work done in parallel using MPI processes.

We implement:
- quicksort logic
- distributed data splitting
- process-to-process data exchange
- final gather of sorted data on rank 0 (main process)

---

## What is MPI?

MPI stands for **Message Passing Interface**.

In simple words:
- You run multiple processes (not threads).
- Every process has its own memory.
- Processes talk by sending/receiving messages.

Important MPI words:
- **Rank**: process id in MPI world (`0, 1, 2, ...`).
- **World size**: total number of processes.
- **Rank 0**: usually the controller process (generates input, prints final output).
- **Broadcast**: send same value to all ranks.
- **Scatter**: split data from rank 0 to all ranks.
- **Gather/Gatherv**: collect data from all ranks to rank 0.
- **Communicator**: a group of processes that can communicate (for recursive splitting).

---

## How This Project Works

High-level flow:

1. Rank 0 reads CLI args (`-y/-n`, `size`) and creates random vector.
2. Vector is split across processes with `MPI_Scatterv`.
3. Processes run recursive distributed quicksort:
   - choose pivot
   - partition local data into `< pivot` and `>= pivot`
   - exchange wrong-side partitions with partner process
   - split communicator into two groups and recurse
4. At base case (group size = 1), local chunk is sorted.
5. Rank 0 gathers all sorted chunks using `MPI_Gatherv`.
6. Rank 0 prints timing and optional sorted data.

---

## Code Walkthrough

### 1) `main.cpp`

Main responsibilities:
- Initialize MPI (`MPI_Init`)
- Get `worldRank`, `worldSize`
- Validate arguments on rank 0
- Broadcast validation/status and parameters
- Start timer (`MPI_Wtime`)
- Call `parallelQuicksortMPI(nums, worldRank, worldSize)`
- Print time and optional output on rank 0
- Finalize MPI (`MPI_Finalize`)

Why rank 0 does input parsing:
- Avoids every process parsing same CLI independently
- Ensures single source of truth
- Broadcast shares validated values with all ranks

### 2) `helpers.cpp`

Key functions:

- `getRandomVector(int size)`
  - Generates random integers using Mersenne Twister.

- `printVector(vector<int>& nums)`
  - Prints vector values.

- `partition(vector<int>& nums, int low, int high)`
  - Standard quicksort partition.

- `normalQuicksort(vector<int>& nums, int low, int high)`
  - Standard recursive quicksort (single-process helper).

- `parallelQuicksortMPI(vector<int>& nums, int worldRank, int worldSize)`
  - Global orchestrator for distributed sorting.
  - Uses scatter -> recursive distributed sort -> gather.

- `recursiveDistributedQuicksort(vector<int>& local, MPI_Comm comm)`
  - Core recursive parallel quicksort over communicator groups.
  - Picks pivot from sampled local values.
  - Partitions local data.
  - Exchanges data with partner rank.
  - Splits communicator and recurses.

- `exchangePartitions(...)`
  - Safe two-way exchange of variable-size arrays with `MPI_Sendrecv`.

---

## Requirements

For this repository setup on Windows:

- Visual Studio C++ tools (`cl`)
- MS-MPI Runtime
- MS-MPI SDK

Detected MS-MPI paths on this machine:

- Include: `C:\Program Files (x86)\Microsoft SDKs\MPI\Include`
- Library (x64): `C:\Program Files (x86)\Microsoft SDKs\MPI\Lib\x64`
- `mpiexec`: `C:\Program Files\Microsoft MPI\Bin\mpiexec.exe`

---

## Build and Run (Windows + MS-MPI)

Open **x64 Native Tools Command Prompt for VS** or a shell where `cl` works.

### Build

```bat
cl /EHsc /I"C:\Program Files (x86)\Microsoft SDKs\MPI\Include" main.cpp /link /LIBPATH:"C:\Program Files (x86)\Microsoft SDKs\MPI\Lib\x64" msmpi.lib
```

This generates `out.exe`.

### Run

```bat
"C:\Program Files\Microsoft MPI\Bin\mpiexec.exe" -n 4 out.exe -n 100000
```

CLI arguments:
- First arg: `-y` or `-n`
  - `-y`: print arrays
  - `-n`: do not print arrays
- Second arg: vector size (positive integer)

Example:

```bat
"C:\Program Files\Microsoft MPI\Bin\mpiexec.exe" -n 4 out.exe -y 30
```

Optional: add MS-MPI bin to PATH in current shell:

```bat
set PATH=%PATH%;C:\Program Files\Microsoft MPI\Bin
```

Then you can use:

```bat
mpiexec -n 4 out.exe -n 100000
```

---

## Sample Runs

Small debug run:

```bat
mpiexec -n 4 out.exe -y 20
```

Performance run:

```bat
mpiexec -n 4 out.exe -n 1000000
```

---

## Troubleshooting

### Error: `Cannot open include file: 'mpi.h'`

Cause:
- Wrong include path
- SDK not installed

Fix:
- Verify file exists:

```bat
dir "C:\Program Files (x86)\Microsoft SDKs\MPI\Include\mpi.h"
```

- Compile with correct `/I` path shown above.

### Error: `cannot open file 'msmpi.lib'`

Cause:
- Wrong library path or wrong architecture

Fix:
- Verify:

```bat
dir "C:\Program Files (x86)\Microsoft SDKs\MPI\Lib\x64\msmpi.lib"
```

- Use `/LIBPATH` with that directory.

### Error: `'mpiexec' is not recognized`

Cause:
- `mpiexec.exe` not in PATH

Fix:
- Use full path:

```bat
"C:\Program Files\Microsoft MPI\Bin\mpiexec.exe" -n 4 out.exe -n 1000
```

- Or add to PATH for session.

### Program runs but no speedup

Possible reasons:
- Input too small
- Too many prints (`-y`)
- Process count too high for data size
- Single machine core limits

Try:
- `-n` mode
- Larger arrays (e.g. `1e6`+)
- Test with 2, 4, 8 processes

### Wrong output ordering

The implementation is designed to gather globally ordered partitions after recursive communicator split.
If you still suspect mismatch:
- run tiny case with `-y 20`
- compare with serial sort output for same seed (or add deterministic seed)

---

## FAQ

### 1. Why include `helpers.cpp` directly in `main.cpp`?

Current codebase keeps everything simple in two files.
In production, this should be split into `.h` + `.cpp` files.

### 2. Why rank 0 only for printing?

If every rank prints, console output becomes unreadable.
Rank 0 prints final merged result.

### 3. MPI vs OpenMP?

- MPI: multiple processes, separate memory, message passing.
- OpenMP: mostly shared-memory threading within one process.

### 4. Do I need Linux?

No. This project works on Windows with MS-MPI + MSVC.

### 5. Can I compile with plain `g++`?

Not for MPI version. `mpi.h` and MPI libraries are required.

### 6. Is this perfectly optimized quicksort?

No. It is a clean educational parallel version.
You can improve pivot selection, load balancing, and communication strategy further.

