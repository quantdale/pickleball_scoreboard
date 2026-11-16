"""Run existing Unity suites without ESP32 hardware or replacing PlatformIO.

Windows uses installed MSVC (avoids the unrelated MinGW libisl failure).
All binaries/log intermediates go to an explicitly supplied external artifact dir.
"""
import argparse
import os
import re
import shutil
import subprocess
from pathlib import Path


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output-dir', type=Path, required=True)
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[1]
    output = args.output_dir.resolve()
    if output.is_relative_to(repo):
        parser.error('Use a session artifacts directory outside the repository')
    output.mkdir(parents=True, exist_ok=True)
    unity = repo / 'firmware/.pio/libdeps/native/Unity/src'
    if not (unity / 'unity.c').exists():
        parser.error('Resolve PlatformIO native Unity dependency first')
    env = ({key.upper(): value for key, value in os.environ.items()}
           if os.name == 'nt' else os.environ.copy())
    windows = os.name == 'nt'
    if windows:
        vswhere = Path(env.get('ProgramFiles(x86)', 'C:/Program Files (x86)')) / 'Microsoft Visual Studio/Installer/vswhere.exe'
        vs = subprocess.check_output([str(vswhere), '-latest', '-products', '*', '-requires',
                                      'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'], text=True).strip()
        vcvars = Path(vs) / 'VC/Auxiliary/Build/vcvars64.bat'
        setup = output / 'msvc-env.cmd'
        setup.write_text(f'@echo off\ncall "{vcvars}" >nul\nif errorlevel 1 exit /b 1\nset\n')
        lines = subprocess.check_output(['cmd.exe', '/d', '/c', str(setup)], text=True)
        for line in lines.splitlines():
            if '=' in line and not line.startswith('='):
                key, value = line.split('=', 1)
                env[key.upper()] = value
        compiler = shutil.which('cl.exe', path=env['PATH'])
        if not compiler:
            parser.error('MSVC compiler was not found after vcvars64')
        common = [compiler, '/nologo', '/EHsc', '/std:c++14', '/W3',
                  '/I' + str(repo / 'firmware/include'), '/I' + str(unity)]
        unity_object = output / 'unity.obj'
        subprocess.run([compiler, '/nologo', '/c', '/TC', str(unity / 'unity.c'), '/Fo' + str(unity_object)],
                       env=env, cwd=output, check=True)
    else:
        common = [env.get('CXX', 'c++'), '-std=c++11', '-I' + str(repo / 'firmware/include'), '-I' + str(unity)]
        unity_object = unity / 'unity.c'
    sources = [repo / ('firmware/src/' + name + '.cpp')
               for name in ['game_state', 'ble_command_parser', 'display_render_logic']]

    def build(source, name, with_unity):
        executable = output / (name + ('.exe' if windows else ''))
        command = common + [str(source)] + [str(path) for path in sources]
        if with_unity:
            command += [str(unity_object)]
        if windows:
            command += ['/Fe' + str(executable)]
        else:
            command += ['-o', str(executable)]
        subprocess.run(command, env=env, cwd=output, check=True)
        return executable

    total = 0
    for source in sorted((repo / 'firmware/test').glob('test_*/*.cpp')):
        executable = build(source, source.parent.name, True)
        run = subprocess.run([str(executable)], env=env, cwd=output, text=True, capture_output=True)
        print(run.stdout, end='', flush=True)
        print(run.stderr, end='', flush=True)
        result = re.search(r'(\d+) Tests (\d+) Failures', run.stdout)
        if run.returncode or not result or int(result.group(2)):
            raise SystemExit('Firmware test suite failed: ' + source.parent.name)
        total += int(result.group(1))
    print(f'PASS: {total} Unity tests')
    tool = repo / 'scripts/render_snapshots.cpp'
    if tool.exists():
        print('Snapshot exporter:', build(tool, 'render-snapshots', False))


if __name__ == '__main__':
    main()
