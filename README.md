# Sic Chess Engine

Sic is an ultra-high-performance, UCI-compliant chess engine written in Modern C++20. Designed for speed, cache efficiency, and scalability, Sic leverages a highly optimized custom search architecture paired with a neural network evaluation core.

**Author:** Claudio M. Camacho (<claudiomkd@gmail.com>)

## Core Architecture

* **Language:** Modern C++20 (Cross-platform support including Linux and macOS Apple Silicon).
* **Evaluation:** NNUE (Efficiently Updatable Neural Networks). Sic uses a highly optimized C++ bridge incorporating Stockfish 16.1's native `SFNNv10` architecture. It features a 2560-dimension incremental accumulator (`HalfKAv2_hm`) that updates purely on the differences between moves, guaranteeing massive Nodes-Per-Second (NPS) throughput. Supports dual-network inference (Big/Small), defaulting to the fast 3.4MB Small network (`nn-baff1ede1f90.nnue`) for lightning-fast node evaluations, configurable via UCI options.
* **Concurrency:** Lazy SMP (Symmetric Multiprocessing) up to 128 threads. Threads search the same tree concurrently, utilizing Root Move Rotations to naturally distribute workloads and prevent TT cache thrashing, allowing scaling to millions of nodes per second.
* **Transposition Table:** 100% Lockless Hash Table with dynamic UCI resizing (`setoption name Hash`) and `hashfull` telemetry. Features TT Draw Bug prevention (safeguarding exact bounds on repetitions) and aggressive `__builtin_prefetch` instructions to hide memory access latency.
* **Endgame Tablebases:** Seamless `Fathom` integration for 6-piece Syzygy tablebases, allowing the engine to instantly prove wins/draws/losses without searching.

## Search & Pruning Features
Sic features a highly aggressive, state-of-the-art search tree designed to heavily prune unpromising branches and maximize depth penetration:

### Search Algorithm
* **Principal Variation Search (PVS / NegaScout):** Assumes perfect move ordering to search the first move with a full window and subsequent moves with ultra-fast zero-windows.
* **Progressive Widening Aspiration Windows:** Searches at the root are conducted with a narrow window around the previous iteration's score. If a search fails high or low, Sic utilizes progressive widening to exponentially expand the window and re-search, retaining extraordinarily tight bounds and preventing search tree explosions.
* **Iterative Deepening:** Progressively deepens the search to ensure the best move is available if time runs out.
* **Quiescence Search (QS):** Resolves tactical sequences at the end of the main search to avoid the horizon effect. Features **Delta Pruning** to outright prune captures that mathematically cannot raise the score above alpha.

### Move Ordering
* **TT-Move Prioritization:** Instantly searches the best move found in previous iterations.
* **MVV-LVA (Most Valuable Victim - Least Valuable Attacker):** Orders captures efficiently.
* **SEE Capture Ordering:** Uses Static Exchange Evaluation to severely penalize materially losing captures, pushing them to the bottom of the move list to be easily pruned.
* **Capture History Heuristic:** Dynamically fine-tunes MVV-LVA by tracking the historical success of specific captures (`Attacker -> Victim -> ToSquare`), ensuring the most promising tactical sequences are explored first.
* **Countermove Heuristic:** Prioritizes the historical best response to the opponent's previous move.
* **Killer Move Heuristic:** Tracks moves that recently caused beta-cutoffs at the same ply.
* **History Heuristic & Continuation History:** Tracks the global success of quiet moves. Sic employs 1-Ply and 2-Ply Continuation History arrays, tracking move success chronologically against the opponent's immediate previous move and our move from 2 plies ago, mapping deep strategic patterns.

### Forward Pruning & Reductions
* **ProbCut:** Fast tactical searches (`beta + 200`) at deep nodes. Sic restricts ProbCut generation to only evaluate tactical captures, probabilistically predicting and cutting off massive unpromising branches at extremely low cost.
* **Dynamic Null Move Pruning (NMP):** Passes the turn to the opponent to prove a position is overwhelmingly winning. The depth reduction is scaled dynamically and kicks in aggressively early in the search tree (`depth >= 2`).
* **Reverse Futility Pruning (RFP / Static NMP):** Instantly returns static evaluation if the position is far above the beta threshold.
* **Razoring:** At very low depths, if the static evaluation is significantly below the alpha threshold, immediately drops into Quiescence Search.
* **Futility Pruning (FP) & Late Move Pruning (LMP):** Skips quiet moves at low depths that cannot mathematically improve the position. Features extremely tight LMP thresholds matched against modern engine standards.
* **Dynamic StatScore LMR Scaling:** Aggressively reduces the search depth of late-ordered quiet moves based on a mathematically principled logarithmic formula. Sic aggregates Main History and Continuation History into a unified `StatScore`, dynamically expanding the depth for exceptionally promising moves and instantly pruning historically poor sequences via linear LMR scaling (`reduction -= stat_score / 4000`).
* **History Pruning:** Outright prunes quiet moves at low depths if they have a terribly negative historical StatScore.
* **Static Exchange Evaluation (SEE) Pruning:** Simulates captures statically to prune materially losing sequences.
* **Improving Heuristic:** Dynamically relaxes pruning margins if the position's static evaluation is worsening.

## Evaluation & Scaling
Sic features a hyper-accurate implementation of Stockfish's NNUE architecture. A critical design decision in Sic is the native preservation of **internal NNUE units** (`~322 = 1 pawn`) throughout the entirety of the search algorithm.
* **Calibrated Pruning Margins:** Because the static evaluation remains unscaled natively, Sic's internal static pruning margins (e.g., Futility Pruning, Razoring) perfectly align with the magnitude of the evaluation. This unlocks extraordinarily aggressive pruning capabilities, drastically collapsing the size of the search tree.
* **WDL Score Translation:** Sic translates its raw internal evaluation into standard UCI Centipawns exclusively at the printing stage via a state-of-the-art Win-Draw-Loss (WDL) model. This ensures that the GUI outputs highly accurate `score cp` readouts that reflect the actual win probability based on material and game phase, avoiding the inflated scores seen in engines that naïvely divide by static constants.

## Compiling and Running

**Dependencies:**
* A C++20 compatible compiler (GCC/Clang)
* Make

**Build:**
```bash
make build -j
```

**Running (UCI Mode):**
Sic is designed to be plugged into any standard UCI GUI (like Cute Chess, Arena, or En Croissant).
```bash
./sic
```

**Single-Shot Commands:**
Sic can execute CLI commands and exit immediately:
```bash
./sic go perft 6
./sic go movetime 5000
```

**Internal Diagnostic Commands:**
If running directly from the terminal, Sic supports custom diagnostic commands:
* `d`: Displays a high-quality ASCII representation of the current board, FEN string, and Zobrist Key.
* `eval`: Prints the raw static NNUE evaluation of the current position in centipawns.
* `moves` / `smoves`: Prints the legal moves generated for the position.

*Note: Sic requires the `nn-baff1ede1f90.nnue` file in its root directory to evaluate positions.*
