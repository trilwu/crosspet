/**
 * hook-logger.cjs - Zero-dependency structured logger for hooks
 *
 * Logs to .claude/hooks/.logs/hook-log.jsonl (JSON Lines format)
 * Auto-creates .logs/ directory and handles rotation (1000 lines max → 500 last)
 * Uses only Node builtins (fs, path) — no external dependencies
 *
 * Export: logHook(hookName, data), createHookTimer(hookName)
 */

const fs = require('fs');
const path = require('path');

const LOG_DIR = path.join(__dirname, '..', '.logs');
const LOG_FILE = path.join(LOG_DIR, 'hook-log.jsonl');
const MAX_LINES = 1000;
const TRUNCATE_TO = 500;

/**
 * Ensure log directory exists
 */
function ensureLogDir() {
  try {
    if (!fs.existsSync(LOG_DIR)) {
      fs.mkdirSync(LOG_DIR, { recursive: true });
    }
  } catch (_) {
    // Fail silently — never crash
  }
}

/**
 * Rotate log file if it exceeds MAX_LINES
 */
function rotateIfNeeded() {
  try {
    if (!fs.existsSync(LOG_FILE)) return;
    const lines = fs.readFileSync(LOG_FILE, 'utf-8').split('\n').filter(Boolean);
    if (lines.length >= MAX_LINES) {
      const truncated = lines.slice(-TRUNCATE_TO).join('\n') + '\n';
      fs.writeFileSync(LOG_FILE, truncated, 'utf-8');
    }
  } catch (_) {
    // Fail silently
  }
}

/**
 * Log a hook event
 * @param {string} hookName - Hook filename (e.g., 'scout-block')
 * @param {object} data - Log data { tool?, dur?, status, exit?, error? }
 */
function logHook(hookName, data) {
  try {
    ensureLogDir();
    rotateIfNeeded();

    const entry = {
      ts: new Date().toISOString(),
      hook: hookName,
      tool: data.tool || '',
      dur: data.dur || 0,
      status: data.status || 'ok',
      exit: data.exit !== undefined ? data.exit : 0,
      error: data.error || ''
    };

    fs.appendFileSync(LOG_FILE, JSON.stringify(entry) + '\n', 'utf-8');
  } catch (_) {
    // Never crash — fail silently
  }
}

/**
 * Create a duration timer for a hook
 * @param {string} hookName - Hook filename
 * @returns {{ end: (data) => void }} Timer object with end() method
 */
function createHookTimer(hookName) {
  const start = Date.now();
  return {
    end(data = {}) {
      const dur = Date.now() - start;
      logHook(hookName, { ...data, dur });
    }
  };
}

module.exports = {
  logHook,
  createHookTimer
};
