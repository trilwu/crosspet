#include "ChessActivity.h"

#include <Arduino.h>
#include <GfxRenderer.h>
#include <I18n.h>

#include <cstdlib>
#include <cstring>

#include "ChessPieceSprites.h"
#include "components/UITheme.h"
#include "fontIds.h"

// Check if path is clear for sliding pieces (bishop, rook, queen)
bool ChessActivity::isValidSlidingMove(int fromR, int fromC, int toR, int toC, int dr, int dc) const {
  int r = fromR + dr, c = fromC + dc;
  while (r != toR || c != toC) {
    if (!inBounds(r, c)) return false;
    if (board[r][c] != EMPTY) return false;
    r += dr;
    c += dc;
  }
  return true;
}

bool ChessActivity::isValidMove(int fromR, int fromC, int toR, int toC) const {
  if (!inBounds(toR, toC)) return false;
  if (fromR == toR && fromC == toC) return false;

  int8_t piece = board[fromR][fromC];
  int8_t target = board[toR][toC];
  if (piece == EMPTY) return false;

  // Can't capture own piece
  bool pieceIsWhite = isWhite(piece);
  if (target != EMPTY && isWhite(target) == pieceIsWhite) return false;

  int dr = toR - fromR, dc = toC - fromC;
  int absDr = abs(dr), absDc = abs(dc);
  int8_t type = abs(piece);

  switch (type) {
    case PAWN: {
      int dir = pieceIsWhite ? -1 : 1;  // white moves up (row decreases)
      if (dc == 0 && target == EMPTY) {
        if (dr == dir) return true;
        // Initial two-square move
        int startRow = pieceIsWhite ? 6 : 1;
        if (fromR == startRow && dr == 2 * dir && board[fromR + dir][fromC] == EMPTY) return true;
      }
      // Diagonal capture
      if (dr == dir && absDc == 1 && target != EMPTY) return true;
      return false;
    }
    case KNIGHT:
      return (absDr == 2 && absDc == 1) || (absDr == 1 && absDc == 2);
    case BISHOP:
      if (absDr != absDc) return false;
      return isValidSlidingMove(fromR, fromC, toR, toC, dr > 0 ? 1 : -1, dc > 0 ? 1 : -1);
    case ROOK:
      if (dr != 0 && dc != 0) return false;
      return isValidSlidingMove(fromR, fromC, toR, toC, dr == 0 ? 0 : (dr > 0 ? 1 : -1),
                                dc == 0 ? 0 : (dc > 0 ? 1 : -1));
    case QUEEN:
      if (dr == 0 || dc == 0 || absDr == absDc) {
        return isValidSlidingMove(fromR, fromC, toR, toC, dr == 0 ? 0 : (dr > 0 ? 1 : -1),
                                  dc == 0 ? 0 : (dc > 0 ? 1 : -1));
      }
      return false;
    case KING:
      return absDr <= 1 && absDc <= 1;
  }
  return false;
}

bool ChessActivity::hasLegalMoves(bool forWhite) const {
  for (int r = 0; r < BOARD; r++)
    for (int c = 0; c < BOARD; c++) {
      int8_t p = board[r][c];
      if (p == EMPTY) continue;
      if (forWhite != isWhite(p)) continue;
      for (int tr = 0; tr < BOARD; tr++)
        for (int tc = 0; tc < BOARD; tc++)
          if (isValidMove(r, c, tr, tc)) return true;
    }
  return false;
}

// Returns material value for a piece (positive regardless of color)
int ChessActivity::pieceValue(int8_t piece) const {
  switch (abs(piece)) {
    case PAWN:   return 1;
    case KNIGHT: return 3;
    case BISHOP: return 3;
    case ROOK:   return 5;
    case QUEEN:  return 9;
    case KING:   return 100;
    default:     return 0;
  }
}

// Score a single move for medium AI (higher = better for black)
int ChessActivity::scoreMove(int fr, int fc, int tr, int tc) const {
  int score = 0;
  // Capture bonus
  if (board[tr][tc] != EMPTY) score += pieceValue(board[tr][tc]) * 10;
  // Center control bonus (rows/cols 3-4)
  if (tr >= 3 && tr <= 4 && tc >= 3 && tc <= 4) score += 5;
  // Check-giving bonus: if after move, white king is attacked
  // (simplified: reward moving to squares near white king)
  for (int r = 0; r < BOARD; r++)
    for (int c = 0; c < BOARD; c++)
      if (board[r][c] == KING && abs(tr - r) <= 1 && abs(tc - c) <= 1) score += 20;
  return score;
}

// Evaluate board material balance from black's perspective (positive = black winning)
int ChessActivity::evaluateBoard() const {
  int score = 0;
  for (int r = 0; r < BOARD; r++)
    for (int c = 0; c < BOARD; c++)
      if (board[r][c] != EMPTY)
        score += isBlack(board[r][c]) ? pieceValue(board[r][c]) : -pieceValue(board[r][c]);
  return score;
}

void ChessActivity::doAiMove() {
  struct Move { int8_t fr, fc, tr, tc; int score; };
  static constexpr int MAX_MOVES = 220;
  Move moves[MAX_MOVES];
  int count = 0;

  // Collect all legal moves for black
  for (int r = 0; r < BOARD && count < MAX_MOVES; r++)
    for (int c = 0; c < BOARD && count < MAX_MOVES; c++) {
      if (board[r][c] >= 0) continue;  // not black
      for (int tr = 0; tr < BOARD && count < MAX_MOVES; tr++)
        for (int tc = 0; tc < BOARD && count < MAX_MOVES; tc++) {
          if (isValidMove(r, c, tr, tc)) {
            int s = 0;
            if (difficulty == Difficulty::MEDIUM) s = scoreMove(r, c, tr, tc);
            else if (difficulty == Difficulty::HARD) {
              // 1-ply minimax: apply move, evaluate, undo
              int8_t saved = board[tr][tc];
              board[tr][tc] = board[r][c];
              board[r][c] = EMPTY;
              s = evaluateBoard();
              board[r][c] = board[tr][tc];
              board[tr][tc] = saved;
            }
            moves[count++] = {(int8_t)r, (int8_t)c, (int8_t)tr, (int8_t)tc, s};
          }
        }
    }

  if (count == 0) { gameOver = true; whiteWins = true; return; }

  randomSeed(millis());
  Move* chosen = nullptr;
  if (difficulty == Difficulty::EASY) {
    chosen = &moves[random(0, count)];
  } else if (difficulty == Difficulty::MEDIUM) {
    // Pick randomly among top 3 scored moves
    for (int i = 0; i < count - 1; i++)
      for (int j = i + 1; j < count; j++)
        if (moves[j].score > moves[i].score) { Move tmp = moves[i]; moves[i] = moves[j]; moves[j] = tmp; }
    int topN = count < 3 ? count : 3;
    chosen = &moves[random(0, topN)];
  } else {
    // Hard: pick best scoring move
    chosen = &moves[0];
    for (int i = 1; i < count; i++)
      if (moves[i].score > chosen->score) chosen = &moves[i];
  }

  auto& m = *chosen;
  if (abs(board[m.tr][m.tc]) == KING) { gameOver = true; whiteWins = false; }
  board[m.tr][m.tc] = board[m.fr][m.fc];
  board[m.fr][m.fc] = EMPTY;
  if (board[m.tr][m.tc] == -PAWN && m.tr == 7) board[m.tr][m.tc] = -QUEEN;
}

void ChessActivity::checkGameEnd() {
  // Check if either king is missing
  bool whiteKing = false, blackKing = false;
  for (int r = 0; r < BOARD; r++)
    for (int c = 0; c < BOARD; c++) {
      if (board[r][c] == KING) whiteKing = true;
      if (board[r][c] == -KING) blackKing = true;
    }
  if (!whiteKing) { gameOver = true; whiteWins = false; }
  if (!blackKing) { gameOver = true; whiteWins = true; }
}

const char* ChessActivity::difficultyLabel() const {
  switch (difficulty) {
    case Difficulty::EASY:   return tr(STR_CARO_EASY);
    case Difficulty::MEDIUM: return tr(STR_CARO_MEDIUM);
    case Difficulty::HARD:   return tr(STR_CARO_HARD);
  }
  return "";
}

void ChessActivity::initBoard() {
  // Standard starting position: row 0 = black (top), row 7 = white (bottom)
  memset(board, 0, sizeof(board));
  board[0][0] = board[0][7] = -ROOK;
  board[0][1] = board[0][6] = -KNIGHT;
  board[0][2] = board[0][5] = -BISHOP;
  board[0][3] = -QUEEN;
  board[0][4] = -KING;
  for (int c = 0; c < BOARD; c++) board[1][c] = -PAWN;
  board[7][0] = board[7][7] = ROOK;
  board[7][1] = board[7][6] = KNIGHT;
  board[7][2] = board[7][5] = BISHOP;
  board[7][3] = QUEEN;
  board[7][4] = KING;
  for (int c = 0; c < BOARD; c++) board[6][c] = PAWN;
  cursorRow = 6; cursorCol = 4;
  selRow = selCol = -1;
  whiteTurn = true; gameOver = false; whiteWins = false;
}

void ChessActivity::onEnter() {
  Activity::onEnter();
  showingDifficultySelect = true;
  requestUpdate();
}

void ChessActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (showingDifficultySelect) {
      finish();
    } else if (selRow >= 0) {
      selRow = selCol = -1;
      requestUpdate();
    } else if (gameOver) {
      showingDifficultySelect = true;
      requestUpdate();
    } else {
      finish();
    }
    return;
  }

  // Difficulty selection screen
  if (showingDifficultySelect) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Left)) {
      difficulty = (Difficulty)(((int)difficulty - 1 + DIFFICULTY_COUNT) % DIFFICULTY_COUNT);
      requestUpdate();
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Right)) {
      difficulty = (Difficulty)(((int)difficulty + 1) % DIFFICULTY_COUNT);
      requestUpdate();
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      showingDifficultySelect = false;
      initBoard();
      requestUpdate();
    }
    return;
  }

  // Game over: Confirm = back to difficulty select
  if (gameOver) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      showingDifficultySelect = true;
      requestUpdate();
    }
    return;
  }

  // Only allow input on white's turn (AI plays black)
  if (!whiteTurn) return;

  bool changed = false;

  // D-pad cursor
  if (mappedInput.wasReleased(MappedInputManager::Button::Left)) {
    cursorCol = (cursorCol - 1 + BOARD) % BOARD;
    changed = true;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Right)) {
    cursorCol = (cursorCol + 1) % BOARD;
    changed = true;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Up)) {
    cursorRow = (cursorRow - 1 + BOARD) % BOARD;
    changed = true;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Down)) {
    cursorRow = (cursorRow + 1) % BOARD;
    changed = true;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (selRow < 0) {
      // No piece selected: select own piece
      if (isWhite(board[cursorRow][cursorCol])) {
        selRow = cursorRow;
        selCol = cursorCol;
        changed = true;
      }
    } else {
      // Piece selected: try to move
      if (cursorRow == selRow && cursorCol == selCol) {
        // Deselect
        selRow = selCol = -1;
        changed = true;
      } else if (isWhite(board[cursorRow][cursorCol])) {
        // Select different own piece
        selRow = cursorRow;
        selCol = cursorCol;
        changed = true;
      } else if (isValidMove(selRow, selCol, cursorRow, cursorCol)) {
        // Execute move
        if (abs(board[cursorRow][cursorCol]) == KING) {
          gameOver = true;
          whiteWins = true;
        }
        board[cursorRow][cursorCol] = board[selRow][selCol];
        board[selRow][selCol] = EMPTY;
        // Auto-promote white pawn to queen
        if (board[cursorRow][cursorCol] == PAWN && cursorRow == 0)
          board[cursorRow][cursorCol] = QUEEN;
        selRow = selCol = -1;
        whiteTurn = false;
        changed = true;

        // Render white's move, then AI plays
        if (!gameOver) {
          requestUpdate();
          doAiMove();
          checkGameEnd();
          whiteTurn = true;
        }
      }
    }
  }

  if (changed) requestUpdate();
}

void ChessActivity::renderDifficultySelect() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_CHESS));

  const int centerY = (metrics.topPadding + metrics.headerHeight + pageHeight - metrics.buttonHintsHeight) / 2;
  renderer.drawCenteredText(UI_12_FONT_ID, centerY - 40, tr(STR_SELECT_DIFFICULTY));

  char label[32];
  snprintf(label, sizeof(label), "< %s >", difficultyLabel());
  renderer.drawCenteredText(UI_10_FONT_ID, centerY, label, true, EpdFontFamily::BOLD);

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}

void ChessActivity::render(RenderLock&&) {
  if (showingDifficultySelect) {
    renderDifficultySelect();
    return;
  }

  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_CHESS));

  const int boardSize = CELL * BOARD;
  const int boardX = (pageWidth - boardSize) / 2;
  const int contentTop = metrics.topPadding + metrics.headerHeight + 4;
  const int contentBot = pageHeight - metrics.buttonHintsHeight - 4;
  const int boardY = contentTop + (contentBot - contentTop - boardSize - 24) / 2;

  // Draw board squares and pieces
  for (int r = 0; r < BOARD; r++) {
    for (int c = 0; c < BOARD; c++) {
      int cx = boardX + c * CELL;
      int cy = boardY + r * CELL;
      bool darkSquare = (r + c) % 2 == 1;
      bool isCursor = (r == cursorRow && c == cursorCol);
      bool isSelected = (r == selRow && c == selCol);

      // Dark squares: fill with horizontal lines pattern (dithering)
      if (darkSquare) {
        for (int y = cy; y < cy + CELL; y += 3) {
          renderer.drawLine(cx, y, cx + CELL - 1, y);
        }
      }

      // Selected piece: solid fill
      if (isSelected) {
        renderer.fillRect(cx + 1, cy + 1, CELL - 2, CELL - 2);
      }

      // Cursor: thick border
      if (isCursor) {
        renderer.drawRect(cx, cy, CELL, CELL);
        renderer.drawRect(cx + 1, cy + 1, CELL - 2, CELL - 2);
        renderer.drawRect(cx + 2, cy + 2, CELL - 4, CELL - 4);
      }

      // Show valid move dots when piece is selected
      if (selRow >= 0 && !isSelected && isValidMove(selRow, selCol, r, c)) {
        int dotSize = 6;
        int dx = cx + (CELL - dotSize) / 2;
        int dy = cy + (CELL - dotSize) / 2;
        renderer.fillRect(dx, dy, dotSize, dotSize);
      }

      // Draw piece sprite
      int8_t piece = board[r][c];
      if (piece != EMPTY) {
        const uint8_t* sprite = getChessPieceSprite(piece);
        if (sprite) {
          if (isSelected) {
            // Opaque draw on solid-filled selected square (piece visible on black bg)
            renderer.drawImage(sprite, cx, cy, CELL, CELL);
          } else {
            // Transparent draw preserves board background (dithered dark squares)
            renderer.drawImageTransparent(sprite, cx, cy, CELL, CELL);
          }
        }
      }
    }
  }

  // Board border
  renderer.drawRect(boardX - 1, boardY - 1, boardSize + 2, boardSize + 2);

  // Status line below board
  int statusY = boardY + boardSize + 4;
  if (gameOver) {
    const char* msg = whiteWins ? tr(STR_CHESS_WHITE_WINS) : tr(STR_CHESS_BLACK_WINS);
    renderer.drawCenteredText(SMALL_FONT_ID, statusY, msg, true, EpdFontFamily::BOLD);
  } else {
    const char* turn = whiteTurn ? tr(STR_CHESS_WHITE_TURN) : tr(STR_CHESS_BLACK_TURN);
    renderer.drawCenteredText(SMALL_FONT_ID, statusY, turn);
  }

  // Button hints
  const char* btn1 = (selRow >= 0) ? tr(STR_CANCEL) : tr(STR_BACK);
  const char* btn2 = "";
  if (gameOver) {
    btn2 = tr(STR_NEW_GAME);
  } else if (selRow >= 0) {
    btn2 = tr(STR_CHESS_MOVE);
  } else {
    btn2 = tr(STR_CHESS_SELECT);
  }

  const auto labels = mappedInput.mapLabels(btn1, btn2, "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
