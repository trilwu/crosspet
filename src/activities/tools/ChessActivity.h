#pragma once

#include <cstdint>

#include "../Activity.h"

// Simple chess game vs AI. D-pad moves cursor. Confirm = select/move piece. Back = cancel/exit.
// Pieces: positive=white, negative=black. Abs values: 1=Pawn,2=Knight,3=Bishop,4=Rook,5=Queen,6=King
// AI difficulty: Easy=random, Medium=capture+center scoring (top-3 random), Hard=1-ply minimax
class ChessActivity final : public Activity {
  static constexpr int BOARD = 8;
  static constexpr int CELL = 48;

  // Piece type constants
  static constexpr int8_t EMPTY = 0;
  static constexpr int8_t PAWN = 1;
  static constexpr int8_t KNIGHT = 2;
  static constexpr int8_t BISHOP = 3;
  static constexpr int8_t ROOK = 4;
  static constexpr int8_t QUEEN = 5;
  static constexpr int8_t KING = 6;

  enum class Difficulty : uint8_t { EASY = 0, MEDIUM, HARD };
  static constexpr int DIFFICULTY_COUNT = 3;
  Difficulty difficulty = Difficulty::EASY;
  bool showingDifficultySelect = true;  // pre-game difficulty selection screen

  int8_t board[BOARD][BOARD];  // [row][col], row 0 = top (black side)
  int cursorRow = 0;
  int cursorCol = 0;
  int selRow = -1, selCol = -1;  // selected piece (-1 = none)
  bool whiteTurn = true;
  bool gameOver = false;
  bool whiteWins = false;

  // Move validation
  bool isWhite(int8_t piece) const { return piece > 0; }
  bool isBlack(int8_t piece) const { return piece < 0; }
  bool isOwnPiece(int8_t piece) const { return whiteTurn ? isWhite(piece) : isBlack(piece); }
  bool isEnemyPiece(int8_t piece) const { return whiteTurn ? isBlack(piece) : isWhite(piece); }
  bool inBounds(int r, int c) const { return r >= 0 && r < BOARD && c >= 0 && c < BOARD; }

  bool isValidMove(int fromR, int fromC, int toR, int toC) const;
  bool isValidSlidingMove(int fromR, int fromC, int toR, int toC, int dr, int dc) const;
  bool hasLegalMoves(bool forWhite) const;
  int pieceValue(int8_t piece) const;
  int scoreMove(int fr, int fc, int tr, int tc) const;
  int evaluateBoard() const;
  void doAiMove();
  void checkGameEnd();
  const char* difficultyLabel() const;
  void initBoard();
  void renderDifficultySelect();

 public:
  explicit ChessActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Chess", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
