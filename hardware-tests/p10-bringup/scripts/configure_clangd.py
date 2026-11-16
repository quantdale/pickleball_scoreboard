"""Generate editor-only Xtensa metadata; never changes firmware build flags.

First run PlatformIO's `run -e dual -t compiledb` for this project, then run this
script with the existing firmware venv Python. Generated absolute paths remain
under ignored .pio/, not in versioned configuration. No third-party dependencies.
"""
import json
import re
import subprocess
from pathlib import Path


def main():
    project = Path(__file__).resolve().parents[1]
    try:
        entries = json.loads((project / 'compile_commands.json').read_text(encoding='utf-8'))
    except (OSError, ValueError) as exc:
        raise SystemExit('Generate compile_commands.json with pio run -e dual -t compiledb first') from exc
    entry = next((item for item in entries
                  if (Path(item['directory']) / item['file']).resolve() == project / 'src/main.cpp'), None)
    if entry is None:
        raise SystemExit('PlatformIO database has no entry for this project\'s src/main.cpp')
    command = entry['command']
    match = re.match(r'^(?:"([^"]+)"|(\S+))', command)
    if match is None:
        raise RuntimeError('Cannot locate the Xtensa compiler in the PlatformIO command')
    compiler = match.group(1) or match.group(2)
    probe = subprocess.run([compiler, '-E', '-x', 'c++', '-', '-v'], input='',
                           capture_output=True, text=True, check=True, cwd=project)
    target = re.search(r'^Target:\s*(\S+)', probe.stderr, re.M)
    include_block = re.search(r'#include <\.\.\.> search starts here:\n(.*?)End of search list\.',
                              probe.stderr, re.S)
    if target is None or include_block is None or not target.group(1).startswith('xtensa-'):
        raise RuntimeError('Compiler did not report an Xtensa target/system include search list')
    includes = [Path(line.strip()).resolve() for line in include_block.group(1).splitlines()
                if line.strip()]
    if not includes or not all(path.is_dir() for path in includes):
        raise RuntimeError('Reported compiler system include directories are missing')

    # These GCC-only options cause host clangd driver errors. Only the derived
    # editor database drops them; the real PlatformIO compile command is intact.
    command = re.sub(r'(?<!\S)-(mlongcalls|fstrict-volatile-bitfields|fno-tree-switch-conversion)(?=\s|$)',
                     '', command)
    additions = ['--target=' + target.group(1)]
    for path in includes:
        additions += ['-isystem', path.as_posix()]
    entry = dict(entry, command=command + ' ' + subprocess.list2cmdline(additions))
    output = project / '.pio/clangd/compile_commands.json'
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps([entry], indent=2) + '\n', encoding='utf-8')
    print(f'Generated editor-only metadata for {target.group(1)} with {len(includes)} system include paths')
    print(output)


if __name__ == '__main__':
    main()
