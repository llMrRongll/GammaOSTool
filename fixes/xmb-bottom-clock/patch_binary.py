from pathlib import Path
import hashlib, struct, json
root=Path(__file__).parent
src=root/'gammaos-nano.original'
b=bytearray(src.read_bytes())
assert hashlib.sha256(b).hexdigest() == '5dda3100f902dce69f7912814b4c1086b597a8006fc3251a7d7c74b3132bb4c2', 'Unsupported original menu version'
site=0x24f1bc
cave=0x34d398
assert struct.unpack_from('<I',b,site)[0]==0xfd417900
assert b[cave:cave+8]==bytes(8)
assert struct.unpack_from('<ff',b,0x5d2f0)==(struct.unpack('<f',struct.pack('<f',1/288))[0],struct.unpack('<f',struct.pack('<f',1/272))[0])
delta=cave-site
assert delta%4==0 and -(1<<20)<=delta<(1<<20)
struct.pack_into('<I',b,site,0x5c000000|(((delta//4)&0x7ffff)<<5))
struct.pack_into('<ff',b,cave,1/282,1/282)
out=root/'gammaos-nano.patched';out.write_bytes(b);out.chmod(0o755)
manifest={'original_sha256':hashlib.sha256(src.read_bytes()).hexdigest(),'patched_sha256':hashlib.sha256(b).hexdigest(),'instruction_offset':hex(site),'constant_offset':hex(cave),'description':'Redirect only secondary clock scale load to dedicated constants in loaded .text alignment padding; shared constants untouched.'}
(root/'binary-patch.json').write_text(json.dumps(manifest,indent=2)+'\n')
print(json.dumps(manifest,indent=2))
