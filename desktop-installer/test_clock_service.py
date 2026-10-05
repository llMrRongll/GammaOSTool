"""Verify startup reload guards without contacting a device."""
from pathlib import Path
import tempfile, subprocess, os
source=(Path(__file__).resolve().parent.parent/'fixes/xmb-bottom-clock/service.sh').read_text()
with tempfile.TemporaryDirectory() as name:
 root=Path(name); script=root/'service.sh'
 prefix='''getprop() { case "$1" in sys.boot_completed|sys.gammaos.nano.menu_active) echo 1;; init.svc.gammaos-nano-overlay) echo stopped;; esac; }
pidof() { if [ "$1" = gammaos-nano ]; then echo 123; else [ "$TEST_GAME" = yes ] && echo 456; fi; }
sha256sum() { case "$1" in /proc/*) echo "$TEST_RUNNING  exe";; *) echo '1c6064b64180352e65da92659fbe6f3fa9e149f74e1477f8011e124ad9d01a10  file';; esac; }
setprop() { echo "$*" > "$TEST_EVENT"; }
log() { :; }
sleep() { :; }
'''
 script.write_text(prefix+source.replace('900','2'))
 def run(running,game='no'):
  event=root/'event';event.unlink(missing_ok=True)
  subprocess.run(['sh',str(script)],env={**os.environ,'TEST_RUNNING':running,'TEST_GAME':game,'TEST_EVENT':str(event)},check=True)
  return event.exists()
 assert run('original')
 assert not run('1c6064b64180352e65da92659fbe6f3fa9e149f74e1477f8011e124ad9d01a10')
 assert not run('original','yes')
 (root/'disable').touch(); assert not run('original')
 print('PASS startup reload, already patched no-op, game and disabled guards')
