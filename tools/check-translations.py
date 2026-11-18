#!/usr/bin/env python3
"""Compares assets/translations/en.json with the strings the source shows.

Reports English keys no source file uses and shown strings missing from en.json.
--fix removes the unused keys and adds the missing strings, keeping the file's order.
"""
import argparse, collections, json, pathlib, re, sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
EN = ROOT / 'assets/translations/en.json'
# Calls whose first string argument is shown to the player.
SHOWN = re.compile(r'\b(?:tr|trNamed|translatedLabel|UI::(?:Button|Checkbox|MenuItem|CollapsingHeader|RadioButton|SliderFloat|SetTooltip|TextWrapped|TextDisabled|Text|InputTextWithHint)|label\(tr|rowLabel|heading|flag|number|choice|padBinding|devicePreset)\(\s*"((?:[^"\\]|\\.)*)"')
COMBO = re.compile(r'UI::Combo\(\s*"(?:[^"\\]|\\.)*"\s*,[^,]+,\s*((?:"(?:[^"\\]|\\.)*"\s*)+)')
LITERAL = re.compile(r'"((?:[^"\\]|\\.)*)"')

def unescape(text):
    return bytes(text, 'utf-8').decode('unicode_escape').encode('latin-1').decode('utf-8')

def clean(text):
    text = unescape(text)
    return text.split('##')[0]

def sources():
    return [p.read_text(encoding='utf-8') for p in sorted((ROOT / 'src').glob('*.[ch]*'))]

def shown_strings(texts):
    found = []
    for text in texts:
        for match in SHOWN.finditer(text):
            found.append(clean(match.group(1)))
        for match in COMBO.finditer(text):
            joined = ''.join(unescape(m) for m in LITERAL.findall(match.group(1)))
            found.extend(part for part in joined.split('\0') if part)
    return [s for s in dict.fromkeys(found) if s.strip() and not s.startswith('%') or s.startswith('%s')]

def literals(texts):
    found = set()
    for text in texts:
        for match in LITERAL.finditer(text):
            value = unescape(match.group(1))
            found.add(value.split('##')[0])
            found.update(value.split('\0'))
    return found

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fix', action='store_true')
    args = parser.parse_args()
    texts = sources()
    pack = json.loads(EN.read_text(encoding='utf-8'), object_pairs_hook=collections.OrderedDict)
    strings = pack['strings']
    every = literals(texts)
    # Smart potion names are composed in SmartConsumables.h and translated whole.
    every.update(f'{r} {e} · {p}' for r in ('Health','Magicka','Stamina') for e in ('restoration','regeneration') for p in ('lowest','highest','best fit'))
    joined = '\n'.join(texts)
    # A key counts as used when a literal holds it, or when a literal ends a phrase the code composes it from.
    unused = [k for k in strings if k not in every and not any(len(p) > 3 and p in k and k.replace(p, '').strip(' ·—') in every for p in every if p)]
    translated = set()
    for text in texts:
        translated.update(clean(m) for m in re.findall(r'\btr(?:Named)?\(\s*"((?:[^"\\]|\\.)*)"', text))
    # Words that only start lowercase when they are data keys, such as JSON field names, are left out unless tr() shows them.
    missing = [s for s in shown_strings(texts) if s not in strings and re.search('[A-Za-z]', re.sub(r'%[-+ #0-9.]*[a-zA-Z]', '', s)) and (s in translated or not re.fullmatch(r'[a-z][A-Za-z]*', s))]
    for key in unused: print('unused:', key.replace('\n', '\\n'))
    for key in missing: print('missing:', key.replace('\n', '\\n'))
    if args.fix:
        for key in unused: del strings[key]
        pack['strings'] = collections.OrderedDict([(k, k) for k in missing] + list(strings.items()))
        EN.write_text(json.dumps(pack, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    return 1 if (unused or missing) and not args.fix else 0

if __name__ == '__main__':
    sys.exit(main())
