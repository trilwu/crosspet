#include "TodoistProvider.h"

#include <ArduinoJson.h>
#include <Logging.h>
#include <esp_crt_bundle.h>
#include <esp_heap_caps.h>
#include <esp_http_client.h>

#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>

#include "TodoistFilter.h"
#include "tasks/TasksConfig.h"

namespace tasks {

namespace {

constexpr const char* kEndpointBase =
    "https://api.todoist.com/api/v1/tasks/filter?query=";
constexpr int kHttpTimeoutMs = 15000;
// Keep HTTP rx/tx buffers small. mbedTLS handshake on ESP32-C3 needs ~32 KB
// of heap on top of these — every KB we free here is one mbedTLS can take.
constexpr size_t kHttpBufSize = 2048;
constexpr size_t kMaxTasks = 64;
// Hard cap on the response body. Allocated once via malloc() and never
// reallocated: std::string's geometric growth (2x) creates transient memory
// spikes that trip OOM on the C3 when responses get large. A fixed buffer
// trades a higher steady-state footprint for predictability.
//
// 16 KB chosen empirically: even ThisMonth on a heavily-loaded account
// rarely pushes past 12 KB of JSON (64-task cap × ~180 bytes/task incl.
// envelope). 32 KB was originally picked for paranoia, but the C3's heap
// fragmentation after WiFi+NTP+TLS handshake makes a single contiguous
// 32 KB block unreliable — largest free block hovers around 24-28 KB.
// Truncation is handled gracefully (we parse what we got).
constexpr size_t kMaxResponseBytes = 16 * 1024;

struct ResponseBuffer {
  char* data = nullptr;
  size_t capacity = 0;
  size_t size = 0;
  bool truncated = false;
  bool allocFailed = false;
};

esp_err_t httpEventHandler(esp_http_client_event_t* evt) {
  if (evt->event_id != HTTP_EVENT_ON_DATA) return ESP_OK;
  auto* buf = static_cast<ResponseBuffer*>(evt->user_data);
  if (!buf || !evt->data || evt->data_len <= 0) return ESP_OK;
  // Lazy allocation: claim the response buffer only after the TLS handshake
  // has run and freed its scratch. Allocating up-front leaves mbedtls without
  // a contiguous block big enough for SSL setup on the C3 (~40KB), even
  // when total free heap looks healthy.
  if (!buf->data) {
    if (buf->allocFailed) return ESP_OK;
    buf->data = static_cast<char*>(malloc(buf->capacity));
    if (!buf->data) {
      buf->allocFailed = true;
      size_t freeBytes = heap_caps_get_free_size(MALLOC_CAP_8BIT);
      size_t largest = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
      LOG_ERR("TODOIST", "OOM allocating %u-byte response buffer (free=%u, largest=%u)",
              static_cast<unsigned>(buf->capacity),
              static_cast<unsigned>(freeBytes),
              static_cast<unsigned>(largest));
      return ESP_OK;
    }
  }
  size_t len = static_cast<size_t>(evt->data_len);
  size_t avail = buf->capacity - buf->size;
  if (len > avail) {
    buf->truncated = true;
    len = avail;
    if (len == 0) return ESP_OK;
  }
  memcpy(buf->data + buf->size, evt->data, len);
  buf->size += len;
  return ESP_OK;
}

void extractDueTime(const char* due, char* out, size_t outCap) {
  if (outCap == 0) return;
  out[0] = '\0';
  if (!due) return;
  const char* tPos = strchr(due, 'T');
  if (!tPos) return;
  if (strlen(tPos + 1) < 5) return;
  if (outCap < 6) return;
  snprintf(out, outCap, "%c%c:%c%c",
           tPos[1], tPos[2], tPos[4], tPos[5]);
}

bool isOverdue(const char* due) {
  if (!due) return false;
  int y, mo, d;
  if (sscanf(due, "%4d-%2d-%2d", &y, &mo, &d) != 3) return false;

  time_t now = time(nullptr);
  if (now < 1700000000) return false;  // clock not set yet
  struct tm nowTm;
  localtime_r(&now, &nowTm);

  if (y < nowTm.tm_year + 1900) return true;
  if (y > nowTm.tm_year + 1900) return false;
  if (mo < nowTm.tm_mon + 1) return true;
  if (mo > nowTm.tm_mon + 1) return false;
  return d < nowTm.tm_mday;
}

void copyTitle(char* dst, size_t dstCap, const char* src) {
  if (dstCap == 0) return;
  if (!src) {
    dst[0] = '\0';
    return;
  }
  size_t n = strlen(src);
  if (n >= dstCap) n = dstCap - 1;
  memcpy(dst, src, n);
  dst[n] = '\0';
}

FetchResult httpStatusToFetchResult(int code) {
  if (code == 200) return FetchResult::Ok;
  if (code == 401 || code == 403) return FetchResult::AuthError;
  if (code == 429) return FetchResult::RateLimited;
  return FetchResult::NetworkError;
}

}  // namespace

TodoistProvider& TodoistProvider::instance() {
  static TodoistProvider inst;
  return inst;
}

FetchResult TodoistProvider::fetch(const TasksFilter& filter,
                                   std::vector<Task>& out) {
  out.clear();
  out.reserve(kMaxTasks);

  const std::string& apiToken = TASKS_CONFIG.getTodoistApiToken();
  if (apiToken.empty()) {
    LOG_ERR("TODOIST", "Empty token");
    return FetchResult::NoAuth;
  }

  const std::string query = todoist::buildQuery(filter.date, filter.overdue);
  const std::string url = std::string(kEndpointBase) + todoist::urlEncode(query);
  LOG_DBG("TODOIST", "Query: %s", query.c_str());

  // Response buffer descriptor — actual char[] allocated lazily in
  // the event handler post-TLS to avoid heap fragmentation.
  ResponseBuffer buf;
  buf.capacity = kMaxResponseBytes;

  esp_http_client_config_t config = {};
  config.url = url.c_str();
  config.event_handler = httpEventHandler;
  config.user_data = &buf;
  config.method = HTTP_METHOD_GET;
  config.timeout_ms = kHttpTimeoutMs;
  config.buffer_size = kHttpBufSize;
  config.buffer_size_tx = kHttpBufSize;
  config.crt_bundle_attach = esp_crt_bundle_attach;

  esp_http_client_handle_t client = esp_http_client_init(&config);
  if (!client) {
    LOG_ERR("TODOIST", "esp_http_client_init failed");
    free(buf.data);
    return FetchResult::NetworkError;
  }

  std::string authHeader = "Bearer " + apiToken;
  if (esp_http_client_set_header(client, "Authorization", authHeader.c_str()) != ESP_OK ||
      esp_http_client_set_header(client, "Accept", "application/json") != ESP_OK) {
    LOG_ERR("TODOIST", "Set header failed");
    esp_http_client_cleanup(client);
    free(buf.data);
    return FetchResult::NetworkError;
  }

  esp_err_t err = esp_http_client_perform(client);
  const int httpCode = esp_http_client_get_status_code(client);
  esp_http_client_cleanup(client);

  LOG_DBG("TODOIST", "HTTP %d (err=%d, %u bytes%s)",
          httpCode, err, static_cast<unsigned>(buf.size),
          buf.truncated ? " [truncated]" : "");

  if (err != ESP_OK || buf.allocFailed) {
    free(buf.data);
    return FetchResult::NetworkError;
  }
  FetchResult statusResult = httpStatusToFetchResult(httpCode);
  if (statusResult != FetchResult::Ok) {
    free(buf.data);
    return statusResult;
  }

  if (buf.truncated) {
    LOG_ERR("TODOIST", "Response truncated at cap (%u bytes)",
            static_cast<unsigned>(buf.capacity));
    // Not fatal — try to parse what we have.
  }

  JsonDocument doc;
  auto parseErr = deserializeJson(doc, buf.data, buf.size);
  // Free raw bytes immediately — ArduinoJson copied what it needed.
  free(buf.data);
  buf.data = nullptr;
  if (parseErr) {
    LOG_ERR("TODOIST", "JSON parse: %s", parseErr.c_str());
    return FetchResult::ParseError;
  }
  if (!doc["results"].is<JsonArray>()) {
    LOG_ERR("TODOIST", "Response missing 'results' array");
    return FetchResult::ParseError;
  }
  if (!doc["next_cursor"].isNull()) {
    LOG_DBG("TODOIST", "Truncated: more tasks available via cursor");
  }

  JsonArray arr = doc["results"].as<JsonArray>();
  for (JsonObject task : arr) {
    if (out.size() >= kMaxTasks) break;
    Task t = {};
    copyTitle(t.title, Task::kTitleCapacity, task["content"] | "");
    t.priority = static_cast<uint8_t>(task["priority"] | 1);

    const char* date = nullptr;
    const char* datetime = nullptr;
    if (task["due"].is<JsonObject>()) {
      JsonObject due = task["due"].as<JsonObject>();
      date = due["date"] | static_cast<const char*>(nullptr);
      datetime = due["datetime"] | static_cast<const char*>(nullptr);
    }
    extractDueTime(datetime, t.dueTime, Task::kDueTimeCapacity);
    t.dueDate[0] = '\0';
    if (date && strlen(date) >= 10) {
      memcpy(t.dueDate, date, 10);
      t.dueDate[10] = '\0';
    }
    t.overdue = isOverdue(date);

    out.push_back(t);
  }

  LOG_DBG("TODOIST", "Parsed %u tasks", static_cast<unsigned>(out.size()));
  return FetchResult::Ok;
}

// Defined here (rather than in TasksConfig.cpp) so TasksConfig.cpp stays
// free of provider dependencies. The dispatch grows when a 2nd provider lands.
TaskProvider& TasksConfig::getActiveProvider() {
  return TodoistProvider::instance();
}

}  // namespace tasks
