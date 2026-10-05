"""Exercise the production Mac engine with a fake ADB; never touches a device."""
from pathlib import Path
import hashlib, subprocess, tempfile
root = Path(__file__).resolve().parent
with tempfile.TemporaryDirectory(prefix="gamma-installer-test-") as name:
    target = Path(name)
    adb = target / "adb"
    adb.write_text('#!/bin/sh\nif [ "$1" = devices ]; then\n cat "${0%/*}/devices.txt"\n exit 0\nfi\nif [ "$3" = push ]; then exit 0; fi\nif [ "$3" = shell ] && [ "$4" = id ]; then echo 0; exit 0; fi\nif [ -e "${0%/*}/fail" ]; then echo "simulated installation rejection"; exit 1; fi\necho OK\n')
    adb.chmod(0o755)
    (target/"module.zip").write_bytes(b"test module")
    (target/"module.sha256").write_text(hashlib.sha256(b"test module").hexdigest())
    (target/"clock.zip").write_bytes(b"test clock")
    (target/"clock.sha256").write_text(hashlib.sha256(b"test clock").hexdigest())
    (target/"device.sh").write_text("test")
    source = (root/"Installer.swift").read_text().split("final class App:")[0]
    source += 'let r = URL(fileURLWithPath: "/tmp/gamma-installer-test")\nlet e = Engine(r)\nfunc rows(_ value: String) throws { try value.write(to: r.appendingPathComponent("devices.txt"), atomically: true, encoding: .utf8) }\nfunc rejected(_ label: String, _ action: () throws -> Void) {\n    do { try action(); fatalError("Unexpected success: " + label) } catch { print("PASS " + label) }\n}\ntry rows("List of devices attached\\n")\nrejected("no device") { _ = try e.device() }\ntry rows("abc unauthorized\\n")\nrejected("authorization required") { _ = try e.device() }\ntry rows("abc device\\ndef device\\n")\nrejected("multiple devices") { _ = try e.device() }\ntry rows("192.168.1.2:5555 device\\n")\nrejected("reject network transport") { _ = try e.device() }\ntry rows("abc device\\n")\nlet serial = try e.device(); assert(serial == "abc")\ntry e.perform("install"); print("PASS install routing")\ntry e.perform("clock-install"); print("PASS clock install routing")\ntry e.perform("clock-verify"); print("PASS clock verify routing")\ntry e.perform("clock-remove"); print("PASS clock remove routing")\ntry e.perform("remove"); print("PASS removal routing")\ntry Data().write(to:r.appendingPathComponent("fail"))\nrejected("ADB failure propagation") { try e.perform("install") }\ntry FileManager.default.removeItem(at:r.appendingPathComponent("fail"))\ntry "bad hash".write(to:r.appendingPathComponent("module.sha256"),atomically:true,encoding:.utf8)\nrejected("corrupt archive") { try e.perform("install") }\ntry "bad hash".write(to:r.appendingPathComponent("clock.sha256"),atomically:true,encoding:.utf8)\nrejected("corrupt clock archive") { try e.perform("clock-install") }\n'.replace("/tmp/gamma-installer-test", name)
    swift = target/"test.swift"; swift.write_text(source)
    binary = target/"test"
    subprocess.run(["xcrun","swiftc","-module-cache-path","/tmp/gamma-swift-cache","-swift-version","5",str(swift),"-o",str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
