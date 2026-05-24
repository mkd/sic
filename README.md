# Sic Chess Engine

Sic is an ultra-high-performance, UCI-compliant chess engine written in Modern C++20. Designed for speed, cache efficiency, and scalability, Sic leverages a highly optimized custom search architecture paired with a neural network evaluation core.

**Author:** Claudio M. Camacho (<claudiomkd@gmail.com>)

## Core Architecture

* **Language:** Modern C++20 (Cross-platform support including Linux and macOS Apple Silicon).
* **Evaluation:** NNUE (Efficiently Updatable Neural Networks). Sic uses a highly optimized C++ bridge for the Stockfish 16.1 legacy `HalfKP` architecture (20MB network: `nn-62ef826d1a6d.nnue`). It features a 256-dimension incremental accumulator that updates purely on the differences between moves, guaranteeing massive Nodes-Per-Second (NPS) throughput.
* **Concurrency:** Lazy SMP (Symmetric Multiprocessing). Threads search the same tree concurrently, sharing data locklessly to naturally distribute the workload.
* **Transposition Table:** 100% Lockless Hash Table with dynamic UCI resizing (`setoption name Hash`) and `hashfull` telemetry. Memory layout uses a Cache-Aligned Multi-Bucket architecture (64-byte `TTCluster` matching L1 cache lines) and Generational Aging. Features aggressive `__builtin_prefetch` instructions to hide memory access latency.
* **Endgame Tablebases:** Seamless `Fathom` integration for 6-piece Syzygy tablebases, allowing the engine to instantly prove wins/draws/losses without searching.
* **Board Representation:** Magic Bitboards (Carry-Rippler initialization) for blazingly fast move generation and attack detection.
* **Time Management:** Advanced Soft/Hard limits with Node Stability Extensions. Calculates optimum search time and gracefully halts between iterative deepening iterations to preserve PV integrity.
* **Advanced Heuristics:** Features dynamic threat extractors (`checkers`, `pinners`, `blockersForKing`) calculated natively on the C++ bitboards to inform search pruning.

## Search & Pruning Features
Sic features a highly aggressive, state-of-the-art search tree designed to heavily prune unpromising branches and maximize depth penetration:

### Search Algorithm
* **Principal Variation Search (PVS / NegaScout):** Assumes perfect move ordering to search the first move with a full window and subsequent moves with ultra-fast zero-windows.
* **Aspiration Windows:** Searches at the root are conducted with a narrow window around the previous iteration's score, rapidly proving fail-highs or fail-lows to prune the tree early.
* **Iterative Deepening:** Progressively deepens the search to ensure the best move is available if time runs out.
* **Internal Iterative Deepening (IID):** At deep nodes missing a TT entry, dynamically executes a shallow search explicitly to find a good principal variation move rather than relying on static ordering.
* **Quiescence Search (QS):** Resolves tactical sequences at the end of the main search to avoid the horizon effect. Features **Delta Pruning** to outright prune captures that mathematically cannot raise the score above alpha.

### Move Ordering
* **TT-Move Prioritization:** Instantly searches the best move found in previous iterations.
* **MVV-LVA (Most Valuable Victim - Least Valuable Attacker):** Orders captures efficiently.
* **SEE Capture Ordering:** Uses Static Exchange Evaluation to severely penalize materially losing captures, pushing them to the bottom of the move list to be easily pruned.
* **Countermove Heuristic:** Prioritizes the historical best response to the opponent's previous move.
* **Killer Move Heuristic:** Tracks moves that recently caused beta-cutoffs at the same ply.
* **History Heuristic (Butterfly Boards):** Dynamically rewards quiet moves that cause cutoffs globally across the search tree, penalizing those that fail.

### Forward Pruning & Reductions
* **ProbCut:** The main search executes shallow, high-beta searches (`beta + 200`) at deep nodes. If this heavily biased search fails high, we probabilistically assume the full-depth search will fail high, cutting off massive chunks of the tree early.
* **Dynamic Null Move Pruning (NMP):** Passes the turn to the opponent to prove a position is overwhelmingly winning. The depth reduction is scaled dynamically with search depth (`R = 3 + depth / 4`).
* **Reverse Futility Pruning (RFP / Static NMP):** Instantly returns static evaluation if the position is far above the beta threshold.
* **Razoring:** At very low depths, if the static evaluation is significantly below the alpha threshold, immediately drops into Quiescence Search to prune unpromising subtrees.
* **Futility Pruning (FP) & Late Move Pruning (LMP):** Skips quiet moves at low depths that cannot mathematically improve the position or are too far down the ordered list.
* **Logarithmic Late Move Reductions (LMR):** Aggressively reduces the search depth of late-ordered moves based on a mathematically principled logarithmic formula. This reduction is softened for moves with a very high historical success rate.
* **History Pruning:** Outright prunes quiet moves at low depths if they have a terribly negative historical success rate.
* **Static Exchange Evaluation (SEE) Pruning:** Simulates captures statically to prune materially losing sequences in both the Main Search and Quiescence Search.
* **Improving Heuristic:** Dynamically relaxes pruning margins if the position's static evaluation is worsening compared to two plies ago.

## Compiling and Running

**Dependencies:**
* A C++20 compatible compiler (GCC/Clang)
* Make

**Build:**
```bash
make build -j

Running (UCI Mode):
Sic is designed to be plugged into any standard UCI GUI (like Cute Chess, Arena, or En Croissant).
Bash

./sic
```

Internal Diagnostic Commands

If running directly from the terminal, Sic supports custom Stockfish-style diagnostic commands:

    d: Displays a high-quality ASCII representation of the current board, FEN string, Zobrist Key, and Checkers bitboard.

    eval: Prints the raw static NNUE evaluation of the current position in centipawns.

Note: Sic requires the nn-62ef826d1a6d.nnue file in its root directory to evaluate positions.
