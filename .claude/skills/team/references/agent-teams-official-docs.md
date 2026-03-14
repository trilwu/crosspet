# Agent Teams — Overview & Architecture

> **Canonical source:** https://code.claude.com/docs/en/agent-teams
> **Version captured:** Claude Code (Feb 2026)
> **Update policy:** Re-fetch canonical URL when Claude Code releases new Agent Teams features.

This is a **self-contained knowledge base** — AI agents should NOT need to re-fetch the URL.

## Overview

Agent Teams coordinate multiple Claude Code instances working together. One session acts as the team lead, coordinating work, assigning tasks, and synthesizing results. Teammates work independently, each in its own context window, and communicate directly with each other.

Unlike subagents (run within a single session, report back only), teammates are full independent sessions you can interact with directly.

## When to Use

Best for tasks where parallel exploration adds real value:

- **Research and review**: multiple teammates investigate different aspects, share and challenge findings
- **New modules or features**: teammates each own a separate piece without conflicts
- **Debugging with competing hypotheses**: test different theories in parallel
- **Cross-layer coordination**: changes spanning frontend, backend, tests — each owned by different teammate

**Not suitable for:** sequential tasks, same-file edits, work with many dependencies.

### Subagents vs Agent Teams

| | Subagents | Agent Teams |
|---|---|---|
| **Context** | Own window; results return to caller | Own window; fully independent |
| **Communication** | Report back to main agent only | Message each other directly |
| **Coordination** | Main agent manages all work | Shared task list, self-coordination |
| **Best for** | Focused tasks, result-only | Complex work requiring discussion |
| **Token cost** | Lower | Higher (each teammate = separate instance) |

## Enable

Still experimental — requires opt-in:

```json
{ "env": { "CLAUDE_CODE_EXPERIMENTAL_AGENT_TEAMS": "1" } }
```

Set in shell environment or settings.json.

## How Teams Start

Two paths:
1. **You request**: describe task + ask for agent team. Claude creates based on instructions.
2. **Claude proposes**: suggests team if task benefits from parallel work.

Both require your confirmation. Claude won't create a team without approval.

## Architecture

| Component | Role |
|-----------|------|
| **Team lead** | Main session — creates team, spawns teammates, coordinates |
| **Teammates** | Separate Claude Code instances with own context windows |
| **Task list** | Shared work items at `~/.claude/tasks/{team-name}/` |
| **Mailbox** | Messaging system for inter-agent communication |

Storage:
- **Team config**: `~/.claude/teams/{team-name}/config.json` (members array with name, agent ID, type)
- **Task list**: `~/.claude/tasks/{team-name}/`

Task dependencies managed automatically — completing a blocking task unblocks dependents without manual intervention.

## Tools API Surface

### TeamCreate

Create team + task list. Params: `team_name`, `description`.

### TeamDelete

Remove team/task dirs. **Takes NO parameters** — just call `TeamDelete` with empty params. Fails if active teammates still exist.

### SendMessage Types

| Type | Purpose |
|------|---------|
| `message` | DM to one teammate (requires `recipient`) |
| `broadcast` | Send to ALL teammates (use sparingly — costs scale with N) |
| `shutdown_request` | Ask teammate to gracefully exit |
| `shutdown_response` | Teammate approves/rejects shutdown (requires `request_id`) |
| `plan_approval_response` | Lead approves/rejects teammate plan (requires `request_id`) |

### Task System Fields

| Field | Values/Purpose |
|-------|---------------|
| `status` | `pending` → `in_progress` → `completed` (or `deleted`) |
| `owner` | Agent name assigned to task |
| `blocks` | Task IDs this task blocks (read via TaskGet) |
| `blockedBy` | Task IDs that must complete first (read via TaskGet) |
| `addBlocks` | Set blocking relations (write via TaskUpdate) |
| `addBlockedBy` | Set dependency relations (write via TaskUpdate) |
| `metadata` | Arbitrary key-value pairs |
| `subject` | Brief imperative title |
| `description` | Full requirements and context |

Task claiming uses file locking to prevent race conditions.

## Hook Events (2.1.33+)

### TaskCompleted

Fires when teammate calls `TaskUpdate` with `status: "completed"`.

| Field | Type | Description |
|-------|------|-------------|
| `task_id` | string | Completed task ID |
| `task_subject` | string | Task title |
| `task_description` | string | Full task description |
| `teammate_name` | string | Who completed it |
| `team_name` | string | Team name |

Note: Does NOT include `permission_mode`.

### TeammateIdle

Fires after `SubagentStop` for team members.

| Field | Type | Description |
|-------|------|-------------|
| `teammate_name` | string | Idle teammate name |
| `team_name` | string | Team name |

Note: Includes `permission_mode`. Always pairs with SubagentStop.

### Event Lifecycle

```
SubagentStart(worker) → TaskCompleted(task) → SubagentStop(worker) → TeammateIdle(worker)
```

TaskCompleted fires BEFORE SubagentStop/TeammateIdle.

## Agent Memory

Agents can declare `memory` in frontmatter for persistent cross-session learning.

| Scope | Location | Persists across |
|-------|----------|-----------------|
| `user` | `~/.claude/agent-memory/<name>/` | All projects |
| `project` | `.claude/agent-memory/<name>/` | Sessions in same project |

First 200 lines of `MEMORY.md` auto-injected into system prompt.

## Task(agent_type) Restrictions

Limit which sub-agents an agent can spawn:

```yaml
tools: Read, Grep, Bash, Task(Explore)
```

This agent can only spawn `Explore` sub-agents. Restricts recursive spawning and cost escalation.

## Context & Communication

Each teammate loads: CLAUDE.md, MCP servers, skills, agents. Receives spawn prompt from lead. Lead's conversation history does NOT carry over.

- **Automatic message delivery** — no polling needed
- **Idle notifications** — teammates notify lead when turn ends
- **Shared task list** — all agents see status and claim work

## Permissions

Teammates inherit lead's permission settings at spawn. If lead uses `--dangerously-skip-permissions`, all teammates do too. Can change individually after spawning but not at spawn time.

## Token Usage

Scales with active teammates. Worth it for research/review/features. Single session more cost-effective for routine tasks.
