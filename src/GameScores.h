#pragma once
#include <cstdint>

// Persists high scores for games to /.crosspoint/game_scores.bin
// Loaded once at boot alongside ReadingStats. Each game saves on improvement.
class GameScores {
  static GameScores instance;

 public:
  uint32_t best2048 = 0;     // 2048 best score
  uint16_t mazeBest[3] = {}; // maze best moves per difficulty (0=Small,1=Med,2=Large)

  static GameScores& getInstance() { return instance; }

  bool loadFromFile();
  bool saveToFile() const;
};

#define GAME_SCORES GameScores::getInstance()
