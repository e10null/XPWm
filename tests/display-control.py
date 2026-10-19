"""Real output protocol checks on private headless outputs; never host displays."""
import configparser
import os
from pathlib import Path
import subprocess
import sys
import traceback

root = Path(__file__).resolve().parents[1]
helper = root / 'shell/xpwm-displayctl'
runtime = Path(os.environ['XDG_RUNTIME_DIR'])


def outputs():
    config = configparser.ConfigParser(interpolation=None)
    config.read_string(subprocess.check_output([helper, '--list'], text=True))
    return {config[group]['name']: dict(config[group]) for group in config.sections() if group != 'outputs'}


def preview(name, width, height, rate, scale):
    process = subprocess.Popen([helper, '--preview', name, str(width), str(height), str(rate), str(scale)],
                               stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    line = process.stdout.readline()
    assert line == 'APPLIED\n', (line, process.stderr.read() if process.poll() is not None else '')
    return process


def finish(process, command):
    text, errors = process.communicate(command, timeout=20)
    assert process.returncode == 0, (text, errors)
    return text


try:
    original = outputs()
    assert len(original) == 2, original
    name = next(iter(original))
    neighbor = next(n for n in original if n != name)
    old = original[name]
    p = preview(name, 1600, 900, 60000, 1.25)
    current = outputs()
    assert current[name]['width'] == '1600'
    assert current[name]['refresh'] == '60000'
    assert float(current[name]['scale']) == 1.25
    assert current[neighbor] == original[neighbor], (current[neighbor], original[neighbor])
    assert 'REVERTED' in finish(p, 'revert\n')
    assert outputs() == original
    p = preview(name, 1280, 720, 59940, 1.5)
    assert 'KEPT' in finish(p, 'keep\n')
    saved_path = Path(os.environ['XDG_CONFIG_HOME']) / 'xpwm/displays.ini'
    saved = saved_path.read_text()
    assert outputs()[name]['refresh'] == '59940'
    # Restore previously confirmed settings after resetting the live output.
    p = preview(name, 1280, 720, 60000, 1)
    finish(p, 'keep\n')
    saved_path.write_text(saved)
    subprocess.run([helper, '--restore'], check=True)
    assert float(outputs()[name]['scale']) == 1.5
    assert outputs()[name]['refresh'] == '59940'
    # EOF models a GUI crash or closing Display Properties during confirmation.
    p = preview(name, 1600, 900, 60000, 1.75)
    assert 'REVERTED' in finish(p, None)
    assert float(outputs()[name]['scale']) == 1.5
    # Also model the entire GUI vanishing, including its stdout/stderr readers.
    p = preview(name, 1600, 900, 60000, 1.75)
    p.stdin.close()
    p.stdout.close()
    p.stderr.close()
    assert p.wait(timeout=8) == 0
    assert float(outputs()[name]['scale']) == 1.5
    p = preview(name, 1600, 900, 60000, 2)
    # Keep stdin open: the independent helper must enforce its own deadline.
    assert p.stdout.readline() == 'REVERTED\n'
    p.wait(timeout=5)
    assert float(outputs()[name]['scale']) == 1.5
    before_rejection = outputs()
    for args in [('1024', '768', '60000', '1'), ('1', '1', '60000', '1'), ('1280', '720', '59940', 'nan')]:
        result = subprocess.run([helper, '--preview', name, *args], input='', capture_output=True, text=True)
        assert result.returncode != 0 and result.stdout.startswith('ERROR '), result
    assert outputs() == before_rejection
    assert outputs()[neighbor] == original[neighbor]
    print('PASS: real resolution/refresh/scale, neighbor preservation, Keep, Revert, GUI EOF, 15-second timeout and saved restore', flush=True)
    subprocess.run([root / 'shell/display-settings-test'], check=True, timeout=12)
    assert outputs()[neighbor] == original[neighbor]
    assert float(outputs()[name]['scale']) == 1.25
    assert outputs()[name]['refresh'] == '59940'
    (runtime / 'display-passed').write_text('passed')
except Exception:
    traceback.print_exc()
    (runtime / 'display-failed').write_text('failed')
    sys.exit(1)
