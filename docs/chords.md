# Chord vocabulary

Generated from `assets/chords.json` with `python3 scripts/generate-chord-reference.py`.

77 chord types and 183 accepted spellings, including the unsuffixed major chord. Open **Chord glossary** in the editor to search this list, choose a root, and add examples to your progression draft.

Roots: A–G (upper or lower case), with # / b or ♯ / ♭. Suffixes are case-sensitive: `m7` is minor; `M7` is major. Each formula gives the exact root-position voicing. 9, 11, and 13 are above the octave; all listed degrees sound. Added tones omit the seventh. Full 11ths and 13ths can sound dense.

Numbers identify chord types before durations. Use `@` for unambiguous bar lengths: `C9` is a ninth chord, `C@9` is a nine-bar major chord, and `C9@2` is a two-bar ninth chord. Legacy unambiguous shorthand such as `Am2` and `C1/2` still works. For the 6/9 sound use `C6add9` or `C69`; `C6/9` remains a fractional duration. Slash bass chords and inversions are not supported.

Try `Dm9@1/2 G13b9@1/2 | Cmaj9@2`, `Fm9 - Bb13 - Ebmaj9 - Abmaj7#11`, or `Dm7b5 - G7(b9,#11) - Cm(maj9)`.

Projects use schema 3. Schema 1 and 2 projects retain their old notes and durations by migrating numeric durations to explicit `@` notation.

## Basics

| Suffix | Name | Degrees | Aliases | Example |
| --- | --- | --- | --- | --- |
| (none) | Major | 1 · 3 · 5 | `maj`, `M` | `C` |
| `m` | Minor | 1 · b3 · 5 | `min` | `Cm` |
| `dim` | Diminished | 1 · b3 · b5 | `o`, `°` | `Cdim` |
| `aug` | Augmented | 1 · 3 · #5 | `+` | `Caug` |
| `sus2` | Suspended second | 1 · 2 · 5 | — | `Csus2` |
| `sus4` | Suspended fourth | 1 · 4 · 5 | `sus` | `Csus4` |
| `5` | Power chord | 1 · 5 | — | `C5` |

## Added tones

| Suffix | Name | Degrees | Aliases | Example |
| --- | --- | --- | --- | --- |
| `add2` | Added second | 1 · 2 · 3 · 5 | — | `Cadd2` |
| `add4` | Added fourth | 1 · 3 · 4 · 5 | — | `Cadd4` |
| `add9` | Added ninth | 1 · 3 · 5 · 9 | — | `Cadd9` |
| `add11` | Added eleventh | 1 · 3 · 5 · 11 | — | `Cadd11` |
| `add#11` | Added sharp eleventh | 1 · 3 · 5 · #11 | — | `Cadd#11` |
| `add13` | Added thirteenth | 1 · 3 · 5 · 13 | — | `Cadd13` |
| `madd9` | Minor added ninth | 1 · b3 · 5 · 9 | `minadd9`, `m(add9)` | `Cmadd9` |
| `madd11` | Minor added eleventh | 1 · b3 · 5 · 11 | `minadd11` | `Cmadd11` |
| `madd13` | Minor added thirteenth | 1 · b3 · 5 · 13 | `minadd13` | `Cmadd13` |
| `7add11` | Dominant seventh, added eleventh | 1 · 3 · 5 · b7 · 11 | `7(add11)` | `C7add11` |
| `7add13` | Dominant seventh, added thirteenth | 1 · 3 · 5 · b7 · 13 | `7(add13)` | `C7add13` |

## Sixths

| Suffix | Name | Degrees | Aliases | Example |
| --- | --- | --- | --- | --- |
| `6` | Major sixth | 1 · 3 · 5 · 6 | `maj6`, `M6` | `C6` |
| `m6` | Minor sixth | 1 · b3 · 5 · 6 | `min6` | `Cm6` |
| `6add9` | Six-nine | 1 · 3 · 5 · 6 · 9 | `69`, `maj69`, `maj6add9` | `C6add9` |
| `m6add9` | Minor six-nine | 1 · b3 · 5 · 6 · 9 | `m69`, `min69`, `min6add9` | `Cm6add9` |

## Sevenths

| Suffix | Name | Degrees | Aliases | Example |
| --- | --- | --- | --- | --- |
| `7` | Dominant seventh | 1 · 3 · 5 · b7 | `dom7` | `C7` |
| `maj7` | Major seventh | 1 · 3 · 5 · 7 | `M7`, `Δ7` | `Cmaj7` |
| `m7` | Minor seventh | 1 · b3 · 5 · b7 | `min7` | `Cm7` |
| `mMaj7` | Minor-major seventh | 1 · b3 · 5 · 7 | `mM7`, `minMaj7`, `m(maj7)`, `mΔ7` | `CmMaj7` |
| `dim7` | Diminished seventh | 1 · b3 · b5 · bb7 | `o7`, `°7` | `Cdim7` |
| `m7b5` | Half-diminished seventh | 1 · b3 · b5 · b7 | `min7b5`, `ø`, `ø7`, `m7(b5)` | `Cm7b5` |
| `7sus2` | Dominant suspended second | 1 · 2 · 5 · b7 | — | `C7sus2` |
| `7sus4` | Dominant suspended fourth | 1 · 4 · 5 · b7 | `7sus` | `C7sus4` |

## Extensions

| Suffix | Name | Degrees | Aliases | Example |
| --- | --- | --- | --- | --- |
| `9` | Dominant ninth | 1 · 3 · 5 · b7 · 9 | `dom9` | `C9` |
| `maj9` | Major ninth | 1 · 3 · 5 · 7 · 9 | `M9`, `Δ9` | `Cmaj9` |
| `m9` | Minor ninth | 1 · b3 · 5 · b7 · 9 | `min9` | `Cm9` |
| `mMaj9` | Minor-major ninth | 1 · b3 · 5 · 7 · 9 | `mM9`, `m(maj9)`, `minMaj9` | `CmMaj9` |
| `11` | Dominant eleventh | 1 · 3 · 5 · b7 · 9 · 11 | `dom11` | `C11` |
| `maj11` | Major eleventh | 1 · 3 · 5 · 7 · 9 · 11 | `M11`, `Δ11` | `Cmaj11` |
| `m11` | Minor eleventh | 1 · b3 · 5 · b7 · 9 · 11 | `min11` | `Cm11` |
| `mMaj11` | Minor-major eleventh | 1 · b3 · 5 · 7 · 9 · 11 | `mM11`, `m(maj11)`, `minMaj11` | `CmMaj11` |
| `13` | Dominant thirteenth | 1 · 3 · 5 · b7 · 9 · 11 · 13 | `dom13` | `C13` |
| `maj13` | Major thirteenth | 1 · 3 · 5 · 7 · 9 · 11 · 13 | `M13`, `Δ13` | `Cmaj13` |
| `m13` | Minor thirteenth | 1 · b3 · 5 · b7 · 9 · 11 · 13 | `min13` | `Cm13` |
| `mMaj13` | Minor-major thirteenth | 1 · b3 · 5 · 7 · 9 · 11 · 13 | `mM13`, `m(maj13)`, `minMaj13` | `CmMaj13` |
| `9sus4` | Suspended ninth | 1 · 4 · 5 · b7 · 9 | `9sus` | `C9sus4` |
| `13sus4` | Suspended thirteenth | 1 · 4 · 5 · b7 · 9 · 13 | `13sus` | `C13sus4` |

## Altered dominants

| Suffix | Name | Degrees | Aliases | Example |
| --- | --- | --- | --- | --- |
| `7b5` | Dominant flat fifth | 1 · 3 · b5 · b7 | `7(b5)` | `C7b5` |
| `7#5` | Augmented dominant seventh | 1 · 3 · #5 · b7 | `aug7`, `+7`, `7(#5)` | `C7#5` |
| `7b9` | Dominant flat ninth | 1 · 3 · 5 · b7 · b9 | `7(b9)` | `C7b9` |
| `7#9` | Dominant sharp ninth | 1 · 3 · 5 · b7 · #9 | `7(#9)` | `C7#9` |
| `7#11` | Dominant sharp eleventh | 1 · 3 · 5 · b7 · #11 | `7(#11)` | `C7#11` |
| `7b13` | Dominant flat thirteenth | 1 · 3 · 5 · b7 · b13 | `7(b13)` | `C7b13` |
| `7b5b9` | Dominant flat fifth, flat ninth | 1 · 3 · b5 · b7 · b9 | `7(b5,b9)` | `C7b5b9` |
| `7b5#9` | Dominant flat fifth, sharp ninth | 1 · 3 · b5 · b7 · #9 | `7(b5,#9)` | `C7b5#9` |
| `7#5b9` | Dominant sharp fifth, flat ninth | 1 · 3 · #5 · b7 · b9 | `7(#5,b9)` | `C7#5b9` |
| `7#5#9` | Dominant sharp fifth, sharp ninth | 1 · 3 · #5 · b7 · #9 | `7(#5,#9)` | `C7#5#9` |
| `7b9#11` | Dominant flat ninth, sharp eleventh | 1 · 3 · 5 · b7 · b9 · #11 | `7(b9,#11)` | `C7b9#11` |
| `7#9#11` | Dominant sharp ninth, sharp eleventh | 1 · 3 · 5 · b7 · #9 · #11 | `7(#9,#11)` | `C7#9#11` |
| `7b9b13` | Dominant flat ninth, flat thirteenth | 1 · 3 · 5 · b7 · b9 · b13 | `7(b9,b13)` | `C7b9b13` |
| `7#9b13` | Dominant sharp ninth, flat thirteenth | 1 · 3 · 5 · b7 · #9 · b13 | `7(#9,b13)` | `C7#9b13` |
| `7alt` | Altered dominant (both altered fifths and ninths) | 1 · 3 · b5 · #5 · b7 · b9 · #9 | `alt` | `C7alt` |
| `9b5` | Dominant ninth, flat fifth | 1 · 3 · b5 · b7 · 9 | `9(b5)` | `C9b5` |
| `9#5` | Augmented dominant ninth | 1 · 3 · #5 · b7 · 9 | `aug9`, `+9`, `9(#5)` | `C9#5` |
| `9#11` | Dominant ninth, sharp eleventh | 1 · 3 · 5 · b7 · 9 · #11 | `9(#11)` | `C9#11` |
| `9b13` | Dominant ninth, flat thirteenth | 1 · 3 · 5 · b7 · 9 · b13 | `9(b13)` | `C9b13` |
| `13b9` | Dominant thirteenth, flat ninth | 1 · 3 · 5 · b7 · b9 · 13 | `13(b9)` | `C13b9` |
| `13#9` | Dominant thirteenth, sharp ninth | 1 · 3 · 5 · b7 · #9 · 13 | `13(#9)` | `C13#9` |
| `13#11` | Dominant thirteenth, sharp eleventh | 1 · 3 · 5 · b7 · 9 · #11 · 13 | `13(#11)` | `C13#11` |
| `13b9#11` | Dominant thirteenth, flat ninth, sharp eleventh | 1 · 3 · 5 · b7 · b9 · #11 · 13 | `13(b9,#11)` | `C13b9#11` |

## Major colors

| Suffix | Name | Degrees | Aliases | Example |
| --- | --- | --- | --- | --- |
| `maj7b5` | Major seventh, flat fifth | 1 · 3 · b5 · 7 | `M7b5`, `maj7(b5)` | `Cmaj7b5` |
| `maj7#5` | Augmented major seventh | 1 · 3 · #5 · 7 | `augMaj7`, `+maj7`, `M7#5`, `maj7(#5)` | `Cmaj7#5` |
| `maj7#11` | Major seventh, sharp eleventh | 1 · 3 · 5 · 7 · #11 | `M7#11`, `maj7(#11)` | `Cmaj7#11` |
| `maj9#11` | Major ninth, sharp eleventh | 1 · 3 · 5 · 7 · 9 · #11 | `M9#11`, `maj9(#11)` | `Cmaj9#11` |
| `maj13#11` | Major thirteenth, sharp eleventh | 1 · 3 · 5 · 7 · 9 · #11 · 13 | `M13#11`, `maj13(#11)` | `Cmaj13#11` |
| `maj7add13` | Major seventh, added thirteenth | 1 · 3 · 5 · 7 · 13 | `maj7(add13)` | `Cmaj7add13` |

## Minor colors

| Suffix | Name | Degrees | Aliases | Example |
| --- | --- | --- | --- | --- |
| `m9b5` | Half-diminished ninth | 1 · b3 · b5 · b7 · 9 | `ø9`, `m9(b5)` | `Cm9b5` |
| `m11b5` | Half-diminished eleventh | 1 · b3 · b5 · b7 · 9 · 11 | `ø11`, `m11(b5)` | `Cm11b5` |
| `m7add11` | Minor seventh, added eleventh | 1 · b3 · 5 · b7 · 11 | `m7(add11)` | `Cm7add11` |
| `m7add13` | Minor seventh, added thirteenth | 1 · b3 · 5 · b7 · 13 | `m7(add13)` | `Cm7add13` |
