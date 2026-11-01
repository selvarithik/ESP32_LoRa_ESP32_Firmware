"""Create separate HTML/CSS/JS reference files without changing deployed pages."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil


def split_page(html, target):
    target.parent.mkdir(parents=True, exist_ok=True)
    original = html
    blocks = []
    def extract(match):
        tag, attrs, body = match.group(1), match.group(2), match.group(3)
        if tag == 'script' and (re.search(r'\bsrc\s*=', attrs) or re.search(r'type\s*=\s*[\"\']application/', attrs)):
            return match.group(0)
        suffix = 'css' if tag == 'style' else 'js'
        name = f'{target.stem}.{len(blocks)+1}.{suffix}'
        (target.parent / name).write_text(body, encoding='utf-8', newline='')
        replacement = (f'<link rel="stylesheet" href="{name}">' if tag == 'style'
                       else f'<script{attrs} src="{name}"></script>')
        blocks.append(dict(file=name, original=match.group(0), replacement=replacement))
        return replacement
    html = re.sub(r'<(style|script)([^>]*)>([\s\S]*?)</\1>', extract, html)
    target.write_text(html, encoding='utf-8', newline='')
    rebuilt = html
    for block in blocks:
        rebuilt = rebuilt.replace(block['replacement'], block['original'], 1)
    assert rebuilt == original, 'Source round-trip failed'
    return dict(html=target.name, blocks=[b['file'] for b in blocks], source_sha256=hashlib.sha256(original.encode()).hexdigest(), round_trip_verified=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--gateway', type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    output = args.output.resolve()
    if output.exists():
        parser.error('Use a new output directory')
    output.mkdir(parents=True)
    manifest = {'esp32': [], 'gateway': []}
    for filename, delimiter, page in [('local_web_server.cpp','HTML','dashboard'), ('mqtt_page.h','MQTT','mqtt'), ('access_page.h','ACCESS','access')]:
        source = (root / 'src/web' / filename).read_text(encoding='utf-8')
        html = re.search('R"'+delimiter+r'\(([\s\S]*?)\)'+delimiter+'"', source)[1]
        manifest['esp32'].append(split_page(html, output / 'ESP32' / f'{page}.html'))
    gateway = args.gateway.resolve()
    # Preserve original templates/assets as the runnable Flask source, too.
    shutil.copytree(gateway / 'dashboard', output / 'Gateway' / 'original-dashboard', ignore=shutil.ignore_patterns('__pycache__'))
    for path in sorted((gateway / 'dashboard/templates').rglob('*.html')):
        relative = path.relative_to(gateway / 'dashboard/templates')
        item = split_page(path.read_text(encoding='utf-8'), output / 'Gateway' / 'separate-blocks' / relative)
        item['template'] = relative.as_posix()
        manifest['gateway'].append(item)
    (output / 'manifest.json').write_text(json.dumps(manifest, indent=2), encoding='utf-8')
    (output / 'README.txt').write_text(
        'REFERENCE/EDITING EXPORT: no device or server settings changed.\n'
        'ESP32 HTML, CSS and JS are separated with exact reconstruction checks.\n'
        'Production sources remain src/web raw HTML literals; regenerate gzip using test/build_web_assets.py after editing production sources.\n'
        'Gateway/original-dashboard contains the actual Flask templates and shared static CSS/JS/fonts/images.\n'
        'Gateway/separate-blocks also extracts inline CSS/JS from every template for inspection.\n'
        'Jinja template inheritance, URLs and template expressions require Flask and their original rendering context.\n'
        'The reference exports are not replacements for production templates; opening file URLs cannot test authenticated APIs.\n', encoding='utf-8')
    print(f"PASS: {len(manifest['esp32'])} ESP32 pages and {len(manifest['gateway'])} Gateway templates; all inline-block round trips verified")


if __name__ == '__main__':
    main()
