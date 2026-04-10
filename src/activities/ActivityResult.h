#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

struct WifiResult {
  bool connected = false;
  std::string ssid;
  std::string ip;
};

struct KeyboardResult {
  std::string text;
};

struct MenuResult {
  int action = -1;
  uint8_t orientation = 0;
  uint8_t pageTurnOption = 0;
};

struct ChapterResult {
  int spineIndex = 0;
  std::string anchor;  // Fragment anchor for within-chapter navigation (e.g., TOC entries sharing same spine item)
};

struct PercentResult {
  int percent = 0;
};

struct PageResult {
  uint32_t page = 0;
};

struct SyncResult {
  int spineIndex = 0;
  int page = 0;
};

enum class NetworkMode;

struct NetworkModeResult {
  NetworkMode mode;
};

struct FootnoteResult {
  std::string href;
};

struct StarredPageResult {
  enum Action : uint8_t { NAVIGATE, DELETE };
  int spineIndex = 0;
  int pageNumber = 0;
  Action action = NAVIGATE;
};

using ResultVariant = std::variant<std::monostate, WifiResult, KeyboardResult, MenuResult, ChapterResult, PercentResult,
                                   PageResult, SyncResult, NetworkModeResult, FootnoteResult, StarredPageResult>;

struct ActivityResult {
  bool isCancelled = false;
  ResultVariant data;

  explicit ActivityResult() = default;

  template <typename ResultType, typename = std::enable_if_t<std::is_constructible_v<ResultVariant, ResultType&&>>>
  // cppcheck-suppress noExplicitConstructor
  ActivityResult(ResultType&& result) : data{std::forward<ResultType>(result)} {}
};

using ActivityResultHandler = std::function<void(const ActivityResult&)>;
