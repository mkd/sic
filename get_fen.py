import chess

pgn = """[Event "?"]
[Site "?"]
[Date "?"]
[Round "?"]
[White "Sic"]
[Black "Gargantua"]
[Result "*"]

1. e4 c5 2. Nf3 Nc6 3. d4 cxd4 4. Nxd4 Nf6 5. Nc3 e5 6. Ndb5 d6 7. h4 a6 8. Na3 b5 9. Nd5 Nxe4 10. c4 Nd4 11. cxb5 Bb7 12. Bc4 Rc8 13. b3 Qa5+ 14. Kf1 axb5 15. Nxb5 Nxb5 16. Qf3 Nc5 17. b4 Qa8 18. bxc5 Rxc5 19. Qd3 Nd4 20. Nb6 Bxg2+ 21. Kg1 Qc6 22. Rh2 Be4 23. Qa3 Qxb6 24. Be3 Rxc4 25. Bxd4 Qxd4 26. Re1 Be7 27. Qa6 O-O 28. Rh3 Rc2 29. Rf1 Bf5 30. Rg3 Bxh4 31. Rg2 Bh3 32. Rh2 Bxf1 33. Qxf1 Qg4+ 34. Rg2 Qf4 35. Rh2 Rc1 36. Qxc1 Qxc1+ 37. Kg2 Qg5+ 38. Kf1 Rb8 39. Ke2 Rb2+ 40. Kd3 Qd2+ 41. Kc4 Qd4# *"""

board = chess.Board()
# Just apply some moves to get to move 45... wait, my PGN is completely wrong!
# The moves in the image are different.
