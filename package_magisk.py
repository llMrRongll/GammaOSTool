"""Build an app-installable Magisk ZIP from current local binaries."""
from pathlib import Path
import zipfile, hashlib

root = Path(__file__).resolve().parent
output = root / "dist" / "GammaOS-RGDS-Performance-HUD-v1.0.12.zip"
output.parent.mkdir(exist_ok=True)
with zipfile.ZipFile(output, "w", zipfile.ZIP_DEFLATED) as archive:
    for path in sorted((root / "magisk-module").rglob("*")):
        if path.is_file():
            archive.write(path, path.relative_to(root / "magisk-module"))
    archive.writestr("skip_mount", "")
    for name in ("perf_hud", "perf_watch"):
        data = (root / name).read_bytes()
        if data[:4] != b"\x7fELF" or int.from_bytes(data[18:20], "little") != 183:
            raise RuntimeError(f"{name} must be an aarch64 ELF")
        archive.writestr("bin/" + name, data)
    for name in ("LICENSE", "NOTICE", "DISCLAIMER.md", "THIRD_PARTY_NOTICES.md"):
        archive.write(root / name, name)
    for path in sorted((root / "licenses").glob("*.txt")):
        archive.write(path, "licenses/" + path.name)
with zipfile.ZipFile(output) as archive:
    assert archive.testzip() is None
digest = hashlib.sha256(output.read_bytes()).hexdigest()
output.with_suffix(".zip.sha256").write_text(digest + "  " + output.name + "\n")
print(output)
print(digest)
