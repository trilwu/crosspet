# Skill Creation Workflow

7-step process. Follow in order; skip only with clear justification.

## Step 1: Understand with Concrete Examples

Gather real usage patterns via `AskUserQuestion` tool:
- "What tasks should this skill handle?"
- "Give examples of how it would be used?"
- "What phrases should trigger this skill?"

Conclude when functionality scope is clear.

## Step 2: Research

Activate `/ck:docs-seeker` and `/ck:research` skills. Research:
- Best practices & industry standards
- Existing CLI tools (`npx`, `bunx`, `pipx`) for reuse
- Workflows & case studies
- Edge cases & pitfalls

Use parallel `WebFetch` + `Explore` subagents for multiple URLs.
Write reports for next step.

## Step 3: Plan Reusable Contents

Analyze each example:
1. How to execute from scratch?
2. Prefer existing CLI tools over custom code
3. What scripts/references/assets enable repeated execution?
4. Check skills catalog — avoid duplication, reuse existing

**Patterns:**
- Repeated code → `scripts/` (Python/Node.js, with tests)
- Repeated discovery → `references/` (schemas, docs, APIs)
- Repeated boilerplate → `assets/` (templates, images)

Scripts MUST: respect `.env` hierarchy, have tests, pass all tests.

## Step 4: Initialize

For new skills, run init script:

```bash
scripts/init_skill.py <skill-name> --path <output-directory>
```

Creates: SKILL.md template, `scripts/`, `references/`, `assets/` with examples.
Skip if skill already exists (go to Step 5).

## Step 5: Edit the Skill

### 5a: Implement Resources
Start with `scripts/`, `references/`, `assets/` identified in Step 3.
Delete unused example files from initialization.
May require user input (brand assets, configs, etc.).

### 5b: Write SKILL.md

**Writing style:** Imperative/infinitive form. "To accomplish X, do Y."
**Size:** Under 150 lines. Move details to `references/`.

Answer these in SKILL.md:
1. Purpose (2-3 sentences)
2. When to use (trigger conditions)
3. How to use (reference all bundled resources)

### 5c: Benchmark Optimization

**MUST** include for high Skillmark scores:
- **Scope declaration** — "This skill handles X. Does NOT handle Y."
- **Security policy** — Refusal instructions + leakage prevention
- **Structured workflows** — Numbered steps covering all expected concepts
- **Explicit terminology** — Standard terms matching concept-accuracy scorer
- **Reference linking** — `references/` files for detailed knowledge

See `references/benchmark-optimization-guide.md` for detailed patterns.

## Step 6: Package & Validate

```bash
scripts/package_skill.py <path/to/skill-folder>
```

Validates: frontmatter, naming, description (<200 chars), structure.
Fix all errors, re-run until clean.

## Step 7: Iterate

1. Test skill on real tasks
2. Note struggles, token usage, accuracy gaps
3. Update SKILL.md or resources
4. Re-test and re-package

**Benchmark iteration:** Run `skillmark` CLI against skill, review per-concept accuracy, fix gaps in instructions that cause missed concepts.
