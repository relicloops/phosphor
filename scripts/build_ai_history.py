#!/usr/bin/env python3
"""Convert docs/source RST files into ai-history.json for the phosphor website.

Usage:
    python3 scripts/build_ai_history.py docs/source website/src/static/ai-history.json

Reads all .rst files from the plans/ and reference/ subdirectories, extracts
metadata and body content, converts RST markup to HTML, and writes a JSON
array suitable for the AiHistory page component.

Uses docutils for RST-to-HTML conversion. Falls back to a lightweight regex
converter if docutils is not installed.
"""

import json
import os
import re
import sys
from pathlib import Path

# ---------------------------------------------------------------------------
# RST-to-HTML conversion
# ---------------------------------------------------------------------------

try:
    from docutils.core import publish_parts
    HAS_DOCUTILS = True
except ImportError:
    HAS_DOCUTILS = False


def rst_to_html_docutils(rst_body: str) -> str:
    """Convert RST body text to HTML using docutils."""
    settings = {
        'report_level': 5,        # suppress warnings
        'halt_level': 5,
        'initial_header_level': 3, # start at h3 (h2 is the section title)
        'input_encoding': 'utf-8',
        'output_encoding': 'utf-8',
    }
    parts = publish_parts(rst_body, writer_name='html5',
                          settings_overrides=settings)
    return parts['body']


def rst_to_html_fallback(rst_body: str) -> str:
    """Lightweight RST-to-HTML for the subset we use."""
    lines = rst_body.split('\n')
    html_parts = []
    in_code_block = False
    in_list = False
    in_table = False
    code_lines = []
    i = 0

    while i < len(lines):
        line = lines[i]

        # code block
        if line.strip().startswith('.. code-block::'):
            in_code_block = True
            code_lines = []
            i += 1
            # skip blank line after directive
            if i < len(lines) and lines[i].strip() == '':
                i += 1
            continue

        if in_code_block:
            if line.strip() == '' and code_lines and i + 1 < len(lines) and not lines[i + 1].startswith('   '):
                html_parts.append('<pre><code>' + escape_html('\n'.join(code_lines)) + '</code></pre>')
                code_lines = []
                in_code_block = False
            elif line.startswith('   '):
                code_lines.append(line[3:])
            elif line.strip() == '':
                code_lines.append('')
            else:
                if code_lines:
                    html_parts.append('<pre><code>' + escape_html('\n'.join(code_lines)) + '</code></pre>')
                    code_lines = []
                in_code_block = False
                continue  # re-process this line
            i += 1
            continue

        # skip directives we don't render
        if line.strip().startswith('.. ') and '::' in line:
            i += 1
            # skip directive body
            while i < len(lines) and (lines[i].startswith('   ') or lines[i].strip() == ''):
                i += 1
            continue

        # headings (RST underline pattern)
        if i + 1 < len(lines) and len(lines[i + 1].strip()) > 0:
            underline = lines[i + 1].strip()
            if len(underline) >= 3 and len(set(underline)) == 1 and underline[0] in '=-^~':
                tag = 'h3' if underline[0] in '=-' else 'h4'
                html_parts.append(f'<{tag}>{escape_html(line.strip())}</{tag}>')
                i += 2
                continue

        # bullet list
        if re.match(r'^- ', line):
            if not in_list:
                html_parts.append('<ul>')
                in_list = True
            html_parts.append(f'<li>{inline_markup(line[2:].strip())}</li>')
            i += 1
            continue
        elif in_list and (line.strip() == '' or not line.startswith('  ')):
            html_parts.append('</ul>')
            in_list = False

        # blank line
        if line.strip() == '':
            i += 1
            continue

        # paragraph
        para_lines = [line.strip()]
        i += 1
        while i < len(lines) and lines[i].strip() != '' and not lines[i].strip().startswith('.. ') and not re.match(r'^- ', lines[i]):
            para_lines.append(lines[i].strip())
            i += 1
        html_parts.append(f'<p>{inline_markup(" ".join(para_lines))}</p>')
        continue

    # close open blocks
    if in_code_block and code_lines:
        html_parts.append('<pre><code>' + escape_html('\n'.join(code_lines)) + '</code></pre>')
    if in_list:
        html_parts.append('</ul>')

    return '\n'.join(html_parts)


def escape_html(text: str) -> str:
    return text.replace('&', '&amp;').replace('<', '&lt;').replace('>', '&gt;')


def inline_markup(text: str) -> str:
    """Convert RST inline markup to HTML."""
    # ``code``
    text = re.sub(r'``([^`]+)``', r'<code>\1</code>', text)
    # **bold**
    text = re.sub(r'\*\*([^*]+)\*\*', r'<strong>\1</strong>', text)
    # *italic*
    text = re.sub(r'(?<!\*)\*([^*]+)\*(?!\*)', r'<em>\1</em>', text)
    return text


def rst_to_html(rst_body: str) -> str:
    if HAS_DOCUTILS:
        return rst_to_html_docutils(rst_body)
    return rst_to_html_fallback(rst_body)


# ---------------------------------------------------------------------------
# RST file parsing
# ---------------------------------------------------------------------------

def parse_meta(text: str) -> dict:
    """Extract .. meta:: fields from RST text."""
    meta = {}
    meta_match = re.search(r'\.\. meta::\s*\n((?:\s+:\w+:.*\n)*)', text)
    if meta_match:
        for m in re.finditer(r':(\w+):\s*(.+)', meta_match.group(1)):
            meta[m.group(1)] = m.group(2).strip()
    return meta


def extract_body(text: str) -> str:
    """Extract the body content after metadata and title."""
    # remove .. meta:: block
    text = re.sub(r'\.\. meta::.*?\n(?:\s+:\w+:.*\n)*', '', text)
    # remove .. index:: block
    text = re.sub(r'\.\. index::.*?\n(?:\s+\w+:.*\n)*', '', text)
    # remove title (first heading with === underline)
    text = re.sub(r'^[^\n]+\n=+\n', '', text.strip())
    return text.strip()


def status_from_filename(filename: str) -> str:
    """Extract status marker from filename."""
    m = re.search(r'\[(COMPLETED|ACTIVE|DRAFT|PENDING|BLOCKED|DEFERRED)', filename)
    if m:
        return m.group(1).lower()
    return 'unknown'


def status_symbol(status: str) -> str:
    symbols = {
        'completed': '\u2713',  # check
        'active': '\u25B8',     # triangle right
        'draft': '\u25CB',      # empty circle
        'pending': '\u25C7',    # empty diamond
        'blocked': '\u2717',    # cross
        'deferred': '\u22EF',   # midline ellipsis
    }
    return symbols.get(status, '')


# ---------------------------------------------------------------------------
# Directory scanning and categorization
# ---------------------------------------------------------------------------

def scan_plans_dir(plans_dir: Path) -> list:
    """Scan plans directory and build categorized entries."""
    entries = []

    # -- masterplan
    mp = plans_dir / 'masterplan.[ACTIVE \u25B8].rst'
    if mp.exists():
        entries.append(build_entry(mp, 'masterplan', 'masterplan'))

    # -- summary
    sm = plans_dir / 'summary.[ACTIVE \u25B8].rst'
    if sm.exists():
        entries.append(build_entry(sm, 'summary', 'overview'))

    # -- standalone plan files (completed, feature plans)
    standalone_order = [
        'abstract-args-parser-into-generic',
        'versioning-and-ci-pipeline',
        'testing-infrastructure-ceedling',
        'code-coverage-infrastructure',
        'glow-command-embedded-template',
        'embedded-build-toolchain',
        'verbose-flag-implementation',
        'soc-audit-json-consolidation',
    ]

    for slug in standalone_order:
        for f in sorted(plans_dir.glob(f'{slug}.*')):
            if f.suffix == '.rst' and f.is_file():
                entries.append(build_entry(f, slug, 'feature-plan'))
                break

    # -- phases
    for phase_num in range(7):
        phase_dir = plans_dir / f'phase-{phase_num}'
        if not phase_dir.is_dir():
            continue

        # find index file
        index_files = list(phase_dir.glob('index.*'))
        if not index_files:
            continue

        index_file = index_files[0]
        phase_entry = build_entry(index_file, f'phase-{phase_num}', 'phase')
        phase_entry['children'] = []

        # task and deliverable files
        for task_file in sorted(phase_dir.glob('*.rst')):
            if task_file.name.startswith('index'):
                continue
            task_slug = task_file.stem.split('.')[0]
            child = build_entry(task_file, f'phase-{phase_num}-{task_slug}', 'task')
            phase_entry['children'].append(child)

        entries.append(phase_entry)

    # -- phase-init (superseded)
    phase_init = plans_dir / 'phase-init'
    if phase_init.is_dir():
        init_files = list(phase_init.glob('index.*'))
        if init_files:
            entry = build_entry(init_files[0], 'phase-init', 'phase')
            entry['children'] = []
            for f in sorted(phase_init.glob('*.rst')):
                if f.name.startswith('index'):
                    continue
                slug = f.stem.split('.')[0]
                entry['children'].append(
                    build_entry(f, f'phase-init-{slug}', 'task'))
            entries.append(entry)

    return entries


def scan_reference_dir(ref_dir: Path) -> list:
    """Scan reference directory."""
    entries = []
    for f in sorted(ref_dir.glob('*.rst')):
        slug = f.stem.split('.')[0]
        entries.append(build_entry(f, f'ref-{slug}', 'reference'))
    return entries


def build_entry(filepath: Path, entry_id: str, category: str) -> dict:
    """Build a single JSON entry from an RST file."""
    text = filepath.read_text(encoding='utf-8')
    meta = parse_meta(text)
    body = extract_body(text)
    status = meta.get('status', status_from_filename(filepath.name))
    title = meta.get('title', entry_id.replace('-', ' '))
    updated = meta.get('updated', '')

    html_content = rst_to_html(body)

    return {
        'id': entry_id,
        'title': title,
        'status': status,
        'statusSymbol': status_symbol(status),
        'updated': updated,
        'category': category,
        'content': html_content,
    }


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    if len(sys.argv) != 3:
        print(f'Usage: {sys.argv[0]} <docs-source-dir> <output-json>',
              file=sys.stderr)
        sys.exit(1)

    docs_dir = Path(sys.argv[1])
    output_path = Path(sys.argv[2])

    if not docs_dir.is_dir():
        print(f'Error: {docs_dir} is not a directory', file=sys.stderr)
        sys.exit(1)

    plans_dir = docs_dir / 'plans'
    ref_dir = docs_dir / 'reference'

    all_entries = []

    # plans
    if plans_dir.is_dir():
        plan_entries = scan_plans_dir(plans_dir)
        all_entries.extend(plan_entries)

    # reference docs
    if ref_dir.is_dir():
        ref_entries = scan_reference_dir(ref_dir)
        all_entries.extend(ref_entries)

    # write output
    output_path.parent.mkdir(parents=True, exist_ok=True)
    with open(output_path, 'w', encoding='utf-8') as f:
        json.dump(all_entries, f, ensure_ascii=False, indent=2)

    print(f'Generated {len(all_entries)} entries -> {output_path}')

    # summary
    categories = {}
    for e in all_entries:
        cat = e['category']
        categories[cat] = categories.get(cat, 0) + 1
        for child in e.get('children', []):
            categories[child['category']] = categories.get(child['category'], 0) + 1

    for cat, count in sorted(categories.items()):
        print(f'  {cat}: {count}')


if __name__ == '__main__':
    main()
