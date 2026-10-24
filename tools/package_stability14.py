"""Package the locally tested native APK and preserve matching crash symbols."""
from pathlib import Path
import hashlib, json, re, shutil, subprocess, zipfile

root = Path(__file__).resolve().parents[1]
out = root.parents[1] / "deliverables" / "RA2-native-stability-14"
out.mkdir(parents=True, exist_ok=True)
assert "BUILD SUCCESSFUL" in (root / "build-tools/android-stability-final.log").read_text(encoding="utf-8-sig")
metadata = json.loads((root / "android/app/build/outputs/apk/release/output-metadata.json").read_text())
assert metadata["elements"][0]["versionCode"] == 900014
apk = out / "RA2AndroidPort-stability-14-arm64.apk"
shutil.copy2(root / "android/app/build/outputs/apk/release/app-release.apk", apk)
with zipfile.ZipFile(apk) as archive:
    retail = [n for n in archive.namelist() if n.lower().endswith((".mix", ".csf", ".shp", ".vxl", ".hva", ".bag", ".idx", ".bik"))]
    assert not retail, retail
    packed = out / "packaged-main.so"
    packed.write_bytes(archive.read("lib/arm64-v8a/libmain.so"))
readelf = next((root / "build-tools/android-sdk/ndk").glob("*/toolchains/llvm/prebuilt/windows-x86_64/bin/llvm-readelf.exe"))
def elf(path, flag):
    return subprocess.check_output([str(readelf), flag, str(path)], text=True)
def build_id(path):
    return re.search(r"Build ID: ([0-9a-f]+)", elf(path, "-n")).group(1)
expected = build_id(packed)
candidates = sorted((root / "android/app/build/intermediates/cxx/Release").rglob("libmain.so"), key=lambda p:p.stat().st_mtime, reverse=True)
symbols = next(p for p in candidates if build_id(p) == expected and ".debug_line" in elf(p,"-S"))
manifest = dict(package=metadata["applicationId"], version_code=900014,
    version_name=metadata["elements"][0]["versionName"],
    base_commit=subprocess.check_output(["git","-C",str(root),"rev-parse","HEAD"],text=True).strip(),
    working_tree="uncommitted patch", apk=apk.name, bytes=apk.stat().st_size,
    sha256=hashlib.sha256(apk.read_bytes()).hexdigest(), native_build_id=expected,
    bundled_retail_assets=retail, target_phone_tested=False)
(out / "BUILD.json").write_text(json.dumps(manifest,indent=2)+"\n",encoding="utf-8")
with zipfile.ZipFile(out / "NATIVE_SYMBOLS.zip","w",zipfile.ZIP_DEFLATED) as archive:
    archive.write(symbols,"arm64-v8a/libmain.so")
    archive.writestr("BUILD.json",json.dumps(manifest,indent=2))
for src, name in [("build-tools/stability-available-tests.txt","native-tests.txt"),
                  ("build-tools/render-stability-result.txt","pixel-tests.txt"),
                  ("build-tools/android-stability-final.log","android-build.txt"),
                  ("android/app/build/reports/lint-results-release.txt","lint.txt"),
                  ("docs/ANDROID_STABILITY_14.md","RELEASE_NOTES.md")]:
    shutil.copy2(root / src,out / name)
print(json.dumps(manifest,indent=2))
