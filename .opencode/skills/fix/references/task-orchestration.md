# Task Orchestration

Native Claude Task tools for tracking and coordinating fix workflows.

## When to Use Tasks

| Complexity | Use Tasks? | Reason |
|-----------|-----------|--------|
| Simple/Quick | No | < 3 steps, overhead exceeds benefit |
| Moderate (Standard) | Yes | 6 steps, multi-subagent coordination |
| Complex (Deep) | Yes | 8 steps, dependency chains, parallel agents |
| Parallel | Yes | Multiple independent issue trees |

## Task Tools

- `TaskCreate(subject, description, activeForm, metadata)` - Create task
- `TaskUpdate(taskId, status, addBlockedBy, addBlocks)` - Update status/deps
- `TaskGet(taskId)` - Get full task details
- `TaskList()` - List all tasks with status

**Lifecycle:** `pending` → `in_progress` → `completed`

## Standard Workflow Tasks

Create all tasks upfront, then work through them:

```
TaskCreate(subject="Debug & investigate", activeForm="Debugging issue", metadata={step: 1})
TaskCreate(subject="Scout related code", activeForm="Scouting codebase", metadata={step: 2})
TaskCreate(subject="Implement fix", activeForm="Implementing fix", metadata={step: 3}, addBlockedBy=[step1, step2])
TaskCreate(subject="Run tests", activeForm="Running tests", metadata={step: 4}, addBlockedBy=[step3])
TaskCreate(subject="Code review", activeForm="Reviewing code", metadata={step: 5}, addBlockedBy=[step4])
TaskCreate(subject="Finalize", activeForm="Finalizing", metadata={step: 6}, addBlockedBy=[step5])
```

Update as work progresses:
```
TaskUpdate(taskId=step1, status="in_progress")
// ... do work ...
TaskUpdate(taskId=step1, status="completed")
// step3 auto-unblocks when step1 + step2 complete
```

## Deep Workflow Tasks

Same pattern but with research/brainstorm/plan phases:

```
TaskCreate(subject="Debug & investigate",    metadata={step: 1, phase: "diagnose"})
TaskCreate(subject="Research solutions",      metadata={step: 2, phase: "research"})
TaskCreate(subject="Brainstorm approaches",   metadata={step: 3, phase: "design"}, addBlockedBy=[step2])
TaskCreate(subject="Create implementation plan", metadata={step: 4, phase: "design"}, addBlockedBy=[step3])
TaskCreate(subject="Implement fix",           metadata={step: 5, phase: "implement"}, addBlockedBy=[step1, step4])
TaskCreate(subject="Run tests",               metadata={step: 6, phase: "verify"}, addBlockedBy=[step5])
TaskCreate(subject="Code review",             metadata={step: 7, phase: "verify"}, addBlockedBy=[step6])
TaskCreate(subject="Finalize & docs",         metadata={step: 8, phase: "finalize"}, addBlockedBy=[step7])
```

**Note:** Steps 1 and 2 run in parallel (debug + research simultaneously).

## Parallel Issue Coordination

For 2+ independent issues, create separate task trees per issue:

```
// Issue A tree
TaskCreate(subject="[Issue A] Debug",     metadata={issue: "A", step: 1})
TaskCreate(subject="[Issue A] Fix",       metadata={issue: "A", step: 2}, addBlockedBy=[A-step1])
TaskCreate(subject="[Issue A] Verify",    metadata={issue: "A", step: 3}, addBlockedBy=[A-step2])

// Issue B tree
TaskCreate(subject="[Issue B] Debug",     metadata={issue: "B", step: 1})
TaskCreate(subject="[Issue B] Fix",       metadata={issue: "B", step: 2}, addBlockedBy=[B-step1])
TaskCreate(subject="[Issue B] Verify",    metadata={issue: "B", step: 3}, addBlockedBy=[B-step2])

// Final shared task
TaskCreate(subject="Integration verify",  addBlockedBy=[A-step3, B-step3])
```

Spawn `fullstack-developer` subagents per issue tree. Each agent:
1. Claims tasks via `TaskUpdate(status="in_progress")`
2. Completes tasks via `TaskUpdate(status="completed")`
3. Blocked tasks auto-unblock when dependencies resolve

## Subagent Task Assignment

Assign tasks to subagents via `owner` field:

```
TaskUpdate(taskId=taskA, owner="agent-debug")
TaskUpdate(taskId=taskB, owner="agent-fix")
```

Check available work: `TaskList()` → filter by `status=pending`, `blockedBy=[]`, `owner=null`

## Rules

- Create tasks BEFORE starting work (upfront planning)
- Only 1 task `in_progress` per agent at a time
- Mark complete IMMEDIATELY after finishing (don't batch)
- Use `metadata` for filtering: `{step, phase, issue, severity}`
- If task fails → keep `in_progress`, create subtask for blocker
- Skip Tasks entirely for Quick workflow (< 3 steps)
