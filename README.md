# Navalha 2 — JUCE/C++ migration

<p align="center">
  <img src="docs/site/assets/navalha2-header.png" alt="Navalha 2" width="560">
  <img src="docs/site/assets/navalha2-mascot.png" alt="Navalha 2 mascot" width="150">
</p>

<p align="center">
  <img src="docs/site/assets/navalha2-juce-interface.jpg" alt="Navalha 2 JUCE native interface: sources, patterns, gestures, form, library and mixer" width="100%">
</p>

Contact: **rasgo.instruments@gmail.com**

Languages:

- [English](#english)
- [Português](#português)
- [Français](#français)
- [Español](#español)

---

## English

Current version of the JUCE migration: **v0.1.0**. The **v0.28.1** version
mentioned throughout this documentation is the functional Pure Data/web
reference used to measure parity; it is not the JUCE application's own
version number.

This tree is parallel to, and does not replace, the v0.28.1 application.
Since the split into two sibling directories inside `RASGO/`, the Pure
Data/web runtime app lives in `../NAVALHA2_PD/` (it used to be `../` from
here, back when this folder was still the `juce/` subfolder of a single
project).

The current runtime still lives at:

- `../NAVALHA2_PD/app/`
- `../NAVALHA2_PD/bridge/`
- `../NAVALHA2_PD/core/`
- `../NAVALHA2_PD/run_navalha.sh`

### State of this first stage

- initial C++ model for sources, slices and mixer;
- fixed bank of 10 × 8 patterns with SOURCE A/B/GAP cells;
- GRID sequencer by samples, with STOP invalidating pending work;
- stereo A/B DSP mixer with pan and mid/side width equivalent to the Pure
  Data patch;
- 15 ms linear ramps in the mixer path, with no per-sample allocation;
- immutable stereo buffers during playback and reading with linear
  interpolation;
- forward/reverse player with an adaptive 0.5–5 ms envelope and STOP with
  fade;
- `AudioEngine` authority linking the sequencer, banks A/B, players and
  mixer;
- two alternating voices per source for retrigger with a crossfade tail;
- JUCE shell wired to a real stereo output callback via `AudioAppComponent`;
- fixed SPSC queue for UI → audio commands, with no locks, waiting or
  allocation;
- structural gestures sent through the same UI → audio queue;
- slice banks in fixed storage, with contiguous BLADE and non-destructive
  undo;
- offline in-memory rendering with peak, energy and checksum for golden
  WAVs;
- Linux golden regression with DSP and full PCM24 WAV signatures;
- thirty-second combined golden stress test with JITTER, Assisted, FORM,
  TRACE, pitch and two virtual voices;
- safety ceiling of ten million samples per test render;
- GRID, FREE and JITTER clocks by samples, with a reproducible time seed;
- deterministic and reversible MEMORY, MUTATION, EROSION and DECONSTRUCT;
- sample-accurate STUTTER ×4 and BURST ×8 plans;
- fixed realtime scheduler to run STUTTER/BURST without allocation;
- MICRO ×2–8 with fixed capacity and per-slice reverse in the realtime
  queue;
- native JUCE controls for MEMORY, transformations, STUTTER, BURST, MICRO
  and REVERSE;
- FORM Director with five default scenes, up to 16 scenes, limits, lock
  and navigation;
- interoperable Project v2 persistence of FORM and TRACE XY (up to 512
  points);
- sample-accurate TRACE LOOP applying BPM and Heritage Pitch in the
  callback;
- FORM advancing by phrase, switching LONG/MEDIUM/SHORT/MICRO banks and
  modulating the Assisted Performer's context;
- complete native FORM editor with name, transition, A/B profiles, six
  dimensions, lock, add/copy/delete/move and 64-step structural undo/redo;
- explicit capture of named A/B banks, recall by scene and optional
  interoperable persistence in Project v2;
- scene names and history in pre-allocated storage, with no allocation in
  operations executed by the callback;
- MASTER stage 0–1 after the mixer, with 15 ms smoothing shared between
  realtime/offline;
- `liveSafe` profile in the app: non-finite sanitation, DC blocking and a
  stereo limiter with 5 ms lookahead and a -1 dBTP ceiling engaged after
  summing the instrument with Library Preview;
- `legacy` profile preserved in the core for parity comparison; the
  liveSafe limiter already contains the EBU 19–23 cases synthesized from
  the spec, but the official WAVs are still needed before a general true
  peak conformance declaration; independent FFmpeg measurement already
  passed;
- `MASTER CREATIVE` stays in the instrument's own domain; the technical
  `OUTPUT TRIM` attenuates from -24 to 0 dB before the limiter and is
  persisted as a local preference;
- technical MUTE with a 10 ms ramp and a reconnect lifecycle with atomic
  suspension, silent resume and fade-in, without touching Project state;
- two virtual voices with source, division, pattern, focus, pitch,
  envelope, level and pan;
- automatic voice headroom compatible with the one/two-voice reference;
- Heritage Pitch with two heads, four-point `vd~`-style interpolation,
  cosine windows and a 5 Hz high-pass;
- 20 ms dry/processed crossfade compatible with the Pure Data stage;
- Project v2 snapshots and Project v1 migration, preserving deterministic
  state;
- lock-free post-MASTER stereo recording FIFO, with overflow accounted
  for;
- streaming WAV RIFF encoder for PCM 16, PCM 24 and float 32;
- deterministic TPDF dither by default on PCM16/PCM24, never on float32,
  with a configurable seed and final sanitation of non-finite samples;
- portable-pack path validation against traversal and absolute paths;
- WAV writer on a separate thread, with a stop handshake and FIFO
  draining;
- atomic REC publication: the temporary WAV only becomes final after a
  valid RIFF;
- persistent TAKE v1 catalog for newly finished WAVs;
- TAKE Timeline window with private metadata, review and pre-REC recipe;
- non-destructive return of a TAKE to SOURCE A/B;
- preventive REC limit by free space, one hour and a 4 GiB RIFF ceiling;
- minimum 1 GiB reserve on the destination volume during recording;
- in-memory WAV decoder for PCM 16/24 and float 32, mono or stereo;
- PCM24 decoder also compatible with `WAVE_FORMAT_EXTENSIBLE`;
- stereo min/max waveform cache with resolution capped at 8192 bins;
- MASTER metrics compatible with the v0.28.1 estimate, with a read limit;
- frame-based ALBUM MASTER planning for trims, fades and gaps;
- safe codec for the `navalha-album-master` v1 manifest and linear
  envelopes;
- initial C++ TRACK MASTER chain with EQ, dynamics, saturation, width and
  ceiling;
- codec for the `navalha-master-recipe` v1 recipe, compatible with the web
  interface;
- golden regression specific to detecting changes in the TRACK MASTER
  chain;
- objective WebAudio/C++ comparison over 353,708 frames within 0.25 dB;
- RIFF LIST/INFO metadata for title, artist, project, year and comment;
- optional later RIFF writing in the TAKE Timeline, with confirmation,
  validated partial, smart-link/copy backup and recoverable replacement;
- Assisted Performer Mulberry32 RNG identical to the JavaScript one, with
  seed/cursor;
- Assisted planner by phrase with timing, transformations, pitch and
  fragments;
- Assisted decisions for source, pattern, region, cuts and AUTO MIX;
- pattern recombination with reverse/interleave/mutation, MEMORY and safe
  GAP;
- automatic slice edits limited to nudge, micro, blade, undo and redivide;
- conservative AUTO MIX restricted to balance, pan and width;
- probabilities, limits and FORM/energy mapping equivalent to v0.28.1;
- realtime Assisted execution when each phrase closes, with no interface
  timers;
- native AUTO, vocabulary, BPM range, variation and seed/rewind controls;
- internal JSON codec with size/depth limits for Project v1/v2;
- JSON mapping compatible with sources, sequencer, DSP, timing and
  Assisted state;
- Project v2 also preserving Assisted settings/vocabulary;
- mixer automation interoperable with `dsp.sourceMixer.automation`;
- realtime telemetry of source, pattern, row, BPM, pitch and mixer for the
  shell;
- Project v2 preserving MEMORY, base and transformation intensities;
- portable pack ZIP store with CRC32, limits, deduplication and traversal
  protection;
- portable Navalha service restricted to project.navalha and SOURCE A/B
  audio;
- shell with LOAD A/B, waveform, PLAY/STOP, MASTER, project and WAV
  recording;
- native BPM/rate, pattern, GRID/FREE/JITTER and Heritage Pitch controls;
- editable JITTER intensity and reproducible seed in the shell;
- eight-step editor with A0–A127, B0–B127 and GAP codes;
- native A/B mixer with persistent level, pan, width, mute and solo;
- persistent global A/B balance, smoothed by the realtime mixer;
- native slice editor with SOURCE A/B, START/END, division, BLADE and
  undo;
- slice bounds and indices drawn over the waveform;
- controls for the two virtual voices' enable, source, division, pitch,
  level and pan;
- virtual voice detail with a 16-step pattern, length, focus and envelope;
- shell content in a scrollable viewport for smaller screens;
- UI → audio queue sync before capturing project snapshots;
- v2 audio references with filename, relativePath, size, date and MIME;
- safe reload of relative WAVs when opening lightweight `.navalha`
  projects;
- Linux guard to exit cleanly when no X11 display is reachable;
- stereo meters, device and recording all receive the same post-safety
  signal, including while Library Preview is active, plus recording
  frame/drop telemetry;
- meters show sample peak in dBFS; additional telemetry publishes
  per-block RMS, input peak, gain reduction, non-finites and ceiling
  engagement, with a two-second visual safety hold;
- polyphase 4× FIR true-peak detector in the live stream, initially
  validated by EBU Tech 3341 cases 15–23 (20–23 being derived fixtures);
  it drives a 5 ms-lookahead limiter, with no allocation in the callback,
  with latency explicitly published by the engine;
- live worst-sum test with Sources A/B at maximum, two virtual voices,
  master at 100% and simultaneous Library Preview, requiring finite output
  and a true-peak ceiling after the limiter;
- optional CTest cross-validation with FFmpeg's `ebur128`: input cases
  15–23 and post-limiter WAVs checked by an external implementation;
- limiter impulse and latency contracts at 44.1, 96 and 192 kHz, plus a
  clean stage reset after a sample-rate change;
- atomic PLAY/STOP, step and generation telemetry for the interface;
- realtime highlight of the next step with no concurrent SessionModel
  read;
- lock-free A/B playhead derived from the main players, with a cursor and
  current/duration readout on both waveforms, including in reverse;
- selectable PCM16, PCM24 or float32 recording with drop diagnostics;
- TAKE Timeline with recursive/deduplicated import of prior WAVs and
  reading of available RIFF metadata;
- private metadata preset derived from any take and applied only to future
  recordings;
- persistent ALBUM PROJECT with take selection, deduplication, editorial
  order, v1 export and direct render through ALBUM MASTER;
- ORIGINAL/MASTER TRACK MASTER comparison via aligned float32 temporaries,
  with compensation that only attenuates the louder side and locks during
  REC;
- relative ALBUM PROJECT matching to an estimated target, capped at ±6 dB,
  with persistent per-track analysis/trim;
- native JUCE panel for device, stereo output, buffer and sample rate;
- device configuration persisted in local app preferences;
- invariants compatible with Project v2;
- tests that need neither JUCE nor an audio device;
- standalone shell built with JUCE 8.0.13 on Linux;
- no JUCE copy vendored into the repository;
- no file from the current implementation removed or renamed.

### Quick test without CMake/JUCE

```sh
./test_core.sh
```

The script builds only the core with the system compiler, runs the
contracts and places the temporary binary outside the source tree.

The full CTest suite also verifies that the application refuses headless
execution cleanly, without a segmentation fault.

### Build with CMake

Requirements:

- CMake 3.22 or later;
- a C++20 compiler;
- JUCE configured externally.

```sh
cmake -S . -B build/juce -DCMAKE_BUILD_TYPE=Debug
cmake --build build/juce
ctest --test-dir build/juce --output-on-failure
```

With a local, non-installed JUCE checkout:

```sh
cmake -S . -B build/juce \
  -DNAVALHA_JUCE_PATH=/path/to/JUCE \
  -DNAVALHA_PD_PATH=/path/to/navalha2-pd \
  -DCMAKE_BUILD_TYPE=Debug
```

The WebView is off in the initial native shell to reduce dependencies:

```sh
cmake -S . -B build/juce \
  -DNAVALHA_JUCE_PATH=/path/to/JUCE \
  -DNAVALHA_PD_PATH=/path/to/navalha2-pd \
  -DNAVALHA_ENABLE_WEBVIEW=OFF
```

In this workspace, after preparing local dependencies, the two-process-
limited build and the tests can be repeated with:

```sh
./build_local.sh
```

`NAVALHA_JOBS` lets you change the parallelism limit explicitly.

#### Multiplatform packages (Linux/Windows/macOS)

Downloads: [**Navalha 2 JUCE v0.1.0** release](https://github.com/lucioaraujo/navalha2-juce/releases/tag/v0.1.0)
— Linux `.deb`, Windows `.exe` (NSIS) and macOS `.dmg` (DragNDrop), all
built and tested via CI on GitHub's own hosted runners; no Windows or
macOS machine is needed to build these.

To build a fresh set yourself, open **Actions** on GitHub and run the
**Package (Linux/Windows/macOS)** workflow (`.github/workflows/
package.yml`); it also runs automatically on any `v*` tag push. Each
platform's installer is uploaded as a workflow artifact, downloadable
from the run page.

The locally-generated Linux package reflects the build machine's own
library versions. For the most compatible internal `.deb`, use the same
CI workflow above (fixed to Ubuntu 22.04 and JUCE 8.0.13).

Instructions for whoever installs the artifact are in
[`docs/INSTALACAO_DEB_INTERNA.md`](docs/INSTALACAO_DEB_INTERNA.md).
Minimum requirements are in
[`docs/REQUISITOS_MINIMOS_LINUX.md`](docs/REQUISITOS_MINIMOS_LINUX.md).

### WAV comparison

The build also produces `navalha_compare_wav`, used to compare a Pure Data
reference render against the JUCE output. The files need the same sample
rate and frame count:

```sh
.local-build/juce-app-native/navalha_compare_wav \
  reference.wav juce-candidate.wav
```

The JSON output contains the reference RMS, the difference's RMS and
peak, correlation and SNR. Reading is capped at 512 MiB per file and does
not create disk copies. A known candidate latency can be compensated
without copying the audio:

```sh
.local-build/juce-app-native/navalha_compare_wav \
  reference.wav juce-candidate.wav --candidate-offset 480
```

A positive offset skips leading candidate frames; negative skips leading
reference frames.

### MASTER analysis

> The analysis and TRACK MASTER below preserve the historical migration
> flow. They do not yet constitute the common safe-output layer defined
> for the RASGO instruments. See `docs/AUDITORIA_ENGENHARIA_SAIDA_AUDIO.md`
> before classifying the result as a stage ceiling or a final master.

The C++ analyzer reads a WAV without altering the original and reports
peak, RMS, estimated LUFS, crest, correlation and headroom:

```sh
.local-build/juce-app-native/navalha_analyze_master mix.wav
```

Input is capped at 512 MiB. As in v0.28.1, LUFS is an internal estimate,
not a certified EBU R128/ITU-R BS.1770 measurement.

The TRACK MASTER renderer applies v0.28.1's default parameters and
publishes a PCM24 with TPDF dither atomically, never overwriting files:

```sh
.local-build/juce-app-native/navalha_render_master \
  mix.wav mix_MASTER.wav [recipe.master.json]
```

This chain is already deterministic and testable, but still requires
objective and auditory comparison against the WebAudio processing before
being considered a replacement.

The full quantization policy is in
[`docs/DITHER_TPDF.md`](docs/DITHER_TPDF.md). Float32 stays undithered;
fixtures that require non-dithered bytes must request that mode
explicitly.

Existing ALBUM MASTER manifests can be verified and planned without
rendering audio:

```sh
.local-build/juce-app-native/navalha_inspect_album \
  album_ALBUM_MASTER.json 48000
```

Inspection caps the manifest at 4 MiB and rejects more than 99 tracks,
traversal, out-of-range parameters and invalid numeric values.

The batch render processes the tracks associated with the manifest, one at
a time, and publishes each PCM24 through a partial file:

```sh
.local-build/juce-app-native/navalha_render_album \
  album_ALBUM_MASTER.json existing-output-folder
```

All inputs are decoded before the first publication. The command refuses
overwrites, preserves 1 GiB free on the volume and caps each WAV at
512 MiB.

### Offline render of portable projects

A portable project can be rendered without an interface or audio device:

```sh
.local-build/juce-app-native/navalha_render_portable \
  project.zip juce-candidate.wav 30 48000
```

Duration is capped at ten minutes. The render uses small blocks, refuses
to overwrite files and writes first to `.partial`; a failure removes only
that temporary file. On completion, it reports frames, peak, RMS and
checksum.

The portable ZIP → Project v2 → WAV decoder → engine → PCM24 path was
validated with 12,000 frames at 48 kHz and inspected with `ffprobe`. The
temporary artifacts from that validation are removed on completion.

If the JUCE package is not found, the core and its tests can still be
configured with:

```sh
cmake -S . -B build/juce -DNAVALHA_BUILD_JUCE_APP=OFF
```

The core's contracts can also be run under AddressSanitizer and
UndefinedBehaviorSanitizer without creating a persistent build:

```sh
ASAN_OPTIONS=detect_leaks=0 NAVALHA_SANITIZE=1 ./test_core.sh
```

After the build, the combined stress test accepts a virtual duration
between one second and one hour. It compares 64- and 511-frame buffers
exactly, without recording audio:

```sh
.local-build/juce-app-native/navalha_engine_stress_tests --seconds 600
```

The ten-minute scenario passed with Assisted, FORM, TRACE, Heritage
Pitch, mixer and virtual voices all active.

A real v1/v2 project can be validated with no interface, no audio and no
modification to it. The report states the input version and the
canonical v2 form:

```sh
.local-build/juce-app-native/navalha_inspect_project project.json
```

The writer can be exercised without a physical device, using a temporary
PCM24, backpressure and automatic cleanup. Duration is capped at ten
minutes:

```sh
.local-build/juce-app-native/navalha_recording_soak_tests --seconds 60
```

The 60-second run published and reopened 2,880,000 frames with zero
drops; its ~17 MB temporary WAV was removed on completion.

`detect_leaks=0` is needed in the current supervised environment because
LeakSanitizer is incompatible with `ptrace`; this should not be read as an
approval of the absence of leaks. The remaining human trials are in
`docs/FINAL_ACCEPTANCE_CHECKLIST.md`.

### Boundaries

- The first product is standalone.
- The web interface will only be embedded via WebView during the
  transition phase.
- A single `SessionModel` will serve main, PERFORM and MASTER.
- On two suitable screens, PERFORM occupies the second monitor and the
  main window distributes editing/production without a scrollbar; smaller
  screens keep scrolling and the mouse wheel.
- The `TAKES / MASTER` workspace stays supplementary to the realtime
  engine; on wide screens it brings take review and mastering side by
  side.
- The native header offers LANG (EN/PT/FR/ES), a ten-chapter tutorial,
  contextual LEARN in the fixed log panel, and ABOUT in the top-right
  corner.
- The web shell's VIEW zoom was not kept: system DPI, resizing, dual
  layout and scrollbar make up the native behavior instead.
- Languages and help are a partial transposition: the tutorial/LEARN are
  in four languages, but not every instrument label is translated yet.
- v0.28.1 remains the reference until parity is approved.
- **Resolved**: GPLv3 Section 13 allows combining the Navalha 2 part under
  GPL-3.0-or-later with JUCE 8 under AGPL-3.0-only, with no commercial
  license. Each part keeps its own license, and the AGPLv3's network-
  interaction requirement applies to the combination. Full detail in
  `docs/LICENSE_STATUS.md` and `LICENSE-AGPLv3.txt`.

See also:

- `../NAVALHA2_PD/ANALISE_MIGRACAO_JUCE_CPP.txt`
- `../NAVALHA2_PD/docs/VIABILIDADE_JUCE_CPP.md`
- `docs/PARIDADE_V0281.md`
- `docs/LICENSE_STATUS.md`

---

## Português

# Navalha 2 — migração JUCE/C++

<p align="center">
  <img src="docs/site/assets/navalha2-header.png" alt="Navalha 2" width="560">
  <img src="docs/site/assets/navalha2-mascot.png" alt="Mascote Navalha 2" width="150">
</p>

<p align="center">
  <img src="docs/site/assets/navalha2-juce-interface.jpg" alt="Interface nativa do Navalha 2 JUCE: fontes, padrões, gestos, forma, biblioteca e mixer" width="100%">
</p>

Contato: **rasgo.instruments@gmail.com**

Versão atual da migração JUCE: **v0.1.0**. A versão **v0.28.1** mencionada
nesta documentação é a referência funcional Pure Data/web usada para medir
paridade; não é o número de versão do aplicativo JUCE.

Esta árvore é paralela e não substitui a aplicação v0.28.1. Desde a
separação em dois diretórios irmãos dentro de `RASGO/`, o runtime Pure
Data/web app vive em `../NAVALHA2_PD/` (antes era `../` a partir daqui,
quando esta pasta ainda era a subpasta `juce/` dentro do projeto único).

O runtime atual continua em:

- `../NAVALHA2_PD/app/`
- `../NAVALHA2_PD/bridge/`
- `../NAVALHA2_PD/core/`
- `../NAVALHA2_PD/run_navalha.sh`

### Estado desta primeira etapa

- modelo C++ inicial para fontes, slices e mixer;
- banco fixo de 10 × 8 padrões com células SOURCE A/B/GAP;
- sequenciador GRID por amostras, com STOP invalidando trabalho pendente;
- mixer DSP estéreo A/B com pan e mid/side width equivalentes ao patch Pure Data;
- rampas lineares de 15 ms no caminho do mixer, sem alocação por amostra;
- buffers estéreo imutáveis durante playback e leitura com interpolação linear;
- player normal/reverse com envelope adaptativo de 0,5–5 ms e STOP com fade;
- autoridade `AudioEngine` ligando sequencer, bancos A/B, players e mixer;
- duas vozes alternadas por source para retrigger com cauda de crossfade;
- shell JUCE ligado a um callback de saída estéreo real via `AudioAppComponent`;
- fila SPSC fixa para comandos UI → áudio, sem locks, espera ou alocação;
- gestos estruturais enviados pela mesma fila UI → áudio;
- bancos de slices em armazenamento fixo, com BLADE contíguo e undo não destrutivo;
- renderização offline em memória com peak, energia e checksum para WAVs dourados;
- regressão dourada Linux com assinaturas do DSP e do WAV PCM24 completo;
- stress dourado combinado de trinta segundos com JITTER, Assisted, FORM,
  TRACE, pitch e duas virtual voices;
- teto de segurança de 10 milhões de amostras por render de teste;
- relógios GRID, FREE e JITTER por amostras, com seed temporal reproduzível;
- MEMORY, MUTATION, EROSION e DECONSTRUCT determinísticos e reversíveis;
- planos sample-accurate de STUTTER ×4 e BURST ×8;
- scheduler realtime fixo para executar STUTTER/BURST sem alocação;
- MICRO ×2–8 com capacidade fixa e reverse por slice na fila realtime;
- controles JUCE nativos para MEMORY, transformações, STUTTER, BURST, MICRO e REVERSE;
- FORM Director com cinco cenas padrão, até 16 cenas, limites, lock e navegação;
- persistência Project v2 interoperável de FORM e TRACE XY (até 512 pontos);
- TRACE LOOP sample-accurate aplicando BPM e Heritage Pitch no callback;
- FORM avançando por frases, trocando bancos LONG/MEDIUM/SHORT/MICRO e
  modulando o contexto do Assisted Performer;
- editor FORM nativo completo com nome, transição, perfis A/B, seis dimensões,
  lock, add/copy/delete/move e undo/redo estrutural de 64 passos;
- captura explícita dos bancos nomeados A/B, recall por cena e persistência
  opcional interoperável no Project v2;
- nomes e histórico de cenas em armazenamento pré-alocado, sem alocação nas
  operações executadas pelo callback;
- estágio MASTER 0–1 após o mixer, com smoothing de 15 ms compartilhado por realtime/offline;
- perfil `liveSafe` no app: saneamento de não-finitos, bloqueio DC e limiter
  estéreo ligado com lookahead de 5 ms e teto de -1 dBTP depois da soma do
  instrumento com Library Preview;
- perfil `legacy` preservado no core para comparação de paridade; o limiter
  liveSafe já contém os casos EBU 19–23 sintetizados da especificação, mas os
  WAV oficiais ainda são necessários antes da declaração geral de conformidade
  true peak; a medição independente FFmpeg já passou;
- `MASTER CREATIVE` permanece no domínio do instrumento; `OUTPUT TRIM` técnico
  atenua de -24 a 0 dB antes do limiter e é persistido como preferência local;
- MUTE técnico com rampa de 10 ms e lifecycle de reconnect com suspensão
  atômica, retomada em silêncio e fade-in, sem tocar no estado do Project;
- duas virtual voices com source, divisão, pattern, foco, pitch, envelope, level e pan;
- headroom automático das vozes compatível com a referência de uma/duas vozes;
- Heritage Pitch com duas cabeças, interpolação `vd~` de quatro pontos,
  janelas cossenoidais e high-pass de 5 Hz;
- crossfade dry/processed de 20 ms compatível com o estágio Pure Data;
- snapshots Project v2 e migração Project v1, preservando estado determinístico;
- FIFO de gravação estéreo pós-MASTER sem locks, com overflow contabilizado;
- encoder WAV RIFF em stream para PCM 16, PCM 24 e float 32;
- dither TPDF determinístico por padrão em PCM16/PCM24, nunca em float32, com
  seed configurável e saneamento final de amostras não finitas;
- validação de caminhos de portable packs contra traversal e caminhos absolutos;
- writer WAV em thread separada, com handshake de parada e drenagem da FIFO;
- publicação atômica de REC: WAV temporário só vira final após RIFF válido;
- catálogo TAKE v1 persistente para novos WAVs finalizados;
- janela TAKE Timeline com metadados privados, review e recipe pré-REC;
- retorno não destrutivo de TAKE para SOURCE A/B;
- limite preventivo de REC por espaço livre, uma hora e teto RIFF de 4 GiB;
- reserva mínima de 1 GiB no volume de destino durante a gravação;
- decoder WAV em memória para PCM 16/24 e float 32, mono ou estéreo;
- decoder PCM24 compatível também com `WAVE_FORMAT_EXTENSIBLE`;
- cache de waveform estéreo min/max com resolução limitada a 8192 bins;
- métricas MASTER compatíveis com a estimativa v0.28.1, com limite de leitura;
- planejamento ALBUM MASTER por frames para trims, fades e gaps;
- codec seguro do manifesto `navalha-album-master` v1 e envelopes lineares;
- cadeia TRACK MASTER C++ inicial com EQ, dinâmica, saturação, width e ceiling;
- codec da receita `navalha-master-recipe` v1 compatível com a interface web;
- regressão dourada específica para detectar alterações na cadeia TRACK MASTER;
- comparação objetiva WebAudio/C++ sobre 353.708 frames dentro de 0,25 dB;
- metadados RIFF LIST/INFO para título, artista, projeto, ano e comentário;
- escrita RIFF posterior opcional na TAKE Timeline, com confirmação, parcial
  validado, backup por link inteligente/cópia e substituição recuperável;
- RNG Mulberry32 do Assisted Performer idêntico ao JavaScript, com seed/cursor;
- planejador Assisted por frase com tempo, transformações, pitch e fragmentos;
- decisões Assisted de source, pattern, region, cuts e AUTO MIX;
- recombinação de patterns com reverse/interleave/mutation, MEMORY e GAP seguro;
- edições automáticas de slices limitadas a nudge, micro, blade, undo e redivide;
- AUTO MIX conservador restrito a balance, pan e width;
- probabilidades, limites e mapeamento FORM/energy equivalentes à v0.28.1;
- execução Assisted realtime ao fechar cada frase, sem timers da interface;
- controles nativos AUTO, vocabulário, faixa BPM, variation e seed/rewind;
- codec JSON interno com limites de tamanho/profundidade para Project v1/v2;
- mapeamento JSON compatível com sources, sequencer, DSP, timing e Assisted state;
- Project v2 preservando também configurações/vocabulário do Assisted;
- automação do mixer interoperável com `dsp.sourceMixer.automation`;
- telemetria realtime de source, pattern, row, BPM, pitch e mixer para o shell;
- Project v2 preservando MEMORY, base e intensidades das transformações;
- portable pack ZIP store com CRC32, limites, deduplicação e proteção traversal;
- serviço portable Navalha restrito a project.navalha e áudio SOURCE A/B;
- shell com LOAD A/B, waveform, PLAY/STOP, MASTER, projeto e gravação WAV;
- controles nativos de BPM/rate, pattern, GRID/FREE/JITTER e Heritage Pitch;
- intensidade e seed reproduzível de JITTER editáveis no shell;
- editor dos oito passos com códigos A0–A127, B0–B127 e GAP;
- mixer nativo A/B com level, pan, width, mute e solo persistentes;
- balance global A/B persistente e suavizado pelo mixer realtime;
- editor nativo de slices com SOURCE A/B, START/END, divisão, BLADE e undo;
- limites e índices de slices desenhados sobre a waveform;
- controles das duas virtual voices para enable, source, divisão, pitch, level e pan;
- detalhe das virtual voices com pattern de 16 passos, comprimento, foco e envelope;
- conteúdo do shell em viewport rolável para telas menores;
- sincronização da fila UI → áudio antes de capturar snapshots de projeto;
- referências v2 de áudio com filename, relativePath, tamanho, data e MIME;
- recarga segura de WAVs relativos ao abrir projetos leves `.navalha`;
- guarda Linux para encerrar claramente quando nenhum display X11 está acessível;
- medidores estéreo, dispositivo e gravação recebem o mesmo sinal pós-safety,
  inclusive quando Library Preview está ativa, além da telemetria de
  frames/drops da gravação;
- medidores mostram sample peak em dBFS; a telemetria adicional publica RMS por
  bloco, pico de entrada, redução de ganho, não-finitos e atuação do ceiling,
  com retenção visual de segurança por dois segundos;
- detector true peak FIR polifásico 4× no fluxo ao vivo, validado inicialmente
  pelos casos 15–23 da EBU Tech 3341, sendo 20–23 fixtures derivadas; ele
  orienta um limiter lookahead de 5 ms, sem alocação no callback, com latência
  explicitamente publicada pelo motor;
- teste de pior soma ao vivo com Sources A/B no máximo, duas virtual voices,
  master em 100% e Library Preview simultânea, exigindo saída finita e teto
  true peak pós-limiter;
- validação cruzada opcional no CTest com FFmpeg `ebur128`: casos de entrada
  15–23 e WAVs pós-limiter verificados por implementação externa;
- contratos de impulso e latência do limiter em 44,1, 48, 96 e 192 kHz, além
  de reinicialização limpa do estágio após mudança de sample rate;
- telemetria atômica PLAY/STOP, passo e geração para a interface;
- destaque realtime do próximo passo sem leitura concorrente do SessionModel;
- playhead A/B lock-free derivado dos players principais, com cursor e
  readout atual/duração nas duas waveforms, inclusive em reverse;
- gravação selecionável em PCM16, PCM24 ou float32 com diagnóstico de drops;
- TAKE Timeline com importação recursiva/deduplicada de WAVs anteriores e
  leitura dos metadados RIFF disponíveis;
- preset privado de metadados derivado de qualquer take e aplicado somente às
  gravações futuras;
- ALBUM PROJECT persistente com seleção de takes, deduplicação, ordem editorial,
  exportação v1 e render direto pelo ALBUM MASTER;
- comparação TRACK MASTER ORIGINAL/MASTER por temporários float32 alinhados,
  com compensação que somente atenua o lado mais alto e bloqueio durante REC;
- matching relativo do ALBUM PROJECT para alvo estimado, limitado a ±6 dB,
  com análise/trim persistentes por faixa;
- painel JUCE de dispositivo, saída estéreo, buffer e sample rate;
- configuração do dispositivo persistida nas preferências locais do aplicativo;
- invariantes compatíveis com Project v2;
- testes que não precisam de JUCE nem dispositivo de áudio;
- shell standalone compilado com JUCE 8.0.13 no Linux;
- nenhuma cópia do JUCE incorporada ao repositório;
- nenhum arquivo da implementação atual removido ou renomeado.

### Teste imediato sem CMake/JUCE

```sh
./test_core.sh
```

O script compila somente o núcleo com o compilador do sistema, executa os
contratos e coloca o binário temporário fora da árvore de fontes.

O CTest completo também verifica que o aplicativo recusa execução headless de
forma limpa, sem segmentation fault.

### Build com CMake

Requisitos:

- CMake 3.22 ou superior;
- compilador com C++20;
- JUCE configurado externamente.

```sh
cmake -S . -B build/juce -DCMAKE_BUILD_TYPE=Debug
cmake --build build/juce
ctest --test-dir build/juce --output-on-failure
```

Com um checkout JUCE local não instalado:

```sh
cmake -S . -B build/juce \
  -DNAVALHA_JUCE_PATH=/caminho/para/JUCE \
  -DNAVALHA_PD_PATH=/caminho/para/navalha2-pd \
  -DCMAKE_BUILD_TYPE=Debug
```

O WebView fica desligado no shell nativo inicial para reduzir dependências:

```sh
cmake -S . -B build/juce \
  -DNAVALHA_JUCE_PATH=/caminho/para/JUCE \
  -DNAVALHA_PD_PATH=/caminho/para/navalha2-pd \
  -DNAVALHA_ENABLE_WEBVIEW=OFF
```

Neste workspace, depois de preparar as dependências locais, o build limitado a
dois processos e os testes podem ser repetidos com:

```sh
./build_local.sh
```

`NAVALHA_JOBS` permite alterar explicitamente o limite de paralelismo.

#### Pacotes multiplataforma (Linux/Windows/macOS)

Downloads: [**release Navalha 2 JUCE v0.1.0**](https://github.com/lucioaraujo/navalha2-juce/releases/tag/v0.1.0)
— `.deb` Linux, `.exe` Windows (NSIS) e `.dmg` macOS (DragNDrop), todos
gerados e testados via CI nos runners hospedados pelo próprio GitHub; não
precisa de máquina Windows nem macOS pra gerar esses builds.

Pra gerar um conjunto novo você mesmo, abra **Actions** no GitHub e rode o
workflow **Package (Linux/Windows/macOS)** (`.github/workflows/
package.yml`); ele também roda automaticamente a cada push de tag `v*`. O
instalador de cada plataforma é disponibilizado como artefato do
workflow, baixável pela página do run.

O pacote Debian gerado localmente reflete a versão das bibliotecas da
máquina de build. Para o `.deb` interno mais compatível, use o mesmo
workflow de CI acima (fixado em Ubuntu 22.04 e JUCE 8.0.13).

As instruções para a pessoa que irá instalar o artefacto estão em
[`docs/INSTALACAO_DEB_INTERNA.md`](docs/INSTALACAO_DEB_INTERNA.md).
Os requisitos mínimos estão em
[`docs/REQUISITOS_MINIMOS_LINUX.md`](docs/REQUISITOS_MINIMOS_LINUX.md).

### Comparação de WAVs

O build também gera `navalha_compare_wav`, usado para comparar uma renderização
da referência Pure Data com a saída JUCE. Os arquivos precisam ter a mesma taxa
e o mesmo número de frames:

```sh
.local-build/juce-app-native/navalha_compare_wav \
  referencia.wav candidato-juce.wav
```

A saída JSON contém RMS da referência, RMS e pico da diferença, correlação e
SNR. A leitura é limitada a 512 MiB por arquivo e não cria cópias no disco.
Uma latência conhecida do candidato pode ser compensada sem copiar o áudio:

```sh
.local-build/juce-app-native/navalha_compare_wav \
  referencia.wav candidato-juce.wav --candidate-offset 480
```

Offset positivo ignora frames iniciais do candidato; negativo ignora frames
iniciais da referência.

### Análise MASTER

> A análise e o TRACK MASTER abaixo preservam o fluxo histórico de migração.
> Eles ainda não constituem a camada comum de saída segura definida para os
> instrumentos RASGO. Ver `docs/AUDITORIA_ENGENHARIA_SAIDA_AUDIO.md` antes de
> classificar o resultado como ceiling de palco ou master final.

O analisador C++ lê um WAV sem alterar o original e informa peak, RMS, LUFS
estimado, crest, correlação e headroom:

```sh
.local-build/juce-app-native/navalha_analyze_master mix.wav
```

A entrada é limitada a 512 MiB. Assim como na v0.28.1, o LUFS é uma estimativa
interna e não uma medição certificada EBU R128/ITU-R BS.1770.

O renderizador TRACK MASTER aplica os parâmetros padrão da v0.28.1 e publica
um PCM24 com dither TPDF de forma atômica, sem sobrescrever arquivos:

```sh
.local-build/juce-app-native/navalha_render_master \
  mix.wav mix_MASTER.wav [receita.master.json]
```

Esta cadeia já é determinística e testável, mas ainda requer comparação
objetiva e auditiva com o processamento WebAudio antes de ser considerada
substituta.

A política completa de quantização está em
[`docs/DITHER_TPDF.md`](docs/DITHER_TPDF.md). Float32 permanece sem dither;
fixtures que exigem bytes não ditherizados precisam pedir esse modo
explicitamente.

Manifestos ALBUM MASTER existentes podem ser verificados e planejados sem
renderizar áudio:

```sh
.local-build/juce-app-native/navalha_inspect_album \
  album_ALBUM_MASTER.json 48000
```

A inspeção limita o manifesto a 4 MiB e recusa mais de 99 faixas, traversal,
parâmetros fora de faixa e valores numéricos inválidos.

O batch render processa as faixas associadas ao manifesto, uma por vez, e
publica cada PCM24 por meio de um arquivo parcial:

```sh
.local-build/juce-app-native/navalha_render_album \
  album_ALBUM_MASTER.json pasta-de-saida-existente
```

Todas as entradas são decodificadas antes da primeira publicação. O comando
recusa sobrescritas, preserva 1 GiB livre no volume e limita cada WAV a 512 MiB.

### Render offline de portable projects

Um portable project pode ser renderizado sem interface ou dispositivo de áudio:

```sh
.local-build/juce-app-native/navalha_render_portable \
  projeto.zip candidato-juce.wav 30 48000
```

A duração é limitada a dez minutos. O render usa blocos pequenos, recusa
sobrescrever arquivos e grava primeiro em `.partial`; uma falha remove somente
esse arquivo temporário. Ao concluir, informa frames, peak, RMS e checksum.

O caminho portable ZIP → Project v2 → decoder WAV → motor → PCM24 foi validado
com 12.000 frames a 48 kHz e inspecionado com `ffprobe`. Os artefatos
temporários dessa validação são removidos ao terminar.

Se o pacote JUCE não for encontrado, o núcleo e seus testes ainda podem ser
configurados com:

```sh
cmake -S . -B build/juce -DNAVALHA_BUILD_JUCE_APP=OFF
```

Os contratos do núcleo também podem ser executados com AddressSanitizer e
UndefinedBehaviorSanitizer sem criar build persistente:

```sh
ASAN_OPTIONS=detect_leaks=0 NAVALHA_SANITIZE=1 ./test_core.sh
```

Depois do build, o stress combinado aceita uma duração virtual entre um segundo
e uma hora. Ele compara exatamente buffers de 64 e 511 frames sem gravar áudio:

```sh
.local-build/juce-app-native/navalha_engine_stress_tests --seconds 600
```

O cenário de dez minutos foi aprovado com Assisted, FORM, TRACE, Heritage
Pitch, mixer e vozes virtuais ativos.

Um projeto v1/v2 real pode ser validado sem interface, sem áudio e sem
modificá-lo. O relatório informa a versão de entrada e a forma canônica v2:

```sh
.local-build/juce-app-native/navalha_inspect_project projeto.json
```

O writer pode ser exercitado sem dispositivo físico, usando PCM24 temporário,
backpressure e limpeza automática. A duração é limitada a dez minutos:

```sh
.local-build/juce-app-native/navalha_recording_soak_tests --seconds 60
```

O ensaio de 60 segundos publicou e reabriu 2.880.000 frames com zero drops; seu
WAV temporário de aproximadamente 17 MB foi removido ao terminar.

`detect_leaks=0` é necessário no ambiente supervisionado atual porque o
LeakSanitizer é incompatível com `ptrace`; isso não deve ser interpretado como
aprovação de ausência de vazamentos. Os ensaios humanos restantes estão em
`docs/FINAL_ACCEPTANCE_CHECKLIST.md`.

### Fronteiras

- O primeiro produto é standalone.
- A interface web será incorporada por WebView apenas na fase de transição.
- Um único `SessionModel` servirá main, PERFORM e MASTER.
- Em duas telas adequadas, PERFORM ocupa o segundo monitor e a main distribui
  edição/produção sem scrollbar; telas menores mantêm rolagem e roda do mouse.
- O workspace `TAKES / MASTER` permanece suplementar ao motor realtime; em
  telas largas reúne revisão de takes e masterização lado a lado.
- O cabeçalho nativo oferece LANG (EN/PT/FR/ES), tutorial em dez capítulos,
  LEARN contextual no painel fixo do log e ABOUT no canto direito.
- O zoom VIEW do shell Web não foi mantido: DPI do sistema, redimensionamento,
  layout dual e scrollbar compõem o comportamento nativo.
- Idiomas e ajuda são uma transposição parcial: tutorial/LEARN estão em quatro
  línguas, mas todos os rótulos do instrumento ainda não foram traduzidos.
- A v0.28.1 continuará sendo a referência até a aprovação de paridade.
- **Resolvido**: a Seção 13 da GPLv3 permite combinar a parte Navalha 2 sob
  GPL-3.0-or-later com o JUCE 8 sob AGPL-3.0-only, sem licença comercial.
  Cada parte mantém sua licença, e a exigência de interação por rede da
  AGPLv3 aplica-se à combinação. Detalhe completo em `docs/LICENSE_STATUS.md`
  e `LICENSE-AGPLv3.txt`.

Consulte também:

- `../NAVALHA2_PD/ANALISE_MIGRACAO_JUCE_CPP.txt`
- `../NAVALHA2_PD/docs/VIABILIDADE_JUCE_CPP.md`
- `docs/PARIDADE_V0281.md`
- `docs/LICENSE_STATUS.md`

---

## Français

<p align="center">
  <img src="docs/site/assets/navalha2-header.png" alt="Navalha 2" width="560">
  <img src="docs/site/assets/navalha2-mascot.png" alt="Mascotte Navalha 2" width="150">
</p>

<p align="center">
  <img src="docs/site/assets/navalha2-juce-interface.jpg" alt="Interface native Navalha 2 JUCE : sources, motifs, gestes, forme, bibliothèque et mixeur" width="100%">
</p>

Contact : **rasgo.instruments@gmail.com**

Version actuelle de la migration JUCE : **v0.1.0**. La version **v0.28.1**
mentionnée dans cette documentation est la référence fonctionnelle Pure
Data/web utilisée pour mesurer la parité ; ce n'est pas le numéro de
version de l'application JUCE elle-même.

Cet arbre est parallèle à l'application v0.28.1 et ne la remplace pas.
Depuis la séparation en deux répertoires frères dans `RASGO/`, le runtime
Pure Data/web vit dans `../NAVALHA2_PD/` (c'était `../` depuis ici avant,
quand ce dossier était encore le sous-dossier `juce/` d'un projet unique).

Le runtime actuel reste dans :

- `../NAVALHA2_PD/app/`
- `../NAVALHA2_PD/bridge/`
- `../NAVALHA2_PD/core/`
- `../NAVALHA2_PD/run_navalha.sh`

### État de cette première étape

- modèle C++ initial pour sources, slices et mixeur ;
- banque fixe de 10 × 8 motifs avec cellules SOURCE A/B/GAP ;
- séquenceur GRID par échantillons, avec STOP invalidant le travail en
  attente ;
- mixeur DSP stéréo A/B avec pan et largeur mid/side équivalents au patch
  Pure Data ;
- rampes linéaires de 15 ms dans le chemin du mixeur, sans allocation par
  échantillon ;
- buffers stéréo immuables pendant la lecture, avec interpolation
  linéaire ;
- lecteur avant/arrière avec enveloppe adaptative de 0,5–5 ms et STOP avec
  fondu ;
- autorité `AudioEngine` reliant le séquenceur, les banques A/B, les
  lecteurs et le mixeur ;
- deux voix alternées par source pour un retrigger avec queue de
  crossfade ;
- shell JUCE relié à un vrai callback de sortie stéréo via
  `AudioAppComponent` ;
- file SPSC fixe pour les commandes UI → audio, sans verrou, attente ni
  allocation ;
- gestes structurels envoyés par la même file UI → audio ;
- banques de slices en stockage fixe, avec BLADE contigu et undo non
  destructif ;
- rendu offline en mémoire avec peak, énergie et checksum pour les WAV de
  référence (« dorés ») ;
- régression dorée Linux avec signatures du DSP et du WAV PCM24 complet ;
- test de stress doré combiné de trente secondes avec JITTER, Assisted,
  FORM, TRACE, pitch et deux virtual voices ;
- plafond de sécurité de 10 millions d'échantillons par rendu de test ;
- horloges GRID, FREE et JITTER par échantillons, avec seed temporelle
  reproductible ;
- MEMORY, MUTATION, EROSION et DECONSTRUCT déterministes et réversibles ;
- plans sample-accurate de STUTTER ×4 et BURST ×8 ;
- ordonnanceur realtime fixe pour exécuter STUTTER/BURST sans allocation ;
- MICRO ×2–8 avec capacité fixe et reverse par slice dans la file
  realtime ;
- contrôles JUCE natifs pour MEMORY, transformations, STUTTER, BURST,
  MICRO et REVERSE ;
- FORM Director avec cinq scènes par défaut, jusqu'à 16 scènes, limites,
  verrouillage et navigation ;
- persistance Project v2 interopérable de FORM et TRACE XY (jusqu'à 512
  points) ;
- TRACE LOOP sample-accurate appliquant BPM et Heritage Pitch dans le
  callback ;
- FORM avançant par phrase, changeant les banques LONG/MEDIUM/SHORT/MICRO
  et modulant le contexte de l'Assisted Performer ;
- éditeur FORM natif complet avec nom, transition, profils A/B, six
  dimensions, verrouillage, add/copy/delete/move et undo/redo structurel
  de 64 pas ;
- capture explicite des banques nommées A/B, rappel par scène et
  persistance optionnelle interopérable dans Project v2 ;
- noms et historique de scènes en stockage préalloué, sans allocation
  dans les opérations exécutées par le callback ;
- étage MASTER 0–1 après le mixeur, avec lissage de 15 ms partagé entre
  realtime/offline ;
- profil `liveSafe` dans l'app : assainissement des non-finis, blocage DC
  et limiteur stéréo avec lookahead de 5 ms et plafond de -1 dBTP après la
  somme de l'instrument avec Library Preview ;
- profil `legacy` préservé dans le core pour comparaison de parité ; le
  limiteur liveSafe contient déjà les cas EBU 19–23 synthétisés de la
  spécification, mais les WAV officiels restent nécessaires avant une
  déclaration générale de conformité true peak ; la mesure indépendante
  FFmpeg est déjà passée ;
- `MASTER CREATIVE` reste dans le domaine de l'instrument ; `OUTPUT TRIM`
  technique atténue de -24 à 0 dB avant le limiteur et est persisté comme
  préférence locale ;
- MUTE technique avec rampe de 10 ms et cycle de reconnexion avec
  suspension atomique, reprise en silence et fondu d'entrée, sans toucher
  à l'état du Project ;
- deux virtual voices avec source, division, motif, focus, pitch,
  enveloppe, niveau et pan ;
- headroom automatique des voix compatible avec la référence à une/deux
  voix ;
- Heritage Pitch avec deux têtes, interpolation à quatre points façon
  `vd~`, fenêtres cosinus et passe-haut à 5 Hz ;
- crossfade dry/processed de 20 ms compatible avec l'étage Pure Data ;
- snapshots Project v2 et migration Project v1, préservant l'état
  déterministe ;
- FIFO d'enregistrement stéréo post-MASTER sans verrou, avec overflow
  comptabilisé ;
- encodeur WAV RIFF en flux pour PCM 16, PCM 24 et float 32 ;
- dither TPDF déterministe par défaut en PCM16/PCM24, jamais en float32,
  avec seed configurable et assainissement final des échantillons non
  finis ;
- validation des chemins de portable packs contre le traversal et les
  chemins absolus ;
- writer WAV sur thread séparé, avec handshake d'arrêt et vidange de la
  FIFO ;
- publication atomique de REC : le WAV temporaire ne devient final
  qu'après un RIFF valide ;
- catalogue TAKE v1 persistant pour les nouveaux WAV finalisés ;
- fenêtre TAKE Timeline avec métadonnées privées, revue et recette
  pré-REC ;
- retour non destructif d'une TAKE vers SOURCE A/B ;
- limite préventive de REC par espace libre, une heure et plafond RIFF de
  4 Gio ;
- réserve minimale de 1 Gio sur le volume de destination pendant
  l'enregistrement ;
- décodeur WAV en mémoire pour PCM 16/24 et float 32, mono ou stéréo ;
- décodeur PCM24 également compatible avec `WAVE_FORMAT_EXTENSIBLE` ;
- cache de waveform stéréo min/max avec résolution limitée à 8192 bins ;
- métriques MASTER compatibles avec l'estimation v0.28.1, avec limite de
  lecture ;
- planification ALBUM MASTER par frames pour trims, fondus et gaps ;
- codec sûr du manifeste `navalha-album-master` v1 et enveloppes
  linéaires ;
- chaîne TRACK MASTER C++ initiale avec EQ, dynamique, saturation, width
  et ceiling ;
- codec de la recette `navalha-master-recipe` v1 compatible avec
  l'interface web ;
- régression dorée spécifique pour détecter les changements dans la
  chaîne TRACK MASTER ;
- comparaison objective WebAudio/C++ sur 353 708 frames à moins de
  0,25 dB ;
- métadonnées RIFF LIST/INFO pour titre, artiste, projet, année et
  commentaire ;
- écriture RIFF ultérieure optionnelle dans la TAKE Timeline, avec
  confirmation, partiel validé, sauvegarde par lien intelligent/copie et
  remplacement récupérable ;
- RNG Mulberry32 de l'Assisted Performer identique à celui en
  JavaScript, avec seed/curseur ;
- planificateur Assisted par phrase avec timing, transformations, pitch
  et fragments ;
- décisions Assisted de source, motif, région, coupes et AUTO MIX ;
- recombinaison de motifs avec reverse/interleave/mutation, MEMORY et GAP
  sûr ;
- éditions automatiques de slices limitées à nudge, micro, blade, undo et
  redivide ;
- AUTO MIX conservateur restreint à balance, pan et width ;
- probabilités, limites et mapping FORM/energy équivalents à v0.28.1 ;
- exécution Assisted en temps réel à la clôture de chaque phrase, sans
  timers d'interface ;
- contrôles natifs AUTO, vocabulaire, plage BPM, variation et
  seed/rewind ;
- codec JSON interne avec limites de taille/profondeur pour Project
  v1/v2 ;
- mapping JSON compatible avec sources, séquenceur, DSP, timing et état
  Assisted ;
- Project v2 préservant aussi les réglages/vocabulaire de l'Assisted ;
- automation du mixeur interopérable avec `dsp.sourceMixer.automation` ;
- télémétrie temps réel de source, motif, ligne, BPM, pitch et mixeur
  pour le shell ;
- Project v2 préservant MEMORY, base et intensités des transformations ;
- portable pack ZIP store avec CRC32, limites, déduplication et
  protection contre le traversal ;
- service portable Navalha restreint à project.navalha et à l'audio
  SOURCE A/B ;
- shell avec LOAD A/B, waveform, PLAY/STOP, MASTER, projet et
  enregistrement WAV ;
- contrôles natifs de BPM/rate, motif, GRID/FREE/JITTER et Heritage
  Pitch ;
- intensité et seed reproductible de JITTER modifiables dans le shell ;
- éditeur des huit pas avec codes A0–A127, B0–B127 et GAP ;
- mixeur natif A/B avec level, pan, width, mute et solo persistants ;
- balance globale A/B persistante et lissée par le mixeur temps réel ;
- éditeur natif de slices avec SOURCE A/B, START/END, division, BLADE et
  undo ;
- limites et index des slices dessinés sur la waveform ;
- contrôles des deux virtual voices pour enable, source, division, pitch,
  level et pan ;
- détail des virtual voices avec motif de 16 pas, longueur, focus et
  enveloppe ;
- contenu du shell dans un viewport défilant pour les petits écrans ;
- synchronisation de la file UI → audio avant de capturer les snapshots
  de projet ;
- références audio v2 avec filename, relativePath, taille, date et MIME ;
- rechargement sûr des WAV relatifs à l'ouverture de projets légers
  `.navalha` ;
- garde Linux pour s'arrêter proprement quand aucun display X11 n'est
  accessible ;
- les vumètres stéréo, le périphérique et l'enregistrement reçoivent le
  même signal post-safety, y compris quand Library Preview est active,
  plus la télémétrie de frames/drops de l'enregistrement ;
- les vumètres affichent le sample peak en dBFS ; la télémétrie
  additionnelle publie le RMS par bloc, le pic d'entrée, la réduction de
  gain, les non-finis et l'activation du ceiling, avec une retenue
  visuelle de sécurité de deux secondes ;
- détecteur true peak FIR polyphasé 4× dans le flux live, validé
  initialement par les cas 15–23 de l'EBU Tech 3341 (20–23 étant des
  fixtures dérivées) ; il pilote un limiteur avec lookahead de 5 ms, sans
  allocation dans le callback, avec latence explicitement publiée par le
  moteur ;
- test de pire somme en direct avec Sources A/B au maximum, deux virtual
  voices, master à 100 % et Library Preview simultanée, exigeant une
  sortie finie et un plafond true peak après le limiteur ;
- validation croisée optionnelle dans CTest avec `ebur128` de FFmpeg :
  cas d'entrée 15–23 et WAV post-limiteur vérifiés par une implémentation
  externe ;
- contrats d'impulsion et de latence du limiteur à 44,1, 48, 96 et
  192 kHz, plus réinitialisation propre de l'étage après changement de
  sample rate ;
- télémétrie atomique PLAY/STOP, pas et génération pour l'interface ;
- surbrillance temps réel du prochain pas sans lecture concurrente du
  SessionModel ;
- playhead A/B lock-free dérivé des lecteurs principaux, avec curseur et
  lecture actuel/durée sur les deux waveforms, y compris en reverse ;
- enregistrement sélectionnable en PCM16, PCM24 ou float32 avec
  diagnostic de drops ;
- TAKE Timeline avec import récursif/dédupliqué des WAV précédents et
  lecture des métadonnées RIFF disponibles ;
- préréglage privé de métadonnées dérivé de n'importe quelle take et
  appliqué seulement aux enregistrements futurs ;
- ALBUM PROJECT persistant avec sélection de takes, déduplication, ordre
  éditorial, export v1 et rendu direct via ALBUM MASTER ;
- comparaison TRACK MASTER ORIGINAL/MASTER via des temporaires float32
  alignés, avec compensation qui n'atténue que le côté le plus fort et
  verrouillage pendant REC ;
- matching relatif de l'ALBUM PROJECT vers une cible estimée, plafonné à
  ±6 dB, avec analyse/trim persistants par piste ;
- panneau JUCE natif pour périphérique, sortie stéréo, buffer et sample
  rate ;
- configuration du périphérique persistée dans les préférences locales de
  l'app ;
- invariants compatibles avec Project v2 ;
- tests ne nécessitant ni JUCE ni périphérique audio ;
- shell standalone compilé avec JUCE 8.0.13 sur Linux ;
- aucune copie de JUCE intégrée au dépôt ;
- aucun fichier de l'implémentation actuelle supprimé ou renommé.

### Test rapide sans CMake/JUCE

```sh
./test_core.sh
```

Le script compile uniquement le core avec le compilateur du système,
exécute les contrats et place le binaire temporaire hors de l'arbre des
sources.

Le CTest complet vérifie aussi que l'application refuse proprement une
exécution headless, sans segmentation fault.

### Build avec CMake

Prérequis :

- CMake 3.22 ou supérieur ;
- compilateur C++20 ;
- JUCE configuré en externe.

```sh
cmake -S . -B build/juce -DCMAKE_BUILD_TYPE=Debug
cmake --build build/juce
ctest --test-dir build/juce --output-on-failure
```

Avec un checkout JUCE local non installé :

```sh
cmake -S . -B build/juce \
  -DNAVALHA_JUCE_PATH=/chemin/vers/JUCE \
  -DNAVALHA_PD_PATH=/chemin/vers/navalha2-pd \
  -DCMAKE_BUILD_TYPE=Debug
```

Le WebView est désactivé dans le shell natif initial pour réduire les
dépendances :

```sh
cmake -S . -B build/juce \
  -DNAVALHA_JUCE_PATH=/chemin/vers/JUCE \
  -DNAVALHA_PD_PATH=/chemin/vers/navalha2-pd \
  -DNAVALHA_ENABLE_WEBVIEW=OFF
```

Dans ce workspace, après préparation des dépendances locales, le build
limité à deux processus et les tests peuvent être relancés avec :

```sh
./build_local.sh
```

`NAVALHA_JOBS` permet de changer explicitement la limite de parallélisme.

#### Paquets multiplateformes (Linux/Windows/macOS)

Téléchargements : [**release Navalha 2 JUCE v0.1.0**](https://github.com/lucioaraujo/navalha2-juce/releases/tag/v0.1.0)
— `.deb` Linux, `.exe` Windows (NSIS) et `.dmg` macOS (DragNDrop), tous
générés et testés via CI sur les runners hébergés par GitHub lui-même ;
aucune machine Windows ou macOS n'est nécessaire pour générer ces builds.

Pour générer un nouveau jeu vous-même, ouvrez **Actions** sur GitHub et
lancez le workflow **Package (Linux/Windows/macOS)**
(`.github/workflows/package.yml`) ; il tourne aussi automatiquement à
chaque push d'un tag `v*`. L'installateur de chaque plateforme est publié
comme artefact du workflow, téléchargeable depuis la page du run.

Le paquet Debian généré localement reflète les versions des bibliothèques
de la machine de build. Pour le `.deb` interne le plus compatible,
utilisez le même workflow CI ci-dessus (fixé sur Ubuntu 22.04 et
JUCE 8.0.13).

Les instructions pour la personne qui installera l'artefact sont dans
[`docs/INSTALACAO_DEB_INTERNA.md`](docs/INSTALACAO_DEB_INTERNA.md).
Les prérequis minimaux sont dans
[`docs/REQUISITOS_MINIMOS_LINUX.md`](docs/REQUISITOS_MINIMOS_LINUX.md).

### Comparaison de WAV

Le build produit aussi `navalha_compare_wav`, utilisé pour comparer un
rendu de référence Pure Data à la sortie JUCE. Les fichiers doivent avoir
le même taux d'échantillonnage et le même nombre de frames :

```sh
.local-build/juce-app-native/navalha_compare_wav \
  reference.wav candidat-juce.wav
```

La sortie JSON contient le RMS de la référence, le RMS et le pic de la
différence, la corrélation et le SNR. La lecture est plafonnée à 512 Mio
par fichier et ne crée pas de copies sur le disque. Une latence connue du
candidat peut être compensée sans copier l'audio :

```sh
.local-build/juce-app-native/navalha_compare_wav \
  reference.wav candidat-juce.wav --candidate-offset 480
```

Un offset positif ignore les frames initiales du candidat ; négatif
ignore les frames initiales de la référence.

### Analyse MASTER

> L'analyse et le TRACK MASTER ci-dessous préservent le flux historique de
> la migration. Ils ne constituent pas encore la couche commune de sortie
> sûre définie pour les instruments RASGO. Voir
> `docs/AUDITORIA_ENGENHARIA_SAIDA_AUDIO.md` avant de classer le résultat
> comme plafond de scène ou master final.

L'analyseur C++ lit un WAV sans altérer l'original et rapporte peak, RMS,
LUFS estimé, crest, corrélation et headroom :

```sh
.local-build/juce-app-native/navalha_analyze_master mix.wav
```

L'entrée est plafonnée à 512 Mio. Comme en v0.28.1, le LUFS est une
estimation interne, pas une mesure certifiée EBU R128/ITU-R BS.1770.

Le rendu TRACK MASTER applique les paramètres par défaut de la v0.28.1 et
publie un PCM24 avec dither TPDF de façon atomique, sans jamais écraser de
fichiers :

```sh
.local-build/juce-app-native/navalha_render_master \
  mix.wav mix_MASTER.wav [recette.master.json]
```

Cette chaîne est déjà déterministe et testable, mais nécessite encore une
comparaison objective et auditive avec le traitement WebAudio avant
d'être considérée comme un remplacement.

La politique complète de quantification est dans
[`docs/DITHER_TPDF.md`](docs/DITHER_TPDF.md). Le float32 reste sans
dither ; les fixtures qui exigent des octets non ditherisés doivent
demander explicitement ce mode.

Les manifestes ALBUM MASTER existants peuvent être vérifiés et planifiés
sans rendre l'audio :

```sh
.local-build/juce-app-native/navalha_inspect_album \
  album_ALBUM_MASTER.json 48000
```

L'inspection plafonne le manifeste à 4 Mio et rejette plus de 99 pistes,
le traversal, les paramètres hors limites et les valeurs numériques
invalides.

Le rendu par lot traite les pistes associées au manifeste, une par une,
et publie chaque PCM24 via un fichier partiel :

```sh
.local-build/juce-app-native/navalha_render_album \
  album_ALBUM_MASTER.json dossier-de-sortie-existant
```

Toutes les entrées sont décodées avant la première publication. La
commande refuse les écrasements, préserve 1 Gio libre sur le volume et
plafonne chaque WAV à 512 Mio.

### Rendu offline de portable projects

Un portable project peut être rendu sans interface ni périphérique
audio :

```sh
.local-build/juce-app-native/navalha_render_portable \
  projet.zip candidat-juce.wav 30 48000
```

La durée est plafonnée à dix minutes. Le rendu utilise de petits blocs,
refuse d'écraser des fichiers et écrit d'abord dans `.partial` ; un échec
ne supprime que ce fichier temporaire. À la fin, il rapporte frames, peak,
RMS et checksum.

Le chemin portable ZIP → Project v2 → décodeur WAV → moteur → PCM24 a été
validé avec 12 000 frames à 48 kHz et inspecté avec `ffprobe`. Les
artefacts temporaires de cette validation sont supprimés à la fin.

Si le paquet JUCE n'est pas trouvé, le core et ses tests peuvent quand
même être configurés avec :

```sh
cmake -S . -B build/juce -DNAVALHA_BUILD_JUCE_APP=OFF
```

Les contrats du core peuvent aussi être exécutés avec AddressSanitizer et
UndefinedBehaviorSanitizer sans créer de build persistant :

```sh
ASAN_OPTIONS=detect_leaks=0 NAVALHA_SANITIZE=1 ./test_core.sh
```

Après le build, le test de stress combiné accepte une durée virtuelle
entre une seconde et une heure. Il compare exactement des buffers de 64
et 511 frames sans enregistrer d'audio :

```sh
.local-build/juce-app-native/navalha_engine_stress_tests --seconds 600
```

Le scénario de dix minutes a réussi avec Assisted, FORM, TRACE, Heritage
Pitch, mixeur et virtual voices tous actifs.

Un vrai projet v1/v2 peut être validé sans interface, sans audio et sans
le modifier. Le rapport indique la version d'entrée et la forme canonique
v2 :

```sh
.local-build/juce-app-native/navalha_inspect_project projet.json
```

Le writer peut être testé sans périphérique physique, avec un PCM24
temporaire, backpressure et nettoyage automatique. La durée est plafonnée
à dix minutes :

```sh
.local-build/juce-app-native/navalha_recording_soak_tests --seconds 60
```

Le run de 60 secondes a publié et rouvert 2 880 000 frames avec zéro
drop ; son WAV temporaire d'environ 17 Mo a été supprimé à la fin.

`detect_leaks=0` est nécessaire dans l'environnement supervisé actuel car
LeakSanitizer est incompatible avec `ptrace` ; cela ne doit pas être lu
comme une approbation de l'absence de fuites. Les essais humains restants
sont dans `docs/FINAL_ACCEPTANCE_CHECKLIST.md`.

### Frontières

- Le premier produit est standalone.
- L'interface web ne sera embarquée via WebView que pendant la phase de
  transition.
- Un seul `SessionModel` servira main, PERFORM et MASTER.
- Sur deux écrans adaptés, PERFORM occupe le second moniteur et la
  fenêtre principale distribue édition/production sans scrollbar ; les
  écrans plus petits gardent le défilement et la molette de la souris.
- L'espace de travail `TAKES / MASTER` reste supplémentaire au moteur
  temps réel ; sur écran large, il réunit revue des takes et
  masterisation côte à côte.
- L'en-tête natif offre LANG (EN/PT/FR/ES), un tutoriel en dix chapitres,
  un LEARN contextuel dans le panneau de log fixe, et ABOUT dans le coin
  supérieur droit.
- Le zoom VIEW du shell web n'a pas été conservé : le DPI système, le
  redimensionnement, la disposition double écran et la scrollbar
  composent le comportement natif à la place.
- Langues et aide sont une transposition partielle : le tutoriel/LEARN
  sont en quatre langues, mais tous les libellés de l'instrument ne sont
  pas encore traduits.
- La v0.28.1 reste la référence jusqu'à l'approbation de la parité.
- **Résolu** : la Section 13 de la GPLv3 permet de combiner la partie
  Navalha 2 sous GPL-3.0-or-later avec JUCE 8 sous AGPL-3.0-only, sans
  licence commerciale. Chaque partie garde sa propre licence, et
  l'exigence d'interaction réseau de l'AGPLv3 s'applique à la
  combinaison. Détail complet dans `docs/LICENSE_STATUS.md` et
  `LICENSE-AGPLv3.txt`.

Voir aussi :

- `../NAVALHA2_PD/ANALISE_MIGRACAO_JUCE_CPP.txt`
- `../NAVALHA2_PD/docs/VIABILIDADE_JUCE_CPP.md`
- `docs/PARIDADE_V0281.md`
- `docs/LICENSE_STATUS.md`

---

## Español

<p align="center">
  <img src="docs/site/assets/navalha2-header.png" alt="Navalha 2" width="560">
  <img src="docs/site/assets/navalha2-mascot.png" alt="Mascota Navalha 2" width="150">
</p>

<p align="center">
  <img src="docs/site/assets/navalha2-juce-interface.jpg" alt="Interfaz nativa de Navalha 2 JUCE: fuentes, patrones, gestos, forma, biblioteca y mezclador" width="100%">
</p>

Contacto: **rasgo.instruments@gmail.com**

Versión actual de la migración JUCE: **v0.1.0**. La versión **v0.28.1**
mencionada en esta documentación es la referencia funcional Pure Data/web
usada para medir la paridad; no es el número de versión de la aplicación
JUCE en sí.

Este árbol es paralelo a la aplicación v0.28.1 y no la sustituye. Desde la
separación en dos directorios hermanos dentro de `RASGO/`, el runtime
Pure Data/web vive en `../NAVALHA2_PD/` (antes era `../` desde aquí,
cuando esta carpeta todavía era la subcarpeta `juce/` de un proyecto
único).

El runtime actual sigue en:

- `../NAVALHA2_PD/app/`
- `../NAVALHA2_PD/bridge/`
- `../NAVALHA2_PD/core/`
- `../NAVALHA2_PD/run_navalha.sh`

### Estado de esta primera etapa

- modelo C++ inicial para fuentes, slices y mezclador;
- banco fijo de 10 × 8 patrones con celdas SOURCE A/B/GAP;
- secuenciador GRID por muestras, con STOP invalidando trabajo
  pendiente;
- mezclador DSP estéreo A/B con pan y anchura mid/side equivalentes al
  patch Pure Data;
- rampas lineales de 15 ms en el camino del mezclador, sin asignación por
  muestra;
- buffers estéreo inmutables durante la reproducción y lectura con
  interpolación lineal;
- reproductor normal/inverso con envolvente adaptativa de 0,5–5 ms y
  STOP con fundido;
- autoridad `AudioEngine` uniendo secuenciador, bancos A/B, reproductores
  y mezclador;
- dos voces alternadas por fuente para retrigger con cola de crossfade;
- shell JUCE conectado a un callback de salida estéreo real vía
  `AudioAppComponent`;
- cola SPSC fija para comandos UI → audio, sin locks, espera ni
  asignación;
- gestos estructurales enviados por la misma cola UI → audio;
- bancos de slices en almacenamiento fijo, con BLADE contiguo y undo no
  destructivo;
- renderizado offline en memoria con peak, energía y checksum para WAV
  dorados;
- regresión dorada Linux con firmas del DSP y del WAV PCM24 completo;
- stress dorado combinado de treinta segundos con JITTER, Assisted, FORM,
  TRACE, pitch y dos virtual voices;
- techo de seguridad de 10 millones de muestras por render de prueba;
- relojes GRID, FREE y JITTER por muestras, con seed temporal
  reproducible;
- MEMORY, MUTATION, EROSION y DECONSTRUCT determinísticos y reversibles;
- planes sample-accurate de STUTTER ×4 y BURST ×8;
- planificador realtime fijo para ejecutar STUTTER/BURST sin asignación;
- MICRO ×2–8 con capacidad fija y reverse por slice en la cola realtime;
- controles JUCE nativos para MEMORY, transformaciones, STUTTER, BURST,
  MICRO y REVERSE;
- FORM Director con cinco escenas por defecto, hasta 16 escenas, límites,
  bloqueo y navegación;
- persistencia Project v2 interoperable de FORM y TRACE XY (hasta 512
  puntos);
- TRACE LOOP sample-accurate aplicando BPM y Heritage Pitch en el
  callback;
- FORM avanzando por frase, cambiando bancos LONG/MEDIUM/SHORT/MICRO y
  modulando el contexto del Assisted Performer;
- editor FORM nativo completo con nombre, transición, perfiles A/B, seis
  dimensiones, bloqueo, add/copy/delete/move y undo/redo estructural de
  64 pasos;
- captura explícita de los bancos nombrados A/B, recall por escena y
  persistencia opcional interoperable en Project v2;
- nombres e historial de escenas en almacenamiento preasignado, sin
  asignación en las operaciones ejecutadas por el callback;
- etapa MASTER 0–1 después del mezclador, con suavizado de 15 ms
  compartido entre realtime/offline;
- perfil `liveSafe` en la app: saneamiento de no-finitos, bloqueo DC y
  limitador estéreo con lookahead de 5 ms y techo de -1 dBTP tras la suma
  del instrumento con Library Preview;
- perfil `legacy` preservado en el core para comparación de paridad; el
  limitador liveSafe ya contiene los casos EBU 19–23 sintetizados de la
  especificación, pero los WAV oficiales aún son necesarios antes de una
  declaración general de conformidad true peak; la medición independiente
  con FFmpeg ya pasó;
- `MASTER CREATIVE` permanece en el dominio del instrumento; el
  `OUTPUT TRIM` técnico atenúa de -24 a 0 dB antes del limitador y se
  persiste como preferencia local;
- MUTE técnico con rampa de 10 ms y ciclo de reconexión con suspensión
  atómica, reanudación en silencio y fundido de entrada, sin tocar el
  estado del Project;
- dos virtual voices con source, división, patrón, foco, pitch,
  envolvente, level y pan;
- headroom automático de las voces compatible con la referencia de
  una/dos voces;
- Heritage Pitch con dos cabezas, interpolación de cuatro puntos estilo
  `vd~`, ventanas coseno y paso alto de 5 Hz;
- crossfade dry/processed de 20 ms compatible con la etapa Pure Data;
- snapshots Project v2 y migración Project v1, preservando estado
  determinístico;
- FIFO de grabación estéreo post-MASTER sin locks, con overflow
  contabilizado;
- codificador WAV RIFF en flujo para PCM 16, PCM 24 y float 32;
- dither TPDF determinístico por defecto en PCM16/PCM24, nunca en
  float32, con seed configurable y saneamiento final de muestras no
  finitas;
- validación de rutas de portable packs contra traversal y rutas
  absolutas;
- writer WAV en hilo separado, con handshake de parada y drenaje de la
  FIFO;
- publicación atómica de REC: el WAV temporal solo se vuelve final tras
  un RIFF válido;
- catálogo TAKE v1 persistente para nuevos WAV finalizados;
- ventana TAKE Timeline con metadatos privados, revisión y receta
  pre-REC;
- retorno no destructivo de una TAKE a SOURCE A/B;
- límite preventivo de REC por espacio libre, una hora y techo RIFF de
  4 GiB;
- reserva mínima de 1 GiB en el volumen de destino durante la grabación;
- decodificador WAV en memoria para PCM 16/24 y float 32, mono o
  estéreo;
- decodificador PCM24 también compatible con `WAVE_FORMAT_EXTENSIBLE`;
- caché de waveform estéreo min/max con resolución limitada a 8192 bins;
- métricas MASTER compatibles con la estimación v0.28.1, con límite de
  lectura;
- planificación ALBUM MASTER por frames para trims, fades y gaps;
- codec seguro del manifiesto `navalha-album-master` v1 y envolventes
  lineales;
- cadena TRACK MASTER C++ inicial con EQ, dinámica, saturación, width y
  ceiling;
- codec de la receta `navalha-master-recipe` v1 compatible con la
  interfaz web;
- regresión dorada específica para detectar cambios en la cadena TRACK
  MASTER;
- comparación objetiva WebAudio/C++ sobre 353.708 frames dentro de
  0,25 dB;
- metadatos RIFF LIST/INFO para título, artista, proyecto, año y
  comentario;
- escritura RIFF posterior opcional en la TAKE Timeline, con
  confirmación, parcial validado, respaldo por enlace inteligente/copia y
  reemplazo recuperable;
- RNG Mulberry32 del Assisted Performer idéntico al de JavaScript, con
  seed/cursor;
- planificador Assisted por frase con tiempo, transformaciones, pitch y
  fragmentos;
- decisiones Assisted de source, patrón, región, cortes y AUTO MIX;
- recombinación de patrones con reverse/interleave/mutation, MEMORY y GAP
  seguro;
- ediciones automáticas de slices limitadas a nudge, micro, blade, undo y
  redivide;
- AUTO MIX conservador restringido a balance, pan y width;
- probabilidades, límites y mapeo FORM/energy equivalentes a v0.28.1;
- ejecución Assisted en tiempo real al cerrar cada frase, sin timers de
  interfaz;
- controles nativos AUTO, vocabulario, rango de BPM, variation y
  seed/rewind;
- codec JSON interno con límites de tamaño/profundidad para Project
  v1/v2;
- mapeo JSON compatible con sources, secuenciador, DSP, timing y estado
  Assisted;
- Project v2 preservando también configuraciones/vocabulario del
  Assisted;
- automatización del mezclador interoperable con
  `dsp.sourceMixer.automation`;
- telemetría en tiempo real de source, patrón, fila, BPM, pitch y
  mezclador para el shell;
- Project v2 preservando MEMORY, base e intensidades de las
  transformaciones;
- portable pack ZIP store con CRC32, límites, deduplicación y protección
  contra traversal;
- servicio portable Navalha restringido a project.navalha y audio
  SOURCE A/B;
- shell con LOAD A/B, waveform, PLAY/STOP, MASTER, proyecto y grabación
  WAV;
- controles nativos de BPM/rate, patrón, GRID/FREE/JITTER y Heritage
  Pitch;
- intensidad y seed reproducible de JITTER editables en el shell;
- editor de los ocho pasos con códigos A0–A127, B0–B127 y GAP;
- mezclador nativo A/B con level, pan, width, mute y solo persistentes;
- balance global A/B persistente y suavizado por el mezclador realtime;
- editor nativo de slices con SOURCE A/B, START/END, división, BLADE y
  undo;
- límites e índices de slices dibujados sobre la waveform;
- controles de las dos virtual voices para enable, source, división,
  pitch, level y pan;
- detalle de las virtual voices con patrón de 16 pasos, longitud, foco y
  envolvente;
- contenido del shell en un viewport desplazable para pantallas más
  pequeñas;
- sincronización de la cola UI → audio antes de capturar snapshots de
  proyecto;
- referencias de audio v2 con filename, relativePath, tamaño, fecha y
  MIME;
- recarga segura de WAV relativos al abrir proyectos ligeros `.navalha`;
- guardia Linux para cerrar limpiamente cuando no hay display X11
  accesible;
- los medidores estéreo, el dispositivo y la grabación reciben la misma
  señal post-safety, incluso con Library Preview activo, más la
  telemetría de frames/drops de la grabación;
- los medidores muestran el sample peak en dBFS; la telemetría adicional
  publica RMS por bloque, pico de entrada, reducción de ganancia,
  no-finitos y activación del ceiling, con una retención visual de
  seguridad de dos segundos;
- detector true peak FIR polifásico 4× en el flujo en vivo, validado
  inicialmente por los casos 15–23 de la EBU Tech 3341 (siendo 20–23
  fixtures derivadas); orienta un limitador con lookahead de 5 ms, sin
  asignación en el callback, con latencia publicada explícitamente por el
  motor;
- prueba de peor suma en vivo con Sources A/B al máximo, dos virtual
  voices, master al 100% y Library Preview simultánea, exigiendo salida
  finita y techo true peak tras el limitador;
- validación cruzada opcional en CTest con `ebur128` de FFmpeg: casos de
  entrada 15–23 y WAV post-limitador verificados por una implementación
  externa;
- contratos de impulso y latencia del limitador a 44,1, 48, 96 y
  192 kHz, además de reinicialización limpia de la etapa tras un cambio
  de sample rate;
- telemetría atómica de PLAY/STOP, paso y generación para la interfaz;
- resaltado en tiempo real del siguiente paso sin lectura concurrente del
  SessionModel;
- playhead A/B lock-free derivado de los reproductores principales, con
  cursor y lectura actual/duración en ambas waveforms, incluso en
  reverse;
- grabación seleccionable en PCM16, PCM24 o float32 con diagnóstico de
  drops;
- TAKE Timeline con importación recursiva/deduplicada de WAV anteriores y
  lectura de los metadatos RIFF disponibles;
- preset privado de metadatos derivado de cualquier take y aplicado solo
  a grabaciones futuras;
- ALBUM PROJECT persistente con selección de takes, deduplicación, orden
  editorial, exportación v1 y render directo mediante ALBUM MASTER;
- comparación TRACK MASTER ORIGINAL/MASTER mediante temporales float32
  alineados, con compensación que solo atenúa el lado más alto y bloqueo
  durante REC;
- matching relativo del ALBUM PROJECT hacia un objetivo estimado,
  limitado a ±6 dB, con análisis/trim persistentes por pista;
- panel JUCE nativo de dispositivo, salida estéreo, buffer y sample
  rate;
- configuración del dispositivo persistida en las preferencias locales de
  la app;
- invariantes compatibles con Project v2;
- pruebas que no necesitan ni JUCE ni dispositivo de audio;
- shell standalone compilado con JUCE 8.0.13 en Linux;
- ninguna copia de JUCE incorporada al repositorio;
- ningún archivo de la implementación actual eliminado o renombrado.

### Prueba inmediata sin CMake/JUCE

```sh
./test_core.sh
```

El script compila solo el núcleo con el compilador del sistema, ejecuta
los contratos y coloca el binario temporal fuera del árbol de fuentes.

El CTest completo también verifica que la aplicación rechaza la ejecución
headless de forma limpia, sin segmentation fault.

### Build con CMake

Requisitos:

- CMake 3.22 o superior;
- compilador con C++20;
- JUCE configurado externamente.

```sh
cmake -S . -B build/juce -DCMAKE_BUILD_TYPE=Debug
cmake --build build/juce
ctest --test-dir build/juce --output-on-failure
```

Con un checkout local de JUCE no instalado:

```sh
cmake -S . -B build/juce \
  -DNAVALHA_JUCE_PATH=/ruta/a/JUCE \
  -DNAVALHA_PD_PATH=/ruta/a/navalha2-pd \
  -DCMAKE_BUILD_TYPE=Debug
```

El WebView está apagado en el shell nativo inicial para reducir
dependencias:

```sh
cmake -S . -B build/juce \
  -DNAVALHA_JUCE_PATH=/ruta/a/JUCE \
  -DNAVALHA_PD_PATH=/ruta/a/navalha2-pd \
  -DNAVALHA_ENABLE_WEBVIEW=OFF
```

En este workspace, tras preparar las dependencias locales, el build
limitado a dos procesos y las pruebas pueden repetirse con:

```sh
./build_local.sh
```

`NAVALHA_JOBS` permite cambiar explícitamente el límite de paralelismo.

#### Paquetes multiplataforma (Linux/Windows/macOS)

Descargas: [**release Navalha 2 JUCE v0.1.0**](https://github.com/lucioaraujo/navalha2-juce/releases/tag/v0.1.0)
— `.deb` Linux, `.exe` Windows (NSIS) y `.dmg` macOS (DragNDrop), todos
generados y probados vía CI en los runners alojados por el propio
GitHub; no se necesita una máquina Windows ni macOS para generar estos
builds.

Para generar un nuevo conjunto usted mismo, abra **Actions** en GitHub y
ejecute el workflow **Package (Linux/Windows/macOS)**
(`.github/workflows/package.yml`); también corre automáticamente en cada
push de un tag `v*`. El instalador de cada plataforma se publica como
artefacto del workflow, descargable desde la página del run.

El paquete Debian generado localmente refleja las versiones de las
bibliotecas de la máquina de build. Para el `.deb` interno más
compatible, use el mismo workflow de CI de arriba (fijado en Ubuntu 22.04
y JUCE 8.0.13).

Las instrucciones para quien instale el artefacto están en
[`docs/INSTALACAO_DEB_INTERNA.md`](docs/INSTALACAO_DEB_INTERNA.md).
Los requisitos mínimos están en
[`docs/REQUISITOS_MINIMOS_LINUX.md`](docs/REQUISITOS_MINIMOS_LINUX.md).

### Comparación de WAV

El build también genera `navalha_compare_wav`, usado para comparar un
render de referencia Pure Data con la salida JUCE. Los archivos necesitan
la misma tasa de muestreo y el mismo número de frames:

```sh
.local-build/juce-app-native/navalha_compare_wav \
  referencia.wav candidato-juce.wav
```

La salida JSON contiene el RMS de la referencia, el RMS y el pico de la
diferencia, la correlación y el SNR. La lectura está limitada a 512 MiB
por archivo y no crea copias en disco. Una latencia conocida del
candidato puede compensarse sin copiar el audio:

```sh
.local-build/juce-app-native/navalha_compare_wav \
  referencia.wav candidato-juce.wav --candidate-offset 480
```

Un offset positivo ignora frames iniciales del candidato; negativo
ignora frames iniciales de la referencia.

### Análisis MASTER

> El análisis y el TRACK MASTER de abajo preservan el flujo histórico de
> la migración. Todavía no constituyen la capa común de salida segura
> definida para los instrumentos RASGO. Ver
> `docs/AUDITORIA_ENGENHARIA_SAIDA_AUDIO.md` antes de clasificar el
> resultado como techo de escenario o master final.

El analizador C++ lee un WAV sin alterar el original e informa peak, RMS,
LUFS estimado, crest, correlación y headroom:

```sh
.local-build/juce-app-native/navalha_analyze_master mix.wav
```

La entrada está limitada a 512 MiB. Como en v0.28.1, el LUFS es una
estimación interna, no una medición certificada EBU R128/ITU-R BS.1770.

El renderizador TRACK MASTER aplica los parámetros por defecto de v0.28.1
y publica un PCM24 con dither TPDF de forma atómica, sin sobrescribir
nunca archivos:

```sh
.local-build/juce-app-native/navalha_render_master \
  mix.wav mix_MASTER.wav [receta.master.json]
```

Esta cadena ya es determinística y comprobable, pero todavía requiere
comparación objetiva y auditiva con el procesamiento WebAudio antes de
considerarse un reemplazo.

La política completa de cuantización está en
[`docs/DITHER_TPDF.md`](docs/DITHER_TPDF.md). Float32 permanece sin
dither; los fixtures que requieren bytes sin ditherizar deben pedir ese
modo explícitamente.

Los manifiestos ALBUM MASTER existentes pueden verificarse y planificarse
sin renderizar audio:

```sh
.local-build/juce-app-native/navalha_inspect_album \
  album_ALBUM_MASTER.json 48000
```

La inspección limita el manifiesto a 4 MiB y rechaza más de 99 pistas,
traversal, parámetros fuera de rango y valores numéricos inválidos.

El render por lotes procesa las pistas asociadas al manifiesto, una por
una, y publica cada PCM24 mediante un archivo parcial:

```sh
.local-build/juce-app-native/navalha_render_album \
  album_ALBUM_MASTER.json carpeta-de-salida-existente
```

Todas las entradas se decodifican antes de la primera publicación. El
comando rechaza sobrescrituras, preserva 1 GiB libre en el volumen y
limita cada WAV a 512 MiB.

### Render offline de portable projects

Un portable project puede renderizarse sin interfaz ni dispositivo de
audio:

```sh
.local-build/juce-app-native/navalha_render_portable \
  proyecto.zip candidato-juce.wav 30 48000
```

La duración está limitada a diez minutos. El render usa bloques
pequeños, rechaza sobrescribir archivos y escribe primero en `.partial`;
un fallo elimina solo ese archivo temporal. Al terminar, informa frames,
peak, RMS y checksum.

La ruta portable ZIP → Project v2 → decodificador WAV → motor → PCM24 fue
validada con 12.000 frames a 48 kHz e inspeccionada con `ffprobe`. Los
artefactos temporales de esa validación se eliminan al terminar.

Si no se encuentra el paquete JUCE, el núcleo y sus pruebas aún pueden
configurarse con:

```sh
cmake -S . -B build/juce -DNAVALHA_BUILD_JUCE_APP=OFF
```

Los contratos del núcleo también pueden ejecutarse con AddressSanitizer y
UndefinedBehaviorSanitizer sin crear un build persistente:

```sh
ASAN_OPTIONS=detect_leaks=0 NAVALHA_SANITIZE=1 ./test_core.sh
```

Tras el build, el stress combinado acepta una duración virtual entre un
segundo y una hora. Compara exactamente buffers de 64 y 511 frames sin
grabar audio:

```sh
.local-build/juce-app-native/navalha_engine_stress_tests --seconds 600
```

El escenario de diez minutos fue aprobado con Assisted, FORM, TRACE,
Heritage Pitch, mezclador y virtual voices activos.

Un proyecto v1/v2 real puede validarse sin interfaz, sin audio y sin
modificarlo. El informe indica la versión de entrada y la forma canónica
v2:

```sh
.local-build/juce-app-native/navalha_inspect_project proyecto.json
```

El writer puede ejercitarse sin dispositivo físico, usando un PCM24
temporal, backpressure y limpieza automática. La duración está limitada a
diez minutos:

```sh
.local-build/juce-app-native/navalha_recording_soak_tests --seconds 60
```

La ejecución de 60 segundos publicó y reabrió 2.880.000 frames con cero
drops; su WAV temporal de aproximadamente 17 MB fue eliminado al
terminar.

`detect_leaks=0` es necesario en el entorno supervisado actual porque
LeakSanitizer es incompatible con `ptrace`; esto no debe interpretarse
como una aprobación de la ausencia de fugas. Las pruebas humanas
restantes están en `docs/FINAL_ACCEPTANCE_CHECKLIST.md`.

### Fronteras

- El primer producto es standalone.
- La interfaz web solo se incorporará vía WebView durante la fase de
  transición.
- Un único `SessionModel` servirá a main, PERFORM y MASTER.
- En dos pantallas adecuadas, PERFORM ocupa el segundo monitor y la
  ventana principal distribuye edición/producción sin scrollbar;
  pantallas más pequeñas mantienen el desplazamiento y la rueda del
  ratón.
- El espacio de trabajo `TAKES / MASTER` permanece suplementario al motor
  realtime; en pantallas anchas reúne revisión de takes y masterización
  lado a lado.
- El encabezado nativo ofrece LANG (EN/PT/FR/ES), un tutorial de diez
  capítulos, LEARN contextual en el panel de log fijo, y ABOUT en la
  esquina superior derecha.
- El zoom VIEW del shell web no se mantuvo: el DPI del sistema, el
  redimensionamiento, el layout dual y la scrollbar componen el
  comportamiento nativo en su lugar.
- Idiomas y ayuda son una transposición parcial: el tutorial/LEARN están
  en cuatro idiomas, pero no todas las etiquetas del instrumento están
  traducidas todavía.
- La v0.28.1 sigue siendo la referencia hasta que se apruebe la paridad.
- **Resuelto**: la Sección 13 de la GPLv3 permite combinar la parte
  Navalha 2 bajo GPL-3.0-or-later con JUCE 8 bajo AGPL-3.0-only, sin
  licencia comercial. Cada parte mantiene su propia licencia, y el
  requisito de interacción por red de la AGPLv3 se aplica a la
  combinación. Detalle completo en `docs/LICENSE_STATUS.md` y
  `LICENSE-AGPLv3.txt`.

Ver también:

- `../NAVALHA2_PD/ANALISE_MIGRACAO_JUCE_CPP.txt`
- `../NAVALHA2_PD/docs/VIABILIDADE_JUCE_CPP.md`
- `docs/PARIDADE_V0281.md`
- `docs/LICENSE_STATUS.md`
