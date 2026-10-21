"""Verify a signed ARM64 sideload APK before publication (Android SDK required)."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import zipfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('apk', type=Path)
parser.add_argument('--build-tools', required=True, type=Path)
parser.add_argument('--report', required=True, type=Path)
args = parser.parse_args()
def run(tool, *options):
    result = subprocess.run([str(args.build_tools / tool), *map(str, options)], capture_output=True, text=True)
    if result.returncode:
        raise SystemExit(result.stdout + result.stderr)
    return result.stdout + result.stderr
badging = run('aapt.exe', 'dump', 'badging', args.apk)
manifest = run('aapt.exe', 'dump', 'xmltree', args.apk, 'AndroidManifest.xml')
# Binary XML boolean values are rendered as hexadecimal; absence defaults false.
for flag in ('testOnly', 'debuggable'):
    match = re.search(r'android:' + flag + r'\([^\n]*\)=(?:\(type[^)]*\))?(0x[0-9a-fA-F]+)', manifest)
    if match and int(match.group(1), 16):
        raise SystemExit('Refusing publication: android:' + flag + '=true')
if "package: name='org.ra2.engine'" not in badging:
    raise SystemExit('Unexpected application ID')
if "native-code: 'arm64-v8a'" not in badging:
    raise SystemExit('Expected ARM64 APK')
signature = run('apksigner.bat', 'verify', '--verbose', '--print-certs', args.apk)
run('zipalign.exe', '-c', '4', args.apk)
with zipfile.ZipFile(args.apk) as archive:
    if archive.testzip():
        raise SystemExit('Corrupt ZIP')
    for name in ('assets/LICENSE', 'assets/THIRD_PARTY_NOTICES.md'):
        archive.getinfo(name)
report = dict(apk=args.apk.name, bytes=args.apk.stat().st_size,
              sha256=hashlib.sha256(args.apk.read_bytes()).hexdigest(),
              badging=badging, test_only=False, debuggable=False,
              signature=signature, zip_alignment='passed', zip_crc='passed',
              device_testing='Not run')
args.report.write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report, indent=2))
