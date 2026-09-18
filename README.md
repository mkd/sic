# Sic 2.0 Chess Engine

Sic is an ultra-high-performance, UCI-compliant chess engine written in Modern C++20. Designed for speed, cache efficiency, and scalability, Sic leverages a highly optimized custom search architecture paired with a neural network evaluation core. 

With Version 2.0, Sic has undergone a **Huge Upgrade** incorporating Stockfish 18/19 level search heuristics, massive multi-dimensional history mapping, and surgically precise pruning, allowing it to evaluate positions with extreme depth and stability.

**Author:** Claudio M. Camacho (<claudiomkd@gmail.com>)

## Core Architecture

* **Language:** Modern C++20 (Cross-platform support including Linux and macOS Apple Silicon).
* **Evaluation:** NNUE (Efficiently Updatable Neural Networks). Sic uses a highly optimized C++ bridge incorporating Stockfish 18/19 native architecture. It features a 2560-dimension incremental accumulator (`HalfKAv2_hm`) that updates purely on the differences between moves, guaranteeing massive Nodes-Per-Second (NPS) throughput.
* **Concurrency:** Lazy SMP (Symmetric Multiprocessing) up to 128 threads. Threads search the same tree concurrently, utilizing Root Move Rotations to naturally distribute workloads and prevent TT cache thrashing, allowing scaling to millions of nodes per second.
* **Transposition Table:** 100% Lockless Hash Table with dynamic UCI resizing (`setoption name Hash`) and `hashfull` telemetry. Features TT Draw Bug prevention (safeguarding exact bounds on repetitions) and aggressive `__builtin_prefetch` instructions to hide memory access latency.
* **Endgame Tablebases:** Seamless `Fathom` integration for 6-piece Syzygy tablebases, allowing the engine to instantly prove wins/draws/losses without searching.

## Search & Pruning Features
Sic features a highly aggressive, state-of-the-art search tree designed to heavily prune unpromising branches and maximize depth penetration:

### Search Algorithm
* **Principal Variation Search (PVS / NegaScout):** Assumes perfect move ordering to search the first move with a full window and subsequent moves with ultra-fast zero-windows.
* **Progressive Widening Aspiration Windows:** Searches at the root are conducted with a narrow window around the previous iteration's score. If a search fails high or low, Sic utilizes progressive widening to exponentially expand the window and re-search, retaining extraordinarily tight bounds and preventing search tree explosions.
* **Internal Iterative Reductions (IIR):** If no TT move is found at a deep node, Sic automatically reduces the search depth by 1 or 2 plies to find a guiding move rapidly without wasting time on a full-depth unguided search.
* **Singular Extensions (SE):** When the Transposition Table suggests a move that is significantly better than all other alternatives (verified via a shallow search), Sic extends the search depth for that forced line. This ensures Sic deeply analyzes forced sequences instead of blundering due to the horizon effect.
* **Quiescence Search (QS):** Resolves tactical sequences at the end of the main search to avoid the horizon effect. Features **Stockfish-scaled Delta Pruning** to outright prune captures that mathematically cannot raise the score above alpha, and **Strict Check Evasion Pruning** using `SEE >= 0` to prevent tree explosions in complex endgames.

### Move Ordering
* **TT-Move Prioritization:** Instantly searches the best move found in previous iterations.
* **MVV-LVA (Most Valuable Victim - Least Valuable Attacker):** Orders captures efficiently.
* **SEE Capture Ordering:** Uses Static Exchange Evaluation to severely penalize materially losing captures, pushing them to the bottom of the move list to be easily pruned.
* **Capture History Heuristic:** Dynamically fine-tunes MVV-LVA by tracking the historical success of specific captures (`Attacker -> Victim -> ToSquare`), ensuring the most promising tactical sequences are explored first.
* **Killer Move Heuristic:** Tracks moves that recently caused beta-cutoffs at the same ply.
* **Multi-Dimensional Continuation History:** Sic employs **1-Ply, 2-Ply, 4-Ply, and 6-Ply Continuation History arrays**. This maps the success of moves chronologically against the opponent's previous moves, building a massive neural-like understanding of deep strategic patterns and significantly accelerating move ordering.

### Forward Pruning & Reductions
* **Dynamic Null Move Pruning (NMP):** Passes the turn to the opponent to prove a position is overwhelmingly winning. The depth reduction is scaled dynamically and kicks in aggressively early in the search tree (`depth >= 2`).
* **Reverse Futility Pruning (RFP / Static NMP):** Instantly returns static evaluation if the position is far above the beta threshold.
* **Razoring:** At very low depths, if the static evaluation is significantly below the alpha threshold, immediately drops into Quiescence Search.
* **Futility Pruning (FP) & Late Move Pruning (LMP):** Skips quiet moves at low depths that cannot mathematically improve the position. Features extremely tight LMP thresholds matched against modern engine standards.
* **Dynamic StatScore LMR Scaling:** Aggressively reduces the search depth of late-ordered quiet moves based on a mathematically principled logarithmic formula. Sic aggregates Main History and Continuation History into a unified `StatScore`, dynamically expanding the depth for exceptionally promising moves and instantly pruning historically poor sequences via linear LMR scaling.
* **History Pruning:** Outright prunes quiet moves at low depths if they have a terribly negative historical StatScore.
* **Static Exchange Evaluation (SEE) Pruning:** Simulates captures statically to prune materially losing sequences.

## Time Management
* **Gargantua-style Strict Boundaries:** Sic features a deeply overhauled time management system strictly modeled after the rigid boundaries used in Gargantua. It conservatively caps maximum thinking time per move to prevent blowing the clock in the opening. It still incorporates subtle stability-based scaling and node-confidence adjustments to ensure safety in sharp positions without compromising its end-game time reserves.

## Evaluation & Scaling
Sic features a hyper-accurate implementation of Stockfish's NNUE architecture. A critical design decision in Sic is the native preservation of **internal NNUE units** (`~328 = 1 pawn`) throughout the entirety of the search algorithm.
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

*Note: Sic requires the `nn-83a0d6daf7e5.nnue` file in its root directory to evaluate positions.*
