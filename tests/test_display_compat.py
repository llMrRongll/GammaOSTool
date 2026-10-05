"""Verify DRM ID changes and FPS ownership against old/new driver states."""
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parents[1]
source = (root / "perf_hud.c").read_text()
helper = source[source.index("static int lower_display("):source.index("static int commit(")]
fps = (root / "hud_fps.h").read_text()
fps = fps[fps.index("static int fps_game_plane("):fps.index("static void fps_poll(")]
preamble = '#include <stdio.h>\n#include <stdint.h>\n#include <string.h>\n#include <assert.h>\nstatic const char *state;\nstatic FILE *fixture(const char *p,const char *m) {(void)p;(void)m;FILE *f=tmpfile();fputs(state,f);rewind(f);return f;}\n#define fopen fixture\n'
fixtures = 'int main(void) {\nuint32_t p=0,c=0;\nstate="plane[266]: Esmart0-win0\\ncrtc[74]: video_port0\\n";\nassert(!lower_display(&p,&c)&&p==266&&c==74);\nstate="plane[267]: Esmart0-win0\\ncrtc[75]: video_port0\\n";\nassert(!lower_display(&p,&c)&&p==267&&c==75);\nstate="plane[267]: Esmart0-win0\\n";assert(lower_display(&p,&c));\nstate="plane[58]: Smart0-win0\\n crtc=video_port0\\n allocated by = drastic-nano\\nplane[82]: Smart1-win0\\n";assert(fps_game_plane());\nstate="plane[59]: Smart0-win0\\n crtc=(null)\\nplane[331]: Cluster0-win0\\n crtc=video_port0\\n allocated by = drastic-nano\\nplane[363]: Cluster1-win0\\n";assert(fps_game_plane());\nstate="plane[331]: Cluster0-win0\\n crtc=video_port0\\n allocated by = binder:306_2\\n";assert(!fps_game_plane());\nstate="plane[331]: Cluster0-win0\\n crtc=video_port1\\n allocated by = drastic-nano\\n";assert(!fps_game_plane());\nputs("old/new IDs and FPS ownership fixtures passed");}\n'
with tempfile.TemporaryDirectory(prefix="gamma-display-test-") as work:
    source_path = Path(work) / "test.c"
    binary = Path(work) / "test"
    source_path.write_text(preamble + helper + fps + fixtures)
    subprocess.run(["cc", "-Wall", "-Wextra", str(source_path), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
