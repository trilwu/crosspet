# Standard Workflow

Full pipeline for moderate complexity issues. Uses native Claude Tasks for phase tracking.

## Task Setup (Before Starting)

Create all phase tasks upfront with dependencies. See `references/task-orchestration.md`.

```
T1 = TaskCreate(subject="Debug & investigate",  activeForm="Debugging issue")
T2 = TaskCreate(subject="Scout related code",   activeForm="Scouting codebase")
T3 = TaskCreate(subject="Implement fix",        activeForm="Implementing fix",    addBlockedBy=[T1, T2])
T4 = TaskCreate(subject="Run tests",            activeForm="Running tests",       addBlockedBy=[T3])
T5 = TaskCreate(subject="Code review",          activeForm="Reviewing code",      addBlockedBy=[T4])
T6 = TaskCreate(subject="Finalize",             activeForm="Finalizing",          addBlockedBy=[T5])
```

## Steps

### Step 1: Debug & Investigate
`TaskUpdate(T1, status="in_progress")`
Activate `ck:debug` skill. Use `debugger` subagent if needed.

- Read error messages, logs, stack traces
- Reproduce the issue
- Trace backward to root cause
- Identify all affected files

`TaskUpdate(T1, status="completed")`
**Output:** `✓ Step 1: Root cause - [summary], [N] files affected`

### Step 2: Parallel Scout
`TaskUpdate(T2, status="in_progress")`
Launch multiple `Explore` subagents in parallel to scout and verify the root cause.

**Pattern:** In SINGLE message, launch 2-3 Explore agents:
```
Task("Explore", "Find [area1] files related to issue", "Scout area1")
Task("Explore", "Find [area2] patterns/usage", "Scout area2")
Task("Explore", "Find [area3] tests/dependencies", "Scout area3")
```

- Only if unclear which files need changes
- Find patterns, similar implementations, dependencies

See `references/parallel-exploration.md` for patterns.

`TaskUpdate(T2, status="completed")`
**Output:** `✓ Step 2: Scouted [N] areas - Found [M] related files`

### Step 3: Implement Fix
`TaskUpdate(T3, status="in_progress")` — auto-unblocked when T1 + T2 complete.
Fix the issue following debugging findings.

- Apply `ck:problem-solving` skill if stuck
- Use `ck:sequential-thinking` for complex logic

**After implementation - Parallel Verification:**
Launch `Bash` agents in parallel to verify:
```
Task("Bash", "Run typecheck", "Verify types")
Task("Bash", "Run lint", "Verify lint")
Task("Bash", "Run build", "Verify build")
```

`TaskUpdate(T3, status="completed")`
**Output:** `✓ Step 3: Implemented - [N] files, verified (types/lint/build passed)`

### Step 4: Test
`TaskUpdate(T4, status="in_progress")`
Use `tester` subagent to run tests.

- Write new tests if needed
- Run existing test suite
- If fail → use `debugger`, fix, repeat

`TaskUpdate(T4, status="completed")`
**Output:** `✓ Step 4: Tests [X/X passed]`

### Step 5: Review
`TaskUpdate(T5, status="in_progress")`
Use `code-reviewer` subagent.

See `references/review-cycle.md` for mode-specific handling.

`TaskUpdate(T5, status="completed")`
**Output:** `✓ Step 5: Review [score]/10 - [status]`

### Step 6: Finalize
`TaskUpdate(T6, status="in_progress")`
- Report summary to user
- Ask to commit via `git-manager` subagent
- Update docs if needed via `docs-manager`

`TaskUpdate(T6, status="completed")`
**Output:** `✓ Step 6: Complete - [action]`

## Skills/Subagents Activated

| Step | Skills/Subagents |
|------|------------------|
| 1 | `ck:debug`, `debugger` subagent |
| 2 | Multiple `Explore` subagents in parallel (optional) |
| 3 | `ck:problem-solving`, `ck:sequential-thinking`, parallel `Bash` for verification |
| 4 | `tester` subagent |
| 5 | `code-reviewer` subagent |
| 6 | `git-manager`, `docs-manager` subagents |

**Rules:** Don't skip steps. Validate before proceeding. One phase at a time.
**Frontend:** Use `chrome`, `ck:chrome-devtools` or any relevant skills/tools to verify.
**Visual Assets:** Use `ck:ai-multimodal` for visual assets generation, analysis and verification.
