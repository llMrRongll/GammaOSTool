"""Run production removal in a temporary fake device filesystem, never real /data."""
from pathlib import Path
import subprocess, tempfile
source = (Path(__file__).parent/'device.sh').read_text()
with tempfile.TemporaryDirectory(prefix='gamma-remove-') as name:
    root = Path(name)
    for folder in ['data/adb/modules/gamma_perf_hud','data/adb/modules_update/gamma_perf_hud','data/adb/gamma-perf','data/adb/modules/other_module','data/adb/service.d','sys/class/devfreq/fde60000.gpu','system/bin','proc/bus/input','data/local/tmp']:
        (root/folder).mkdir(parents=True)
    for item in ['sys/class/devfreq/fde60000.gpu/cur_freq','system/bin/drastic-nano','data/adb/modules/other_module/keep','data/adb/service.d/99-gamma-perf-hud.sh']:
        (root/item).write_text('keep'); (root/item).chmod(0o755)
    (root/'proc/bus/input/devices').write_text('gt9xx-0')
    prefix = '''id() { echo 0; }
getprop() { case "$1" in ro.product.cpu.abi) echo arm64-v8a;; *) echo 1;; esac; }
magisk() { echo 27.0; }
pidof() { return 1; }
'''
    for path in ['/data/','/proc/','/sys/','/system/']:
        source = source.replace(path, name+path)
    script = root/'device.sh'; script.write_text(prefix+source)
    def run(mode, ok=True):
        p = subprocess.run(['sh',str(script),mode],text=True,capture_output=True)
        assert (p.returncode == 0) == ok, p.stdout+p.stderr
    # Stop failure must leave the disabled module intact for a safe retry.
    script.write_text(prefix+source.replace("  stop_panel || fail", "  false || fail"))
    run('remove',False)
    assert (root/'data/adb/modules/gamma_perf_hud/disable').exists()
    assert (root/'data/adb/modules_update/gamma_perf_hud/remove').exists()
    script.write_text(prefix+source)
    run('remove'); run('remove'); run('verify')
    for path in ['data/adb/modules/gamma_perf_hud','data/adb/modules_update/gamma_perf_hud','data/adb/gamma-perf','data/adb/service.d/99-gamma-perf-hud.sh']:
        assert not (root/path).exists(), path
    assert (root/'data/adb/modules/other_module/keep').read_text() == 'keep'
    print('PASS stop failure, installed/pending cleanup, repeat removal, removed-state verification, other module preservation')
