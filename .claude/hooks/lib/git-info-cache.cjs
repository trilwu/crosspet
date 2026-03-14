#!/usr/bin/env node
'use strict';

/**
 * Git Info Cache - Cross-platform git information batching
 *
 * Problem: 5-6 git process spawns per statusline render are slow on Windows (CreateProcess overhead)
 * Solution: Cache git query results for 3 seconds — subsequent renders read cache (zero processes)
 *
 * Performance: 5 spawns per render → event-driven refresh + 30s TTL fallback
 * Cross-platform: No bash-only syntax (no 2>/dev/null), windowsHide on all exec calls
 */

const { execSync } = require('child_process');
const fs = require('fs');
const path = require('path');
const os = require('os');

// Cache TTL — long fallback for external changes (git checkout outside Claude)
// Active invalidation happens via PostToolUse hooks after Edit/Write/Bash
const CACHE_TTL = 30000;
const CACHE_MISS = Symbol('cache_miss');

/**
 * Safe command execution wrapper with optional cwd
 */
function execIn(cmd, cwd) {
  try {
    return execSync(cmd, {
      encoding: 'utf8',
      stdio: ['pipe', 'pipe', 'ignore'],
      windowsHide: true,
      cwd: cwd || undefined
    }).trim();
  } catch {
    return '';
  }
}

/**
 * Get cache file path for current working directory
 */
function getCachePath(cwd) {
  const hash = require('crypto')
    .createHash('md5')
    .update(cwd)
    .digest('hex')
    .slice(0, 8);
  return path.join(os.tmpdir(), `ck-git-cache-${hash}.json`);
}

/**
 * Read cache if valid (not expired). Returns CACHE_MISS on miss.
 * No existsSync check (TOCTOU race) — just try read and catch.
 */
function readCache(cachePath) {
  try {
    const cache = JSON.parse(fs.readFileSync(cachePath, 'utf8'));
    if (Date.now() - cache.timestamp < CACHE_TTL) {
      return cache.data; // Can be null (non-git dir) or object (git info)
    }
  } catch {
    // File missing, corrupted, or expired — all treated as cache miss
  }
  return CACHE_MISS;
}

/**
 * Write cache atomically (temp file + rename to avoid partial reads on Windows)
 */
function writeCache(cachePath, data) {
  const tmpPath = cachePath + '.tmp';
  try {
    fs.writeFileSync(tmpPath, JSON.stringify({ timestamp: Date.now(), data }));
    fs.renameSync(tmpPath, cachePath);
  } catch {
    try { fs.unlinkSync(tmpPath); } catch {}
  }
}

/**
 * Count non-empty lines in a newline-delimited string
 */
function countLines(str) {
  if (!str) return 0;
  return str.split('\n').filter(l => l.trim()).length;
}

/**
 * Fetch git info directly in-process
 * The cache is what eliminates redundant spawns — not subprocess wrapping
 * @param {string} cwd - Directory to run git commands in
 * Returns: { branch, unstaged, staged, ahead, behind } or null if not git repo
 */
function fetchGitInfo(cwd) {
  // Check if git repo (fast check) — run in target cwd, not process.cwd()
  if (!execIn('git rev-parse --git-dir', cwd)) {
    return null;
  }

  const branch = execIn('git branch --show-current', cwd) || execIn('git rev-parse --short HEAD', cwd);
  const unstaged = countLines(execIn('git diff --name-only', cwd));
  const staged = countLines(execIn('git diff --cached --name-only', cwd));

  // Ahead/behind — no 2>/dev/null (invalid on Windows cmd.exe)
  let ahead = 0, behind = 0;
  const aheadBehind = execIn('git rev-list --left-right --count @{u}...HEAD', cwd);
  if (aheadBehind) {
    const parts = aheadBehind.split(/\s+/);
    behind = parseInt(parts[0], 10) || 0;
    ahead = parseInt(parts[1], 10) || 0;
  }

  return { branch, unstaged, staged, ahead, behind };
}

/**
 * Get git info with caching
 * Main export function used by statusline
 */
function getGitInfo(cwd = process.cwd()) {
  const cachePath = getCachePath(cwd);

  // Try cache first (includes cached null for non-git dirs)
  const cached = readCache(cachePath);
  if (cached !== CACHE_MISS) return cached;

  // Cache miss or expired, fetch fresh data in target cwd
  const data = fetchGitInfo(cwd);
  // Cache both positive and null results (avoids re-spawning git in non-git dirs)
  writeCache(cachePath, data);

  return data;
}

/**
 * Invalidate cache for a directory (call after file changes to trigger fresh git query)
 */
function invalidateCache(cwd = process.cwd()) {
  try { fs.unlinkSync(getCachePath(cwd)); } catch {}
}

module.exports = { getGitInfo, invalidateCache };
