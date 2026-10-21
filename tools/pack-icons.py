#!/usr/bin/env python3
"""Pack the complete set of supplied white silhouettes into the HUD atlas."""
import argparse
import hashlib
import io
import json
from pathlib import Path
import re

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
ICONS = ROOT / 'assets/icons'


def digest(data):
    return hashlib.sha256(data).hexdigest()


def silhouette(path):
    with Image.open(path) as original:
        image = original.convert('RGBA')
    alpha = image.getchannel('A')
    mode = 'source alpha'
    if alpha.getextrema() == (255, 255):
        # This supplied scroll has a baked gray checkerboard behind white art.
        # Extract its white mask without regenerating or changing the drawing.
        if path.name != 'scroll.png':
            raise ValueError(f'Opaque image needs an explicit mask conversion: {path}')
        alpha = image.convert('L').point(lambda v: max(0, min(255, round((v - 160) * 255 / 85))))
        mode = 'white mask from baked checkerboard (160..245)'
    bounds = alpha.getbbox()
    if not bounds:
        raise ValueError(f'Empty image: {path}')
    return alpha.crop(bounds), {'sourceSize': list(image.size), 'sourceBounds': list(bounds), 'alpha': mode}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=ICONS / 'source')
    parser.add_argument('--check', action='store_true', help='Verify generated files without changing them')
    parser.add_argument('--preview', type=Path, help='Write a labeled contact sheet')
    args = parser.parse_args()
    catalog = json.loads((ICONS / 'catalog.json').read_text(encoding='utf-8'))
    columns, rows, cell, padding = (catalog[k] for k in ('columns', 'rows', 'cellSize', 'padding'))
    entries = catalog['icons']
    assert catalog['schema'] == 1 and columns * rows >= len(entries)
    assert len({e['id'] for e in entries}) == len(entries)
    assert all(re.fullmatch(r'[a-z][a-z0-9_]*', e['id']) for e in entries)
    assert all('alias' in e or (Path(e['file']).name == e['file'] and e['file'].endswith('.png')) for e in entries)
    known = {e['file'] for e in entries if 'file' in e}
    unknown = {p.name for p in args.source.glob('*.png')} - known
    if unknown:
        raise ValueError(f'Add these files to catalog.json before packing: {sorted(unknown)}')
    missing = known - {p.name for p in args.source.glob('*.png')}
    if missing:
        raise ValueError(f'Missing source icons: {sorted(missing)}')
    atlas = Image.new('RGBA', (columns * cell, rows * cell))
    manifest = {'schema': 2, 'atlas': 'items.png', 'cellSize': cell, 'columns': columns,
                'rows': rows, 'padding': padding, 'icons': []}
    tiles = []
    by_id = {}
    for index, entry in enumerate(entries):
        if 'alias' in entry:
            tile = by_id[entry['alias']].copy()
            atlas.paste(tile, (index % columns * cell, index // columns * cell))
            manifest['icons'].append({**entry, 'cell': index, 'kind': 'alias', 'packedBounds': list(tile.getbbox())})
            tiles.append(tile); by_id[entry['id']] = tile
            continue
        source = args.source / entry['file']
        mask, details = silhouette(source)
        ratio = min((cell - 2 * padding) / mask.width, (cell - 2 * padding) / mask.height)
        size = tuple(max(1, round(d * ratio)) for d in mask.size)
        mask = mask.resize(size, Image.Resampling.LANCZOS)
        tile = Image.new('RGBA', (cell, cell))
        art = Image.new('RGBA', size, (255, 255, 255, 255))
        art.putalpha(mask)
        offset = ((cell - size[0]) // 2, (cell - size[1]) // 2)
        tile.paste(art, offset)
        atlas.paste(tile, (index % columns * cell, index // columns * cell))
        record = {**entry, 'cell': index, 'kind': 'supplied',
                  'source': 'source/' + entry['file'],
                  'source_sha256': digest(source.read_bytes()), 'packedBounds': list(tile.getbbox()), **details}
        manifest['icons'].append(record)
        tiles.append(tile); by_id[entry['id']] = tile
    buffer = io.BytesIO()
    atlas.save(buffer, format='PNG', optimize=True)
    png = buffer.getvalue()
    manifest['atlas_sha256'] = digest(png)
    manifest['supplied'] = sum(e['kind'] == 'supplied' for e in manifest['icons'])
    manifest['aliases'] = sum(e['kind'] == 'alias' for e in manifest['icons'])
    manifest['placeholders'] = len(entries) - manifest['supplied'] - manifest['aliases']
    names = ',\n    '.join(', '.join(e['id'] for e in entries[i:i+8]) for i in range(0, len(entries), 8))
    header = f'''#pragma once
#include <cstdint>

namespace quickslot {{
enum class Icon : unsigned {{
    automatic,
    {names},
    count
}};
inline constexpr unsigned iconColumns={columns},iconRows={rows},iconCell={cell};
inline constexpr unsigned iconCount=unsigned(Icon::count)-1;
static_assert(iconCount<=iconColumns*iconRows);
inline constexpr char itemAtlasPath[]="Quickslot/icons/items.png";
}}
'''
    outputs = {ICONS / 'items.png': png,
               ICONS / 'manifest.json': (json.dumps(manifest, indent=2) + '\n').encode(),
               ROOT / 'src/Icons.h': header.encode()}
    for path, data in outputs.items():
        if args.check:
            if not path.exists() or path.read_bytes() != data:
                raise ValueError(f'Generated file differs: {path}')
        else:
            temp = path.with_suffix(path.suffix + '.tmp')
            temp.write_bytes(data)
            temp.replace(path)
    if args.preview:
        width, height = 176, 188
        sheet = Image.new('RGB', (columns * width, rows * height), '#202426')
        draw = ImageDraw.Draw(sheet)
        font = ImageFont.truetype(str(ROOT / 'assets/fonts/Jost-Regular.ttf'), 16)
        for index, (entry, tile) in enumerate(zip(manifest['icons'], tiles)):
            left, top = index % columns * width, index // columns * height
            art = tile.resize((144, 144), Image.Resampling.LANCZOS)
            sheet.paste(art, (left + 16, top + 4), art)
            label = entry['id']
            draw.text((left + width // 2, top + 154), label, font=font, fill='white', anchor='mt')
        args.preview.parent.mkdir(parents=True, exist_ok=True)
        sheet.save(args.preview)
    print(f"{'Verified' if args.check else 'Packed'} {len(entries)} cells: {manifest['supplied']} supplied icons, "
          f"{manifest['placeholders']} placeholders; {atlas.width} x {atlas.height} RGBA.")


if __name__ == '__main__':
    main()
