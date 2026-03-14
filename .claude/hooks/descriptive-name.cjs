#!/usr/bin/env node

// Crash wrapper
try {
  const { isHookEnabled } = require('./lib/ck-config-utils.cjs');

  // Early exit if hook disabled in config
  if (!isHookEnabled('descriptive-name')) {
    process.exit(0);
  }

  try {
  let injectedPrompt = `## File naming guidance:
- Skip this guidance if you are creating markdown or plain text files
- Prefer kebab-case for JS/TS/Python/shell (.js, .ts, .py, .sh) with descriptive names
- Respect language conventions: C#/Java/Kotlin/Swift use PascalCase (.cs, .java, .kt, .swift), Go/Rust use snake_case (.go, .rs)
- Other languages: follow their ecosystem's standard naming convention
- Goal: self-documenting names for LLM tools (Grep, Glob, Search)`

  console.log(JSON.stringify({
    "hookSpecificOutput": {
      "hookEventName": "PreToolUse",
      "permissionDecision": "allow",
      "additionalContext": injectedPrompt
    }
  }));

    // All paths allowed
    process.exit(0);

  } catch (error) {
    // Fail-open for unexpected errors
    console.error('WARN: Hook error, allowing operation -', error.message);
    process.exit(0);
  }
} catch (e) {
  // Minimal crash logging (zero deps — only Node builtins)
  try {
    const fs = require('fs');
    const p = require('path');
    const logDir = p.join(__dirname, '.logs');
    if (!fs.existsSync(logDir)) fs.mkdirSync(logDir, { recursive: true });
    fs.appendFileSync(p.join(logDir, 'hook-log.jsonl'),
      JSON.stringify({ ts: new Date().toISOString(), hook: p.basename(__filename, '.cjs'), status: 'crash', error: e.message }) + '\n');
  } catch (_) {}
  process.exit(0); // fail-open
}
