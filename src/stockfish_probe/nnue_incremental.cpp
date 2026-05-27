#include "nnue_incremental.h"
#include "evaluate.h"
#include "probe.h" // for init
#include <iostream>

// Gargantua piece constants (mirrored from bitboard.h/types.h of Gargantua)
// P=0, N=1, B=2, R=3, Q=4, K=5
// p=6, n=7, b=8, r=9, q=10, k=11
// Note: Gargantua uses 0-11.
// Stockfish uses 1-6 (White), 9-14 (Black).

// We need to access Gargantua's move encoding macros.
// However, including "movgen.h" creates circular dependencies or conflicts.
// We will redefine the necessary decoding logic locally or pass decoded
// parameters. For now, we assume we receive the raw int and decode it here. But
// we need the encoding spec. From movgen.h: source: 0x3f target: 0xfc0 >> 6
// piece: 0xf000 >> 12
// promo: 0xf0000 >> 16
// capture: 0x100000
// double: 0x200000
// ep: 0x400000
// castle: 0x800000

#include "../../include/move.h"

// Sic 16-bit Move Encoding:
// bits  0- 5: from
// bits  6-11: to
// bits 12-13: promotion (0=KNIGHT, 1=BISHOP, 2=ROOK, 3=QUEEN) -> but if move_prom(m) != NONE
// bits 14-15: flag (0=Normal, 1=EnPassant, 2=Castling)

// Gargantua squares: a8=0 .. h1=63 (Rank Major Top-Down)
// Stockfish squares: a1=0 .. h8=63 (Rank Major Bottom-Up)
// We don't need MAP_SQ since sic uses Bottom-Up too.

namespace Stockfish {
namespace Incremental {

thread_local Position global_pos;
thread_local StateInfo global_si; // Root state info

// Mapping arrays
// Gargantua pieces (0-11) to Stockfish pieces
static int g_to_sf_piece[12] = {
    1, 2,  3,  4,  5,  6, // P N B R Q K -> W_PAWN..W_KING
    9, 10, 11, 12, 13, 14 // p n b r q k -> B_PAWN..B_KING
};

// Helper to get Stockfish Piece from Gargantua encoded piece
Piece map_piece(int g_piece) {
  if (g_piece >= 0 && g_piece <= 11)
    return Piece(g_to_sf_piece[g_piece]);
  return NO_PIECE;
}

// Reverse map for captures (we need to know what was captured)
// Gargantua move has 'capture' flag but doesn't store captured piece type in 24
// bits. We must rely on our internal board tracking to know what is at the
// target square. Stockfish Position has piece_on(sq).

// Global Stack for Search
thread_local std::vector<StateInfo> search_stack;
thread_local size_t stack_ptr = 0;

void init() {
  // Initialize with startpos
  // TODO: Support FEN initialization if Gargantua supports it
  global_pos.set("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
                 &global_si);

  // Reserve memory for the state stack to prevent reallocations
  // Max depth is usually < 128, but we be safe.
  search_stack.resize(2048);
  stack_ptr = 0;
}

// Global Stack definition moved to top of file

void push_state(int move) {
  if (stack_ptr >= 2048)
    return; // Silent fail? Or assertion.
  StateInfo *si = &search_stack[stack_ptr++];
  // We don't need to memset 0 because do_move sets everything relevant?
  // do_move sets: previous, accum.computed (false), dirty_num(0), rule50,
  // nonPawnMaterial. It does NOT set: key, checkers, etc. (which we identified
  // are missing). But for now, we rely on do_move behavior.
  do_move(move, si);
}

void push_null_state() {
  if (stack_ptr >= 2048)
    return;
  StateInfo *si = &search_stack[stack_ptr++];
  do_null_move(si);
}

void pop_state(int move) {
  undo_move(move);
  if (stack_ptr > 0)
    stack_ptr--;
}

void do_move(int move, StateInfo *new_si) {
  // 1. Link StateInfo
  new_si->previous = global_pos.state();

  // Reset accumulator computed flags (Vital for reused local StateInfo)
  new_si->accumulatorBig.computed[WHITE] = false;
  new_si->accumulatorBig.computed[BLACK] = false;
  new_si->accumulatorSmall.computed[WHITE] = false;
  new_si->accumulatorSmall.computed[BLACK] = false;

  // 2. Decode move
  int from = static_cast<int>(move_from(move));
  int to = static_cast<int>(move_to(move));
  if (from >= 64 || to >= 64) {
    std::cout << "INVALID MOVE DETECTED: " << move << " from: " << from
              << " to: " << to << std::endl;
  }
  
  Square sf_from = Square(from);
  Square sf_to = Square(to);
  Piece sf_piece = global_pos.piece_on(sf_from);
  Piece sf_promo = NO_PIECE;

  int prom_type = static_cast<int>(move_prom(move));
  if (prom_type != 0 /* NONE */) {
      if (prom_type == 2 /* KNIGHT */) sf_promo = make_piece(global_pos.sideToMove, KNIGHT);
      else if (prom_type == 3 /* BISHOP */) sf_promo = make_piece(global_pos.sideToMove, BISHOP);
      else if (prom_type == 4 /* ROOK */) sf_promo = make_piece(global_pos.sideToMove, ROOK);
      else if (prom_type == 5 /* QUEEN */) sf_promo = make_piece(global_pos.sideToMove, QUEEN);
  }

  // 3. Setup DirtyPiece (Incremental Update Data)
  DirtyPiece &dp = new_si->dirtyPiece;
  dp.dirty_num = 0;

  // Logic to update board and dirty pieces
  // A. Move the piece
  Piece captured = global_pos.piece_on(sf_to);

  // Initialize nonPawnMaterial from previous state
  new_si->nonPawnMaterial[WHITE] = new_si->previous->nonPawnMaterial[WHITE];
  new_si->nonPawnMaterial[BLACK] = new_si->previous->nonPawnMaterial[BLACK];

  // Logic for captured piece material update
  bool is_ep = move_flag(move) == MOVE_FLAG_ENPASSANT;
  if (captured != NO_PIECE && type_of(captured) != PAWN && !is_ep) {
    new_si->nonPawnMaterial[color_of(captured)] -= PieceValue[captured];
  }
  // En Passant captures a Pawn, so no NPM change.

  // Handle En Passant Capture
  if (is_ep) {
    // Captured pawn is not on 'to' square, but on 'to +/- 8' (Stockfish
    // perspective: behind) Gargantua 'to' is the empty square. Stockfish 'to'
    // is mapped. The pawn to be removed is: If White moving (sideToMove=0),
    // capturing pawn is 'to + 8' (top-down) -> 'to - 8' (bottom-up)? Wait,
    // MAP_SQ(to + 8) ? Let's rely on sf_to and relative_square logic or simple
    // rank shift. SF: W pawn moves up (rank increases). EP capture is square
    // behind 'to'. W moves +8. EP sq is passed pawn. If we are White, we
    // captured Black pawn at to-8 (SF relative). Actually simpler:
    Square cap_sq = make_square(file_of(sf_to), rank_of(sf_from));
    // EnPassant capture happens on the rank of 'from'.

    captured = global_pos.piece_on(cap_sq);

    // Remove captured pawn
    global_pos.remove_piece(cap_sq);
    dp.piece[dp.dirty_num] = captured;
    dp.from[dp.dirty_num] = cap_sq;
    dp.to[dp.dirty_num] = SQ_NONE;
    dp.dirty_num++;
  } else if (captured != NO_PIECE) {
    // Standard capture
    global_pos.remove_piece(sf_to);
    dp.piece[dp.dirty_num] = captured;
    dp.from[dp.dirty_num] = sf_to;
    dp.to[dp.dirty_num] = SQ_NONE;
    dp.dirty_num++;
  }

  if (global_pos.piece_on(sf_from) == NO_PIECE) {
      std::cout << "CRITICAL ERROR: trying to remove NO_PIECE from " << sf_from << " (move: " << move << ")" << std::endl;
      std::cout << "from=" << from << ", sf_from=" << (int)sf_from << std::endl;
      std::cout << "board state at from: " << global_pos.piece_on(sf_from) << std::endl;
      abort();
  }

  // Remove moving piece from 'from'
  global_pos.remove_piece(sf_from);
  dp.piece[dp.dirty_num] = sf_piece;
  dp.from[dp.dirty_num] = sf_from;
  dp.to[dp.dirty_num] = SQ_NONE;
  dp.dirty_num++;

  // Place moving piece (or promo) at 'to'
  Piece placed = (prom_type != 0) ? sf_promo : sf_piece;

  if (prom_type != 0 && type_of(placed) != PAWN) { // Should be true for promo
    new_si->nonPawnMaterial[color_of(placed)] += PieceValue[placed];
  }

  // Castling
  if (move_flag(move) == MOVE_FLAG_CASTLING) {
    // Move the Rook
    // Gargantua Castling constants/logic in movgen.h
    // We need to detect which rook.
    // White K-side: to=g1 (G argantua). SF: g1.
    // Rook from h1 -> f1.
    // We can treat castling as two piece moves?
    // DirtyPiece supports 3 pieces max. King move + Rook move = 2 pieces.
    // perfect.

    Square r_from = SQ_NONE;
    Square r_to = SQ_NONE;
    if (sf_to == SQ_G1) {
      r_from = SQ_H1;
      r_to = SQ_F1;
    } // W K-side
    else if (sf_to == SQ_C1) {
      r_from = SQ_A1;
      r_to = SQ_D1;
    } // W Q-side
    else if (sf_to == SQ_G8) {
      r_from = SQ_H8;
      r_to = SQ_F8;
    } // B K-side
    else if (sf_to == SQ_C8) {
      r_from = SQ_A8;
      r_to = SQ_D8;
    } // B Q-side

    if (r_from != SQ_NONE) {
      Piece rook = global_pos.piece_on(r_from);
      global_pos.remove_piece(r_from);
      global_pos.put_piece(rook, r_to);

      // Add rook to dirty
      dp.piece[dp.dirty_num] = rook;
      dp.from[dp.dirty_num] = r_from;
      dp.to[dp.dirty_num] = r_to;
      dp.dirty_num++;
    } else {
      std::cout << "BAD CASTLE BLOCK 1: " << move << " sf_to: " << sf_to
                << std::endl;
    }
  }

  global_pos.put_piece(placed, sf_to);

  // Add placed piece to dirty info
  dp.piece[dp.dirty_num] = placed;
  dp.from[dp.dirty_num] = SQ_NONE;
  dp.to[dp.dirty_num] = sf_to;
  dp.dirty_num++;

  // Update side to move
  global_pos.sideToMove = ~global_pos.sideToMove;

  // Finally, link state
  global_pos.st = new_si;
  // Rule50 update
  if (type_of(sf_piece) == PAWN || captured != NO_PIECE) {
    new_si->rule50 = 0;
  } else {
    new_si->rule50 = global_pos.st->previous->rule50 + 1;
  }
}

void do_null_move(StateInfo *new_si) {
  // 1. Link StateInfo
  new_si->previous = global_pos.state();

  // Reset accumulator computed flags
  new_si->accumulatorBig.computed[WHITE] = false;
  new_si->accumulatorBig.computed[BLACK] = false;
  new_si->accumulatorSmall.computed[WHITE] = false;
  new_si->accumulatorSmall.computed[BLACK] = false;

  // 2. Clear DirtyPiece (No pieces moved)
  new_si->dirtyPiece.dirty_num = 0;

  // 3. Update side to move
  global_pos.sideToMove = ~global_pos.sideToMove;

  // 4. Link state (update rule50 logic optional but good practice)
  global_pos.st = new_si;
  new_si->rule50 = global_pos.st->previous->rule50 + 1;
}

void undo_move(int /*move*/) {
  // Restore StateInfo
  StateInfo *old_si = global_pos.state();
  StateInfo *prev_si = old_si->previous;

  // Inverse board updates
  // We can use the DirtyPiece info from old_si!
  DirtyPiece &dp = old_si->dirtyPiece;

  for (int i = dp.dirty_num - 1; i >= 0; --i) {
    // Inverse: move from 'to' back to 'from'
    // If 'to' is SQ_NONE, it was a capture (removal). We perform
    // put_piece(from). If 'to' is valid, we move back: put(from), remove(to).

    // Wait, we must be careful with order (if swapping etc).
    // But standard chess moves don't overlap strangely (except castling).
    // Best to reverse iterate?
    // Castling: Move rook, Move King.
    // Undo: Move King back, Move Rook back.

    Piece pc = dp.piece[i];
    Square f = dp.from[i]; // Original From
    Square t = dp.to[i];   // Original To

    if (t == SQ_NONE) {
      // Was a capture/removal. Restore it.
      global_pos.put_piece(pc, f);
    } else {
      // Was a move/put.
      // Remove from 't', Put at 'f' if valid.
      global_pos.remove_piece(t);
      if (f != SQ_NONE) {
        global_pos.put_piece(pc, f);
      }
    }
  }

  // Restore side
  global_pos.sideToMove = ~global_pos.sideToMove;

  // Restore linkage
  global_pos.st = prev_si;
}

int evaluate() {
  // Call Stockfish Eval
  // Note: verify if evaluate expects side to move matching pos?
  // Yes, evaluate(pos) handles it.
  return Eval::evaluate(global_pos);
}
} // namespace Incremental
} // namespace Stockfish

#include <deque>

namespace Stockfish {
namespace Incremental {

thread_local std::deque<StateInfo> setup_states;

void setup_reset(const std::string &fen) {
  setup_states.clear();
  // Emplace root state
  setup_states.emplace_back();
  setup_states.back().previous = nullptr;
  // Initialize position with this root state
  global_pos.set(fen, &setup_states.back());
  stack_ptr = 0;
}

void setup_move(int move) {
  // Ensure we have a previous state (should be from reset or previous move)
  // Create new state
  setup_states.emplace_back();
  StateInfo *next = &setup_states.back();

  // Perform move
  do_move(move, next);
}

void setup_undo(int move) {
  // Undo move
  undo_move(move);
  // Remove the state we just invalidates
  setup_states.pop_back();
}

const Position& get_global_pos() {
    return global_pos;
}

const std::deque<StateInfo>& get_setup_states() {
    return setup_states;
}

void sync_from_main_thread(const Position& main_pos, const std::deque<StateInfo>& main_setup) {
    setup_states = main_setup;
    
    // Copy the Position by value using memcpy
    std::memcpy(&global_pos, &main_pos, sizeof(Position));
    
    // Fix pointers
    if (!setup_states.empty()) {
        global_pos.st = &setup_states.back();
        
        // Re-link previous pointers so they point to the thread-local copies,
        // rather than the original thread's TLS memory!
        for (size_t i = 1; i < setup_states.size(); ++i) {
            setup_states[i].previous = &setup_states[i - 1];
        }
        setup_states[0].previous = nullptr;
    } else {
        global_pos.st = nullptr;
    }

    if (global_pos.st == nullptr) {
        fprintf(stderr, "CRITICAL ERROR: global_pos.st is nullptr in sync_from_main_thread! main_setup.size()=%zu\n", main_setup.size());
    }

    // Also reset the search stack
    search_stack.resize(2048);
    stack_ptr = 0;
}

} // namespace Incremental
} // namespace Stockfish
