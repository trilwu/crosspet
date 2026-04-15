#pragma once
#include <Print.h>
#include <expat.h>

#include <string>
#include <vector>

/**
 * Type of OPDS entry.
 */
enum class OpdsEntryType {
  NAVIGATION,  // Link to another catalog
  BOOK         // Downloadable book
};

/**
 * Represents an entry from an OPDS feed (either a navigation link or a book).
 */
struct OpdsEntry {
  OpdsEntryType type = OpdsEntryType::NAVIGATION;
  std::string title;
  std::string author;  // Only for books
  std::string href;    // Navigation URL or epub download URL
  std::string id;
};

// Legacy alias for backward compatibility
using OpdsBook = OpdsEntry;

/**
 * Parser for OPDS (Open Publication Distribution System) Atom feeds.
 * Uses the Expat XML parser to parse OPDS catalog entries.
 *
 * Usage:
 *   OpdsParser parser;
 *   parser.setBaseUrl("http://example.com/opds");
 *   if (parser.parse(xmlData, xmlLength)) {
 *     for (const auto& entry : parser.getEntries()) {
 *       if (entry.type == OpdsEntryType::BOOK) {
 *         // Downloadable book
 *       } else {
 *         // Navigation link to another catalog
 *       }
 *     }
 *     // Optional: search and pagination URLs
 *     auto searchUrl = parser.getSearchUrl();
 *     auto nextUrl = parser.getNextPageUrl();
 *   }
 */
class OpdsParser final : public Print {
 public:
  OpdsParser();
  ~OpdsParser();

  // Disable copy
  OpdsParser(const OpdsParser&) = delete;
  OpdsParser& operator=(const OpdsParser&) = delete;

  size_t write(uint8_t) override;
  size_t write(const uint8_t*, size_t) override;

  void flush() override;

  bool error() const;

  operator bool() { return !error(); }

  /**
   * Set the base URL of the feed being parsed (used to resolve relative hrefs).
   */
  void setBaseUrl(const std::string& url) { baseUrl_ = url; }

  /**
   * Get the parsed entries (both navigation and book entries).
   * @return Vector of OpdsEntry entries
   */
  const std::vector<OpdsEntry>& getEntries() const& { return entries; }
  std::vector<OpdsEntry> getEntries() && { return std::move(entries); }

  /**
   * Get only book entries (legacy compatibility).
   * @return Vector of book entries
   */
  std::vector<OpdsEntry> getBooks() const;

  /**
   * Get OpenSearch description URL (empty string if not present).
   */
  const std::string& getSearchUrl() const { return searchUrl_; }

  /**
   * Get the "next page" URL from the feed (empty string if not present).
   */
  const std::string& getNextPageUrl() const { return nextPageUrl_; }

  /**
   * Get the "previous page" URL from the feed (empty string if not present).
   */
  const std::string& getPrevPageUrl() const { return prevPageUrl_; }

  /**
   * Clear all parsed entries and feed-level metadata.
   */
  void clear();

 private:
  // Expat callbacks
  static void XMLCALL startElement(void* userData, const XML_Char* name, const XML_Char** atts);
  static void XMLCALL endElement(void* userData, const XML_Char* name);
  static void XMLCALL characterData(void* userData, const XML_Char* s, int len);

  // Helper to find attribute value
  static const char* findAttribute(const XML_Char** atts, const char* name);

  // Resolve href against baseUrl_ if it is relative
  std::string resolveHref(const char* href) const;

  XML_Parser parser = nullptr;
  std::vector<OpdsEntry> entries;
  OpdsEntry currentEntry;
  std::string currentText;
  std::string baseUrl_;

  // Feed-level navigation URLs
  std::string searchUrl_;
  std::string nextPageUrl_;
  std::string prevPageUrl_;

  // Parser state
  bool inEntry = false;
  bool inTitle = false;
  bool inAuthor = false;
  bool inAuthorName = false;
  bool inId = false;

  bool errorOccured = false;
};
