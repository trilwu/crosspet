# Generation Modes

## Step 1: Determine Output Location

1. Check if there's an active plan context (from `## Plan Context` in hook injection)
2. If active plan exists: save to `{plan_dir}/visuals/{topic-slug}.md`
3. If no active plan: save to `plans/visuals/{topic-slug}.md`
4. Create `visuals/` directory if it doesn't exist

## Step 2: Generate Content

**Mermaid Diagram Syntax:**
When generating mermaid code blocks, use `/ck:mermaidjs-v11` skill for v11 syntax rules.

**Essential rules (always apply):**
- Quote node text with special characters: `A["text with /slashes"]`
- Escape brackets in labels: `A["array[0]"]`

Use the appropriate template based on flag:

### --explain (Visual Explanation)
```markdown
# Visual Explanation: {topic}

## Overview
Brief description of what we're explaining.

## Quick View (ASCII)
[ASCII diagram of component relationships]

## Detailed Flow
[Mermaid sequence/flowchart diagram]

## Key Concepts
1. **Concept A** - Explanation
2. **Concept B** - Explanation

## Code Example (if applicable)
[Relevant code snippet with comments]
```

### --slides (Presentation Format)
```markdown
# {Topic} - Visual Presentation

---
## Slide 1: Introduction
- One concept per slide
- Bullet points only

---
## Slide 2: The Problem
[Mermaid flowchart]

---
## Slide 3: The Solution
- Key point 1
- Key point 2

---
## Slide 4: Summary
Key takeaways...
```

### --diagram (Focused Diagram)
```markdown
# Diagram: {topic}

## ASCII Version
[ASCII architecture diagram]

## Mermaid Version
[Mermaid flowchart/graph]
```

### --ascii (Terminal-Friendly Only)
```
[ASCII-only box diagram with legend]
```

## Step 3: Save and Preview

1. Write generated content to determined path
2. Start preview server with the generated file:
```bash
node .claude/skills/markdown-novel-viewer/scripts/server.cjs \
  --file "<generated-file-path>" --host 0.0.0.0 --open --foreground
```

## Step 4: Report to User

Report:
- Generated file path
- Preview URL (local + network)
- Remind: file saved in plan's `visuals/` folder for future reference
