import chess
import chess.pgn
import sys

with open('test_game.pgn') as f:
    game = chess.pgn.read_game(f)
board = game.board()
moves = list(game.mainline_moves())
for i, move in enumerate(moves):
    board.push(move)
    print(f"{i+1} {move.uci()} Zobrist: {hex(board.zobrist_hash())} FEN: {board.fen()}")
