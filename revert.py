with open("src/stockfish_probe/evaluate.cpp", "r") as f:
    content = f.read()

import re
content = re.sub(r'Value Eval::evaluate\(const Position &pos, bool force_small\) \{.*?\n\}', """
Value Eval::evaluate(const Position &pos, bool force_small) {
  if (pos.state() == nullptr) abort();

  int simpleEval = simple_eval(pos, pos.side_to_move());
  bool smallNet = force_small || (std::abs(simpleEval) > 1050);

  int nnueComplexity;

  Value nnue = smallNet
                   ? NNUE::evaluate<NNUE::Small>(pos, true, &nnueComplexity)
                   : NNUE::evaluate<NNUE::Big>(pos, true, &nnueComplexity);

  nnue -= nnue * (nnueComplexity + std::abs(simpleEval - nnue)) / 32768;

  int npm = pos.non_pawn_material() / 64;
  int v = (nnue * (915 + npm + 9 * pos.count<PAWN>())) / 1024;

  int shuffling = pos.rule50_count();
  v = v * (200 - shuffling) / 214;

  v = std::clamp(v, VALUE_TB_LOSS_IN_MAX_PLY + 1, VALUE_TB_WIN_IN_MAX_PLY - 1);
  v += mop_up(pos, pos.side_to_move());

  return v;
}
""", content, flags=re.DOTALL)

with open("src/stockfish_probe/evaluate.cpp", "w") as f:
    f.write(content)
