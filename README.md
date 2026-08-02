# Navalha 2 — migração JUCE/C++

Esta árvore é paralela e não substitui a aplicação v0.28.1. Desde a
separação em dois diretórios irmãos dentro de `RASGO/`, o runtime Pure
Data/web app vive em `../NAVALHA2_PD/` (antes era `../` a partir daqui,
quando esta pasta ainda era a subpasta `juce/` dentro do projeto único).

O runtime atual continua em:

- `../NAVALHA2_PD/app/`
- `../NAVALHA2_PD/bridge/`
- `../NAVALHA2_PD/core/`
- `../NAVALHA2_PD/run_navalha.sh`

## Estado desta primeira etapa

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
- editor FORM nativo completo com transição, perfis A/B e seis dimensões;
- nomes e operações de cenas em armazenamento fixo, sem alocação realtime;
- estágio MASTER 0–1 após o mixer, com smoothing de 15 ms compartilhado por realtime/offline;
- duas virtual voices com source, divisão, pattern, foco, pitch, envelope, level e pan;
- headroom automático das vozes compatível com a referência de uma/duas vozes;
- Heritage Pitch com duas cabeças, interpolação `vd~` de quatro pontos,
  janelas cossenoidais e high-pass de 5 Hz;
- crossfade dry/processed de 20 ms compatível com o estágio Pure Data;
- snapshots Project v2 e migração Project v1, preservando estado determinístico;
- FIFO de gravação estéreo pós-MASTER sem locks, com overflow contabilizado;
- encoder WAV RIFF em stream para PCM 16, PCM 24 e float 32;
- validação de caminhos de portable packs contra traversal e caminhos absolutos;
- writer WAV em thread separada, com handshake de parada e drenagem da FIFO;
- publicação atômica de REC: WAV temporário só vira final após RIFF válido;
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
- medidores estéreo pós-MASTER e telemetria de frames/drops da gravação;
- telemetria atômica PLAY/STOP, passo e geração para a interface;
- destaque realtime do próximo passo sem leitura concorrente do SessionModel;
- gravação selecionável em PCM16, PCM24 ou float32 com diagnóstico de drops;
- painel JUCE de dispositivo, saída estéreo, buffer e sample rate;
- configuração do dispositivo persistida nas preferências locais do aplicativo;
- invariantes compatíveis com Project v2;
- testes que não precisam de JUCE nem dispositivo de áudio;
- shell standalone compilado com JUCE 8.0.13 no Linux;
- nenhuma cópia do JUCE incorporada ao repositório;
- nenhum arquivo da implementação atual removido ou renomeado.

## Teste imediato sem CMake/JUCE

```sh
./test_core.sh
```

O script compila somente o núcleo com o compilador do sistema, executa os
contratos e coloca o binário temporário fora da árvore de fontes.

O CTest completo também verifica que o aplicativo recusa execução headless de
forma limpa, sem segmentation fault.

## Build com CMake

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

## Comparação de WAVs

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

## Análise MASTER

O analisador C++ lê um WAV sem alterar o original e informa peak, RMS, LUFS
estimado, crest, correlação e headroom:

```sh
.local-build/juce-app-native/navalha_analyze_master mix.wav
```

A entrada é limitada a 512 MiB. Assim como na v0.28.1, o LUFS é uma estimativa
interna e não uma medição certificada EBU R128/ITU-R BS.1770.

O renderizador TRACK MASTER aplica os parâmetros padrão da v0.28.1 e publica
um PCM24 de forma atômica, sem sobrescrever arquivos:

```sh
.local-build/juce-app-native/navalha_render_master \
  mix.wav mix_MASTER.wav [receita.master.json]
```

Esta cadeia já é determinística e testável, mas ainda requer comparação
objetiva e auditiva com o processamento WebAudio antes de ser considerada
substituta.

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

## Render offline de portable projects

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

## Fronteiras

- O primeiro produto é standalone.
- A interface web será incorporada por WebView apenas na fase de transição.
- Um único `SessionModel` servirá main, PERFORM e MASTER.
- PERFORM pode ocupar o segundo monitor.
- MASTER permanece suplementar e não participa da distribuição dos módulos.
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
