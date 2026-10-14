# Generating the byte-for-byte audio (BAG/ADPCM) oracle

The oracle test (`tests/test_audio.cpp`, case "audio bag: real clips match
XCC oracle byte-for-byte") compares `ra2assets`' PCM output against ground
truth decoded by **XCC's own IMA-ADPCM code**. The oracle set lives in the
git-ignored `oracle/audio/` directory and is **never committed** (derived
from EA's game assets). Same Task-9 pattern as `make_oracle.md`.

> Extraction vs decode: the idx/bag bytes are extracted by **ra2assets**
> (`ra2extract --raw`); the decode of clip bytes into PCM is done by **XCC's
> code**. The byte-for-byte gate therefore validates *our decoder* against
> XCC's, holding extraction constant.

## 1. Extract the real idx + bag ONCE with ra2assets

Each `ra2extract` invocation re-reads `language.mix` (~5 s) — batch these,
never per-clip:

```powershell
Set-Location D:\ra2-android
New-Item -ItemType Directory -Force D:\ra2-re\oracle-tool\staging | Out-Null
& .\build\ra2extract.exe "D:\Red Alert 2\language.mix" --raw audio.mix/audio.idx D:\ra2-re\oracle-tool\staging\audio.idx
& .\build\ra2extract.exe "D:\Red Alert 2\language.mix" --raw audio.mix/audio.bag D:\ra2-re\oracle-tool\staging\audio.bag
```

Expected: `audio.idx (41520 bytes)`, `audio.bag` ~17.6 MB.

## 2. Build the standalone oracle tool (OUTSIDE this repo — XCC is GPLv3)

`D:\ra2-re\oracle-tool\xcc_adpcm_dump.cpp` embeds, verbatim and with
provenance comments, from `D:\ra2-re\reference\xcc`:
`t_audio_idx_header`/`t_audio_idx_entry` (`misc/cc_structures.h`),
`t_ima_adpcm_chunk_header` (`misc/riff_structures.h`), and the step/index
tables + `aud_decode_ima_chunk` (`misc/aud_decode.cpp`); its dispatch and
chunk walk mirror `Cxse::extract` (`misc/xse.cpp`) and
`Cima_adpcm_wav_decode::load` (`misc/ima_adpcm_wav_decode.cpp`).

```powershell
Set-Location D:\ra2-re\oracle-tool
g++ -std=c++17 -O2 -Wall -static -static-libgcc -static-libstdc++ xcc_adpcm_dump.cpp -o xcc_adpcm_dump.exe
```

## 3. Dump the oracle PCM with XCC's decoder

```powershell
& .\xcc_adpcm_dump.exe staging\audio.idx staging\audio.bag list          # inspect entries
New-Item -ItemType Directory -Force D:\ra2-android\oracle\audio | Out-Null
& .\xcc_adpcm_dump.exe staging\audio.idx staging\audio.bag ichratta D:\ra2-android\oracle\audio\ichratta.pcm
& .\xcc_adpcm_dump.exe staging\audio.idx staging\audio.bag gradio3f D:\ra2-android\oracle\audio\gradio3f.pcm
```

Expected: `ichratta` → 141,323 samples (282,646 bytes); `gradio3f` → 1,017
samples (2,034 bytes). `<name>.pcm` files are raw little-endian int16 mono
PCM, no header.

## 4. Write the manifest

`oracle/audio/manifest.txt` — one clip per line, whitespace-separated
`<name> <sample_rate> <flags> <chunk_size>`:

```
ichratta 22050 12 512
gradio3f 22050 12 512
```

The test skips cleanly (doctest MESSAGE) when the manifest or
`language.mix` is absent, and fails if a present manifest exercises zero
clips (`clips_checked > 0` guard).
