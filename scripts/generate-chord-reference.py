#!/usr/bin/env python3
"""Generate the readable chord reference from the shared parser/glossary catalog."""
import json
from pathlib import Path

root = Path(__file__).resolve().parent.parent
chords = json.loads((root / 'assets/chords.json').read_text())
lines = [
    '# Chord vocabulary', '',
    'Generated from `assets/chords.json` with `python3 scripts/generate-chord-reference.py`.', '',
    f'{len(chords)} chord types and {sum(1 + len(chord["aliases"]) for chord in chords)} accepted spellings, including the unsuffixed major chord. Open **Chord glossary** in the editor to search this list, choose a root, and add examples to your progression draft.', '',
    'Roots: A–G (upper or lower case), with # / b or ♯ / ♭. Suffixes are case-sensitive: `m7` is minor; `M7` is major. Each formula gives the exact root-position voicing. 9, 11, and 13 are above the octave; all listed degrees sound. Added tones omit the seventh. Full 11ths and 13ths can sound dense.', '',
    'Numbers identify chord types before durations. Use `@` for unambiguous bar lengths: `C9` is a ninth chord, `C@9` is a nine-bar major chord, and `C9@2` is a two-bar ninth chord. Legacy unambiguous shorthand such as `Am2` and `C1/2` still works. For the 6/9 sound use `C6add9` or `C69`; `C6/9` remains a fractional duration. Slash bass chords and inversions are not supported.', '',
    'Try `Dm9@1/2 G13b9@1/2 | Cmaj9@2`, `Fm9 - Bb13 - Ebmaj9 - Abmaj7#11`, or `Dm7b5 - G7(b9,#11) - Cm(maj9)`.', '',
    'Projects use schema 3. Schema 1 and 2 projects retain their old notes and durations by migrating numeric durations to explicit `@` notation.', ''
]
for family in dict.fromkeys(chord['family'] for chord in chords):
    lines += [f'## {family}', '', '| Suffix | Name | Degrees | Aliases | Example |', '| --- | --- | --- | --- | --- |']
    for chord in (item for item in chords if item['family'] == family):
        suffix = f'`{chord["suffix"]}`' if chord['suffix'] else '(none)'
        aliases = ', '.join(f'`{alias}`' for alias in chord['aliases']) or '—'
        lines.append(f'| {suffix} | {chord["name"]} | {" · ".join(chord["degrees"])} | {aliases} | `C{chord["suffix"]}` |')
    lines.append('')
(root / 'docs/chords.md').write_text('\n'.join(lines))
