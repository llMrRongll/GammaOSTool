"""Exercise immediate install/remove and gameplay deferral using mocked system commands."""
from pathlib import Path
import tempfile, subprocess, os
source=(Path(__file__).parent/'live.sh').read_text()
with tempfile.TemporaryDirectory() as name:
 root=Path(name)
 for d in ['data/adb/modules','data/adb/modules_update/gamma_xmb_clock_fix/system/bin','data/local/tmp','system/bin','proc']:
  (root/d).mkdir(parents=True)
 (root/'proc/self').mkdir()
 (root/'proc/self/mountinfo').write_text('')
 (root/'data/local/tmp/gamma-clock-original').write_text('original')
 prefix='''getprop() { case "$1" in sys.boot_completed) echo 1;; sys.gammaos.nano.menu_active) echo "$TEST_MENU";; esac; }
pidof() { [ "$1" = gammaos-nano ] && echo 123; }
sha256sum() { case "$1" in */original|*/gamma-clock-original) echo '5dda3100f902dce69f7912814b4c1086b597a8006fc3251a7d7c74b3132bb4c2 file';; */system/bin/gammaos-nano) if [ -e "$TEST_MOUNT" ]; then cat "$TEST_MOUNT"; else echo '5dda3100f902dce69f7912814b4c1086b597a8006fc3251a7d7c74b3132bb4c2 file'; fi;; */proc/*/exe) cat "$TEST_MOUNT";; esac; }
mount() { echo '1c6064b64180352e65da92659fbe6f3fa9e149f74e1477f8011e124ad9d01a10 file' > "$TEST_MOUNT"; }
setprop() { if [ "$TEST_ACTION" = clock-remove ]; then echo '5dda3100f902dce69f7912814b4c1086b597a8006fc3251a7d7c74b3132bb4c2 file' > "$TEST_MOUNT"; fi; }
chcon() { :; }
sleep() { :; }
nohup() { echo queued > "$TEST_QUEUE"; }
'''
 for x in ['/data/','/system/','/proc/']: source=source.replace(x,name+x)
 script=root/'live.sh';script.write_text(prefix+source)
 def run(action,menu='1'):
  p=subprocess.run(['sh',str(script),action],env={**os.environ,'TEST_MENU':menu,'TEST_ACTION':action,'TEST_MOUNT':str(root/'mount'),'TEST_QUEUE':str(root/'queue')},capture_output=True,text=True)
  assert p.returncode==0,p.stdout+p.stderr
  return p.stdout
 pending=root/'data/adb/modules_update/gamma_xmb_clock_fix'
 (pending/'module.prop').write_text('test')
 (pending/'system/bin/gammaos-nano').write_text('patched')
 # Package hash for pending binary is distinct from the underlying system binary.
 s=script.read_text().replace('sha256sum() { case', "sha256sum() { case").replace('*/system/bin/gammaos-nano) if', "*/modules_update/*/gammaos-nano) echo '1c6064b64180352e65da92659fbe6f3fa9e149f74e1477f8011e124ad9d01a10 file';; */system/bin/gammaos-nano) if")
 script.write_text(s)
 assert '自动生效' in run('clock-apply')
 (root/'mount').unlink()
 assert '自动恢复' in run('clock-remove')
 assert not (root/'data/adb/modules/gamma_xmb_clock_fix').exists()
 assert '退出游戏' in run('clock-apply','0')
 assert (root/'queue').exists()
 print('PASS immediate activation, process validation, restore, gameplay deferral')
