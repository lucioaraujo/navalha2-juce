# Fluxo de sinal — Navalha 2 JUCE

Mapa do motor ao vivo (`src/core/AudioEngine.cpp` + módulos que ele orquestra),
nos mesmos três eixos definidos em
[`RASGO_DOCUMENTATION/GOVERNANCA_E_TRANSVERSALIDADE.md`](../../RASGO_DOCUMENTATION/GOVERNANCA_E_TRANSVERSALIDADE.md#3-responsabilidades)
e já usados em `ANTITOTEM/docs/FLUXO_DE_SINAL.md`. Escrito em 18 ago. 2026
depois de encontrar que `docs/AUDITORIA_ENGENHARIA_SAIDA_AUDIO.md` tinha três
achados (3.1-3.3) desatualizados desde o próprio commit que a criou - ver a
nota de correção no topo daquele arquivo. Este documento cobre só o **motor ao
vivo**; o caminho offline (TRACK MASTER, Album Project, Mastering A/B -
`MasteringProcessor`/`OfflineRenderer`) é um pipeline totalmente separado, não
revisitado aqui (ver a própria auditoria, seção 2 - os achados 3.4-3.7 desse
caminho, incluindo BS.1770 e o limiter com lookahead, foram corrigidos e
validados depois da primeira versão deste documento; auditoria já
atualizada).

## Parte 1 — Fluxo de áudio

```text
Fonte A: 2 SlicePlayer (voz nova rouba a mais antiga das 2) ──┐
                                                                 soma
Fonte B: 2 SlicePlayer (idem) ──────────────────────────────────┼──► renderSource(0)/(1)
                                                                 │
                                                                 ▼
                                      StereoSourceMixer (pan/level/width por
                                      fonte - effectiveLevel já combina nível,
                                      balance, mute/solo antes de chegar aqui)
                                                                 │
2 bancos de "vozes virtuais" (2 SlicePlayer cada, disparados por             │
subdivisão/foco próprios, não pelo Sequencer principal) ──┐                  │
   cada banco: soma dos players → seu próprio                │              │
   StereoChannelProcessor (pan/level/width PRÓPRIO,           │              │
   não o StereoSourceMixer acima) ─────────────────────────────┼──► soma tudo
                                                                 │
                                                                 ▼
                                                        HeritagePitch (global,
                                                        pitch/mode únicos para
                                                        toda a mistura)
                                                                 │
                                                                 ▼
                                                        masterLevel (rampa linear)
                                                                 │
                                                                 ▼
                                                     = "program" (renderSample())
```

```text
finalizeOutput(program, external)
   external = Library Preview (juce::AudioTransportSource independente,
              somado aqui via parâmetro externalLeft/externalRight de
              processBlock - não faz parte do motor SlicePlayer/mixer acima)
   │
   ▼
program + external
   │
   ▼ (só roda se outputProfile == liveSafe - e prepareToPlay() seta isso
   │  incondicionalmente a cada início de áudio, então É o que roda na
   │  prática, não um modo opcional)
OutputStage: finite guard → DC reject → LookaheadLimiter (stereo-linked,
   true-peak 4x) → trim/mute (rampas lineares)
   │
   ├──► medidor da GUI (dBFS, true peak in/out, RMS, gain reduction,
   │     peak-hold + clip-latch de 60 ticks)
   ├──► RecordingFifo (só se `recording` estiver ativo)
   └──► saída do dispositivo JUCE
```

Modo `legacy` (paridade histórica, não usado por `prepareToPlay` hoje) pula o
`OutputStage` inteiro por decisão de projeto, não por omissão - ver o
comentário da própria classe em `OutputStage.h`: "legacy/offline parity
bypasses this stage in the AudioEngine rather than weakening the live
contract".

## Parte 2 — Fluxo de controle / modulação

```text
Sequencer (grid/free/jitter, tempo, PatternBank de 10 padrões × 8 passos)
   │  cada passo processado uma vez por amostra (processSample())
   ▼
SequencerEvent { step, PatternCell(sourceA/sourceB/gap, sliceIndex) }
   │
   ▼
trigger() ──► triggerSlice(source, slice) ──► SlicePlayer da Parte 1

FragmentGesture (planStutter/planBurst) - plano alternativo de até 8 células
   com offsets de frame próprios, avançado por advanceFragmentGesture() a
   cada amostra; suas células chamam o MESMO triggerSlice() acima, não um
   caminho de áudio separado - ver Parte 3 sobre a voz compartilhada.

ControlTrace (pontos de BPM+pitch gravados ao longo do tempo, até 512) -
   enquanto tocando (tracePlayer), sobrescreve tempo do Sequencer e
   semitons/modo do HeritagePitch a cada avanço - não combina com o controle
   manual, substitui-o enquanto ativo (ver Parte 3).

FormDirector (até 16 cenas: bars, energia, transição, perfil de slice bank
   A/B, densidade/tensão/continuidade/contraste/estabilidade/movimento
   estéreo) - avança ao final de cada frase completa (8 passos), e ao trocar
   de cena chama applyCurrentFormSceneMaterial() para reconfigurar o material
   ativo das fontes.

AssistedPerformer (assistedPerformanceContext: intensidade/energia derivadas
   do estado do FormDirector) - usado por applyAssistedPhrase() no fim de
   cada padrão de 8 passos para decidir variação assistida do próximo trecho.
```

## Parte 3 — Topologia de roteamento

### 3.1 — Preview: protegido só condicionalmente, não por natureza

Ver Parte 1: Preview (`external`) só passa pelo `OutputStage` (limiter/DC/
true-peak), só entra na gravação e só aparece no medidor **quando
`outputProfile == liveSafe`**. Isso é verdade na prática hoje (é o único
modo que `prepareToPlay()` usa), mas é uma condição de estado, não uma
garantia estrutural - se algum caminho futuro rodar `processBlock()` sem
passar por `prepareToPlay()` primeiro (ex.: um teste, um harness offline),
Preview volta a ficar desprotegido. Documentado aqui exatamente pelo motivo
da regra de governança: um controle "desliga" ou "protege" uma coisa
específica sob uma condição específica, não universalmente.

### 3.2 — FragmentGesture e Sequencer competem pela mesma voz

`triggerSlice()` é o único ponto de entrada pros `SlicePlayer` de uma fonte,
chamado tanto pelo Sequencer normal (`trigger()`) quanto pelo FragmentGesture
(`triggerFragmentCell()`). As duas vozes por fonte são compartilhadas
(`nextVoice[source]`, roubo round-robin) - um gesto de stutter/burst em
andamento pode roubar a voz que o Sequencer principal acabou de disparar, e
vice-versa. Não são rotas paralelas independentes; são duas fontes de
eventos convergindo no mesmo par de vozes.

### 3.3 — ControlTrace substitui, não soma, o controle manual

Enquanto uma trace está tocando, `advanceControlTrace()` escreve tempo e
pitch a cada amostra processada, incondicionalmente - qualquer ajuste manual
de BPM/pitch feito durante a reprodução é sobrescrito no próximo avanço, não
combinado com ele. O controle manual só volta a valer quando a trace para
(`tracePlayer.stop()`), não antes.

### 3.4 — Vozes normais e vozes virtuais não compartilham mixagem

Ambas somam no mesmo `output` final (Parte 1), mas por processadores
estéreo completamente separados: `StereoSourceMixer` (fontes A/B, com
`effectiveLevel`/balance/mute/solo) e `virtualMixers[voiceIndex]`
(`StereoChannelProcessor` próprio, parâmetros vindos direto de
`VirtualVoiceState`, sem relação com balance/mute/solo do mixer principal).
Mutar/solar a fonte A ou B no MIXER principal não afeta vozes virtuais
mesmo que apontem para a mesma fonte de áudio.

### 3.5 — Offline é outro motor, não uma derivação deste

TRACK MASTER, Album Project e Mastering A/B (`MasteringProcessor`,
`OfflineRenderer`) não chamam `AudioEngine::renderSample()`/
`finalizeOutput()` - é um pipeline de processamento de arquivo próprio (ver
`docs/AUDITORIA_ENGENHARIA_SAIDA_AUDIO.md`, seção 2, "TRACK MASTER offline").
Nada nas Partes 1-3 acima se aplica a exports; os achados 3.4-3.7 daquela
auditoria (todos resolvidos - ver a nota de correção no topo do arquivo) são
a referência para esse caminho.
