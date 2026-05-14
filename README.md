# Sic Chess Engine

Sic is an ultra-high-performance, UCI-compliant chess engine written in Modern C++20. Designed for speed, cache efficiency, and scalability, Sic leverages a highly optimized custom search architecture paired with a neural network evaluation core.

**Author:** Claudio M. Camacho (<claudiomkd@gmail.com>)

## Core Architecture

* **Language:** Modern C++20
* **Evaluation:** NNUE (Efficiently Updatable Neural Networks). Sic uses a highly optimized C++ bridge for the Stockfish 16.1 legacy `HalfKP` architecture (20MB network: `nn-62ef826d1a6d.nnue`). It features a 256-dimension incremental accumulator that updates purely on the differences between moves, guaranteeing massive Nodes-Per-Second (NPS) throughput.
* **Concurrency:** Lazy SMP (Symmetric Multiprocessing). Threads search the same tree concurrently, sharing data locklessly to naturally distribute the workload.
* **Transposition Table:** 100% Lockless Hash Table. Optimized for multi-threaded access without thread contention, allowing linear NPS scaling.
* **Board Representation:** Magic Bitboards (Carry-Rippler initialization) for blazingly fast move generation and attack detection.
* **Advanced Heuristics:** Features dynamic threat extractors (`checkers`, `pinners`, `blockersForKing`) calculated natively on the C++ bitboards to inform search pruning.

## Current Search Optimizations
Sic is currently undergoing aggressive algorithmic tuning. Its search tree includes:
* Iterative Deepening
* Quiescence Search (QS)
* Transposition Table (Lockless Probing & Recording)
* Null Move Pruning (NMP)
* Basic Late Move Reductions (LMR)
* Move Ordering (TT-Move prioritization + MVV-LVA)

## Compiling and Running

**Dependencies:** * A C++20 compatible compiler (GCC/Clang)
* Make

**Build:**
```bash
make build -j

Running (UCI Mode):
Sic is designed to be plugged into any standard UCI GUI (like Cute Chess, Arena, or En Croissant).
Bash

./sic

Note: Sic requires the nn-62ef826d1a6d.nnue file in its root directory to evaluate positions.
