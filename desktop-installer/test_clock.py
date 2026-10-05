"""Exercise clock compatibility and removal gates in an isolated device filesystem."""
from pathlib import Path
import subprocess, tempfile
source = (Path(__file__).parent/'device.sh').read_text()
with tempfile.TemporaryDirectory(prefix='gamma-clock-test-') as name:
    root = Path(name)
    for folder in ['data/adb/modules/gamma_perf_hud','data/adb/modules/other_module','sys/class/devfreq/fde60000.gpu','system/bin','proc/bus/input','data/local/tmp']:
        (root/folder).mkdir(parents=True)
    for item in ['sys/class/devfreq/fde60000.gpu/cur_freq','system/bin/drastic-nano','data/adb/modules/other_module/keep']:
        (root/item).write_text('keep'); (root/item).chmod(0o755)
    (root/'proc/bus/input/devices').write_text('gt9xx-0')
    (root/'data/local/tmp/gamma-desktop-live.sh').write_text('exit 0\n')
    prefix = '''id() { echo 0; }
getprop() { case "$1" in ro.product.cpu.abi) echo arm64-v8a;; *) echo 1;; esac; }
magisk() { echo 27.0; }
pidof() { [ "$1" = gammaos-nano ] && echo 123; }
sha256sum() { echo "$TEST_HASH  $1"; }
'''
    for path in ['/data/','/proc/','/sys/','/system/']:
        source=source.replace(path,name+path)
    script=root/'device.sh'; script.write_text(prefix+source)
    def run(mode, digest='', ok=True):
        import os
        p=subprocess.run(['sh',str(script),mode],env={**os.environ,'TEST_HASH':digest},capture_output=True,text=True)
        assert (p.returncode==0)==ok,p.stdout+p.stderr
        return p.stdout
    assert '未安装' in run('clock-verify')
    assert '不兼容' in run('clock-install','unsupported',False)
    for digest in ['5dda3100f902dce69f7912814b4c1086b597a8006fc3251a7d7c74b3132bb4c2','1c6064b64180352e65da92659fbe6f3fa9e149f74e1477f8011e124ad9d01a10']:
        assert '兼容' in run('check',digest)
    mod=root/'data/adb/modules/gamma_xmb_clock_fix'; mod.mkdir()
    assert '已生效' in run('clock-verify','1c6064b64180352e65da92659fbe6f3fa9e149f74e1477f8011e124ad9d01a10')
    run('clock-verify','unsupported',False)
    pending=root/'data/adb/modules_update/gamma_xmb_clock_fix';pending.mkdir(parents=True)
    assert '待重启' in run('clock-verify')
    run('clock-remove'); run('clock-remove')
    for p in [mod,pending]:
        assert (p/'disable').exists() and (p/'remove').exists()
    assert (root/'data/adb/modules/gamma_perf_hud').is_dir()
    assert (root/'data/adb/modules/other_module/keep').read_text()=='keep'
    print('PASS clock version gate, loaded hash, pending state, repeat removal, HUD and other modules preserved')
