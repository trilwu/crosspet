#include "FileBrowserActivity.h"

#include <Epub.h>
#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>

#include <algorithm>

#include "../util/ConfirmationActivity.h"
#include "CrossPointSettings.h"
#include "CrossPointState.h"
#include "FavoritesStore.h"
#include "TabNavigation.h"
#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr unsigned long GO_HOME_MS = 1000;
constexpr unsigned long FAVORITE_MS = 700;

// Natural sort comparator for two strings (case-insensitive, numeric-aware)
bool naturalLess(const std::string& str1, const std::string& str2) {
  const char* s1 = str1.c_str();
  const char* s2 = str2.c_str();

  while (*s1 && *s2) {
    if (isdigit(*s1) && isdigit(*s2)) {
      while (*s1 == '0') s1++;
      while (*s2 == '0') s2++;
      int len1 = 0, len2 = 0;
      while (isdigit(s1[len1])) len1++;
      while (isdigit(s2[len2])) len2++;
      if (len1 != len2) return len1 < len2;
      for (int i = 0; i < len1; i++) {
        if (s1[i] != s2[i]) return s1[i] < s2[i];
      }
      s1 += len1;
      s2 += len2;
    } else {
      char c1 = tolower(*s1);
      char c2 = tolower(*s2);
      if (c1 != c2) return c1 < c2;
      s1++;
      s2++;
    }
  }
  return *s1 == '\0' && *s2 != '\0';
}
// Sort file list by mode with favorites first, directories always on top
void sortFileList(std::vector<FileEntry>& files) {
  uint8_t sortMode = SETTINGS.librarySortMode;

  std::sort(files.begin(), files.end(), [sortMode](const FileEntry& a, const FileEntry& b) {
    // Directories always first
    bool isDir1 = !a.name.empty() && a.name.back() == '/';
    bool isDir2 = !b.name.empty() && b.name.back() == '/';
    if (isDir1 != isDir2) return isDir1;

    // Favorites second (uses pre-computed flag — no heap allocation)
    if (!isDir1 && a.favorite != b.favorite) return a.favorite;

    // Sort by selected mode
    switch (sortMode) {
      case CrossPointSettings::SORT_ALPHA: {
        // Case-insensitive compare without allocating
        const char* s1 = a.name.c_str();
        const char* s2 = b.name.c_str();
        while (*s1 && *s2) {
          char c1 = tolower(*s1++);
          char c2 = tolower(*s2++);
          if (c1 != c2) return c1 < c2;
        }
        return *s1 == '\0' && *s2 != '\0';
      }
      case CrossPointSettings::SORT_DATE:
        // Newest first, fallback to name (modTime=0 if HalFile lacks API)
        if (a.modTime != b.modTime) return a.modTime > b.modTime;
        return naturalLess(a.name, b.name);
      case CrossPointSettings::SORT_SIZE:
        // Largest first, fallback to name
        if (a.fileSize != b.fileSize) return a.fileSize > b.fileSize;
        return naturalLess(a.name, b.name);
      default:
        return naturalLess(a.name, b.name);
    }
  });
}
}  // namespace

void FileBrowserActivity::loadFiles() {
  files.clear();

  auto root = Storage.open(basepath.c_str());
  if (!root || !root.isDirectory()) {
    if (root) root.close();
    return;
  }

  root.rewindDirectory();

  char name[500];
  for (auto file = root.openNextFile(); file; file = root.openNextFile()) {
    file.getName(name, sizeof(name));
    if (name[0] == '.' || strcmp(name, "System Volume Information") == 0) {
      file.close();
      continue;
    }

    if (file.isDirectory()) {
      FileEntry entry;
      entry.name = std::string(name) + "/";
      files.push_back(std::move(entry));
    } else {
      std::string_view filename{name};
      if (FsHelpers::hasEpubExtension(filename) || FsHelpers::hasXtcExtension(filename) ||
          FsHelpers::hasTxtExtension(filename) || FsHelpers::hasMarkdownExtension(filename) ||
          FsHelpers::hasBmpExtension(filename)) {
        FileEntry entry;
        entry.name = std::string(name);
        entry.fileSize = static_cast<uint32_t>(file.fileSize());
        entry.favorite = FAVORITES.isFavorite(buildFullPath(entry.name));
        files.push_back(std::move(entry));
      }
    }
    file.close();
  }
  root.close();
  sortFileList(files);
}

std::string FileBrowserActivity::buildFullPath(const std::string& filename) const {
  if (basepath.back() == '/') return basepath + filename;
  return basepath + "/" + filename;
}

void FileBrowserActivity::onEnter() {
  Activity::onEnter();
  loadFiles();
  selectorIndex = 0;
  requestUpdate();
}

void FileBrowserActivity::onExit() {
  Activity::onExit();
  files.clear();
}

void FileBrowserActivity::clearFileMetadata(const std::string& fullPath) {
  if (FsHelpers::hasEpubExtension(fullPath)) {
    Epub(fullPath, "/.crosspoint").clearCache();
    LOG_DBG("FileBrowser", "Cleared metadata cache for: %s", fullPath.c_str());
  }
}

void FileBrowserActivity::loop() {
  // Tab switching via L/R (only at root directory level)
  if (basepath == "/") {
    int nextTab = handleTabInput(mappedInput, static_cast<Tab>(APP_STATE.currentTab));
    if (nextTab >= 0) {
      APP_STATE.currentTab = static_cast<uint8_t>(nextTab);
      APP_STATE.saveToFile();
      activityManager.replaceActivity(createTabActivity(static_cast<Tab>(nextTab), renderer, mappedInput));
      return;
    }
  }

  // Long press BACK (1s+) goes to root folder
  if (mappedInput.isPressed(MappedInputManager::Button::Back) && mappedInput.getHeldTime() >= GO_HOME_MS &&
      basepath != "/") {
    basepath = "/";
    loadFiles();
    selectorIndex = 0;
    return;
  }

  const int pageItems = UITheme::getInstance().getNumberOfItemsPerPage(renderer, true, false, true, false);

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (files.empty()) return;

    const std::string& entry = files[selectorIndex].name;
    bool isDirectory = (!entry.empty() && entry.back() == '/');
    unsigned long heldTime = mappedInput.getHeldTime();

    if (heldTime >= GO_HOME_MS && !isDirectory) {
      // --- LONG PRESS (1s+): DELETE FILE ---
      const std::string fullPath = buildFullPath(entry);

      auto handler = [this, fullPath](const ActivityResult& res) {
        if (!res.isCancelled) {
          LOG_DBG("FileBrowser", "Attempting to delete: %s", fullPath.c_str());
          clearFileMetadata(fullPath);
          if (Storage.remove(fullPath.c_str())) {
            LOG_DBG("FileBrowser", "Deleted successfully");
            loadFiles();
            if (files.empty()) {
              selectorIndex = 0;
            } else if (selectorIndex >= files.size()) {
              selectorIndex = files.size() - 1;
            }
            requestUpdate(true);
          } else {
            LOG_ERR("FileBrowser", "Failed to delete file: %s", fullPath.c_str());
          }
        }
      };

      std::string heading = tr(STR_DELETE) + std::string("? ");
      startActivityForResult(std::make_unique<ConfirmationActivity>(renderer, mappedInput, heading, entry), handler);
      return;
    } else if (heldTime >= FAVORITE_MS && !isDirectory) {
      // --- MEDIUM PRESS (700ms-999ms): TOGGLE FAVORITE ---
      const std::string fullPath = buildFullPath(entry);
      FAVORITES.toggleFavorite(fullPath);
      // Update pre-computed flag for all entries
      for (auto& f : files) {
        if (!f.name.empty() && f.name.back() != '/') {
          f.favorite = FAVORITES.isFavorite(buildFullPath(f.name));
        }
      }
      sortFileList(files);
      // Re-find the entry after re-sort
      for (size_t i = 0; i < files.size(); i++) {
        if (files[i].name == entry) {
          selectorIndex = i;
          break;
        }
      }
      requestUpdate(true);
      return;
    } else {
      // --- SHORT PRESS: OPEN/NAVIGATE ---
      if (basepath.back() != '/') basepath += "/";

      if (isDirectory) {
        basepath += entry.substr(0, entry.length() - 1);
        loadFiles();
        selectorIndex = 0;
        requestUpdate();
      } else {
        onSelectBook(basepath + entry);
      }
    }
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (mappedInput.getHeldTime() < GO_HOME_MS) {
      if (basepath != "/") {
        const std::string oldPath = basepath;
        basepath.replace(basepath.find_last_of('/'), std::string::npos, "");
        if (basepath.empty()) basepath = "/";
        loadFiles();

        const auto pos = oldPath.find_last_of('/');
        const std::string dirName = oldPath.substr(pos + 1) + "/";
        selectorIndex = findEntry(dirName);
        requestUpdate();
      } else {
        onGoHome();
      }
    }
  }

  int listSize = static_cast<int>(files.size());
  buttonNavigator.onNextRelease([this, listSize] {
    selectorIndex = ButtonNavigator::nextIndex(static_cast<int>(selectorIndex), listSize);
    requestUpdate();
  });

  buttonNavigator.onPreviousRelease([this, listSize] {
    selectorIndex = ButtonNavigator::previousIndex(static_cast<int>(selectorIndex), listSize);
    requestUpdate();
  });

  buttonNavigator.onNextContinuous([this, listSize, pageItems] {
    selectorIndex = ButtonNavigator::nextPageIndex(static_cast<int>(selectorIndex), listSize, pageItems);
    requestUpdate();
  });

  buttonNavigator.onPreviousContinuous([this, listSize, pageItems] {
    selectorIndex = ButtonNavigator::previousPageIndex(static_cast<int>(selectorIndex), listSize, pageItems);
    requestUpdate();
  });
}

std::string getFileName(const std::string& filename) {
  if (filename.back() == '/') {
    return filename.substr(0, filename.length() - 1);
  }
  const auto pos = filename.rfind('.');
  return filename.substr(0, pos);
}

void FileBrowserActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const auto& metrics = UITheme::getInstance().getMetrics();

  std::string folderName = (basepath == "/") ? tr(STR_SD_CARD) : basepath.substr(basepath.rfind('/') + 1);
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, folderName.c_str());

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing;
  if (files.empty()) {
    renderer.drawText(UI_10_FONT_ID, metrics.contentSidePadding, contentTop + 20, tr(STR_NO_FILES_FOUND));
  } else {
    GUI.drawList(
        renderer, Rect{0, contentTop, pageWidth, contentHeight}, files.size(), selectorIndex,
        [this](int index) {
          const FileEntry& entry = files[index];
          std::string display = getFileName(entry.name);
          // Prepend star for favorites (uses pre-computed flag)
          if (entry.favorite) {
            display = "* " + display;
          }
          return display;
        },
        nullptr, [this](int index) { return UITheme::getFileIcon(files[index].name); });
  }

  // Tab bar at bottom (Kindle-style) — only show at root level
  if (basepath == "/") {
    auto tabs = getGlobalTabs(static_cast<Tab>(APP_STATE.currentTab));
    int tabBarY = pageHeight - metrics.tabBarHeight;
    GUI.drawTabBar(renderer, Rect{0, tabBarY, pageWidth, metrics.tabBarHeight}, tabs, true);
  } else {
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), files.empty() ? "" : tr(STR_OPEN),
                                               files.empty() ? "" : tr(STR_DIR_UP), files.empty() ? "" : tr(STR_DIR_DOWN));
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  }

  renderer.displayBuffer();
}

size_t FileBrowserActivity::findEntry(const std::string& name) const {
  for (size_t i = 0; i < files.size(); i++)
    if (files[i].name == name) return i;
  return 0;
}
