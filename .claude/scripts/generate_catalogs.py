#!/usr/bin/env python3
"""Generate updated command and skill catalogs.

Outputs YAML to stdout by default for easy consumption by Claude.
Use --output to write to a specific file instead.
"""

import argparse
import sys
try:
    import yaml
except ModuleNotFoundError:
    raise SystemExit(
        "PyYAML is required. Install with: python3 -m pip install -r .claude/scripts/requirements.txt"
    )
from pathlib import Path
from datetime import datetime

# Script directory for resolving relative paths
SCRIPT_DIR = Path(__file__).parent

# Windows UTF-8 compatibility (use shared utility)
try:
    from win_compat import ensure_utf8_stdout
    ensure_utf8_stdout()
except ImportError:
    if sys.platform == 'win32':
        import io
        if hasattr(sys.stdout, 'buffer'):
            sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')


def load_yaml(filename):
    """Load YAML file from script directory with helpful error handling."""
    primary = SCRIPT_DIR / filename

    # skills_data canonical source moved under ck-help skill.
    # SCRIPT_DIR is .claude/scripts/, parent is .claude/
    if filename == 'skills_data.yaml':
        canonical = SCRIPT_DIR.parent / 'skills' / 'ck-help' / 'scripts' / 'skills_data.yaml'
        if canonical.exists():
            loaded = yaml.safe_load(canonical.read_text(encoding='utf-8'))
            return loaded if loaded is not None else []

    if not primary.exists():
        print(f"Error: {primary} not found", file=sys.stderr)
        print(f"Hint: Run scan_skills.py or scan_commands.py first to generate data files", file=sys.stderr)
        sys.exit(1)

    loaded = yaml.safe_load(primary.read_text(encoding='utf-8'))
    if loaded is None:
        return []
    return loaded


def generate_commands_yaml():
    """Generate COMMANDS.yaml catalog."""
    commands = load_yaml('commands_data.yaml')

    # Group by category
    categories = {}
    for cmd in commands:
        cat = cmd['category']
        if cat not in categories:
            categories[cat] = []
        categories[cat].append(cmd)

    # Sort commands within each category
    for cat in categories:
        categories[cat] = sorted(categories[cat], key=lambda x: x['name'])

    # Generate catalog structure
    catalog = {
        'metadata': {
            'title': 'Commands Catalog',
            'description': 'Auto-generated catalog of all available commands in ClaudeKit Engineer',
            'last_updated': datetime.now().strftime('%Y-%m-%d'),
            'total_commands': len(commands)
        },
        'categories': {
            'core': 'Core Commands',
            'bootstrap': 'Bootstrap Commands',
            'content': 'Content Creation',
            'cook': 'Cook Commands',
            'design': 'Design Commands',
            'docs': 'Documentation',
            'fix': 'Fix & Debug',
            'git': 'Git Commands',
            'integrate': 'Integrations',
            'plan': 'Planning',
            'review': 'Code Review',
            'scout': 'Scout Commands',
            'skill': 'Skill Management'
        },
        'commands': categories
    }

    return yaml.dump(catalog, sort_keys=False, allow_unicode=True, default_flow_style=False)


def generate_skills_yaml():
    """Generate SKILLS.yaml catalog."""
    skills = load_yaml('skills_data.yaml')

    # Group by category
    categories = {}
    for skill in skills:
        cat = skill['category']
        if cat not in categories:
            categories[cat] = []
        categories[cat].append(skill)

    # Sort skills within each category
    for cat in categories:
        categories[cat] = sorted(categories[cat], key=lambda x: x['name'])

    # Generate catalog structure
    catalog = {
        'metadata': {
            'title': 'Skills Catalog',
            'description': 'Auto-generated catalog of all available skills in ClaudeKit Engineer',
            'last_updated': datetime.now().strftime('%Y-%m-%d'),
            'total_skills': len(skills)
        },
        'categories': {
            'ai-ml': 'AI & Machine Learning',
            'frontend': 'Frontend & Design',
            'backend': 'Backend Development',
            'infrastructure': 'Infrastructure & DevOps',
            'database': 'Database & Storage',
            'dev-tools': 'Development Tools',
            'multimedia': 'Multimedia & Processing',
            'frameworks': 'Frameworks & Platforms',
            'utilities': 'Utilities & Helpers',
            'core': 'Core Workflow',
            'plan': 'Planning & Delivery',
            'review': 'Code Review',
            'test': 'Testing & QA',
            'docs': 'Documentation',
            'tools': 'Tooling',
            'infra': 'Infrastructure',
            'kanban': 'Kanban & Planning Board',
            'worktree': 'Git Worktree',
            'journal': 'Journaling',
            'watzup': 'Session Wrap-up',
            'other': 'Other'
        },
        'legend': {
            'has_scripts': 'Has executable scripts',
            'has_references': 'Has reference documentation'
        },
        'skills': categories
    }

    return yaml.dump(catalog, sort_keys=False, allow_unicode=True, default_flow_style=False)


def write_output(content, output_path=None, label=None):
    """Write content to stdout or file."""
    if output_path:
        path = Path(output_path)
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content, encoding='utf-8')
        print(f"Generated {output_path}", file=sys.stderr)
    else:
        print(content)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description='Generate command and skill catalogs',
        epilog='Outputs to stdout by default. Use --output to write to a file.'
    )
    parser.add_argument('--skills', action='store_true', help='Generate only skills catalog')
    parser.add_argument('--commands', action='store_true', help='Generate only commands catalog')
    parser.add_argument('--output', '-o', metavar='PATH', help='Write output to file instead of stdout')
    args = parser.parse_args()

    # Validate: --output requires exactly one of --skills or --commands
    if args.output and not (args.skills ^ args.commands):
        print("Error: --output requires exactly one of --skills or --commands", file=sys.stderr)
        sys.exit(1)

    # If no specific flag, generate both (to stdout only)
    generate_both = not (args.skills or args.commands)

    if args.commands or generate_both:
        commands_yaml = generate_commands_yaml()
        if generate_both:
            print("# === COMMANDS CATALOG ===")
        write_output(commands_yaml, args.output if args.commands else None)

    if args.skills or generate_both:
        skills_yaml = generate_skills_yaml()
        if generate_both:
            print("\n# === SKILLS CATALOG ===")
        write_output(skills_yaml, args.output if args.skills else None)
