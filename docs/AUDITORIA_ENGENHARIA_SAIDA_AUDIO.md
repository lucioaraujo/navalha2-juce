# Auditoria de engenharia da saída de áudio — Navalha 2 JUCE

Data da auditoria: 9 de agosto de 2026.

## 1. Veredito e escopo

O Navalha 2 JUCE possui uma fundação realtime consistente, gravação assíncrona
e uma cadeia TRACK MASTER offline determinística. Ainda não atende, porém, à
engenharia de saída definida para a família RASGO em
`../../RASGO_DOCUMENTATION/architecture/SAIDA_AUDIO_COMUM.md`.

Paridade com o Pure Data/WebAudio e excelência de saída são critérios
diferentes. Reproduzir o comportamento histórico não transforma um ganho
linear em limiter, nem uma estimativa RMS em medição BS.1770.

Esta auditoria é diagnóstica. Ela não registra a cadeia atual como aprovada para
ceiling de palco, true peak ou master final.

Quando a correção produzir componentes reutilizáveis, a promoção deve seguir
[`MODULOS_DSP_COMPARTILHADOS.md`](../../RASGO_DOCUMENTATION/architecture/MODULOS_DSP_COMPARTILHADOS.md)
e ser registrada em
[`HISTORICO_GLOBAL.md`](../../RASGO_DOCUMENTATION/HISTORICO_GLOBAL.md). O
comportamento criativo, a genealogia e a compatibilidade do Navalha 2 permanecem
locais.

## 2. Fluxo atual

```text
players A/B
  -> mixer A/B
  -> soma de virtual voices
  -> Heritage Pitch
  -> MASTER linear suavizado
  -> medidor sample-peak + FIFO de gravação
  -> saída JUCE

Library Preview
  -> soma direta no buffer JUCE depois do fluxo acima
```

O TRACK MASTER offline é separado:

```text
trim -> HPF/EQ -> compressor -> tanh -> width -> compressor 20:1
     -> multiplicação pelo ceiling -> WAV PCM24
```

## 3. Achados bloqueadores

### 3.1 Saída ao vivo sem proteção final

Depois do `masterLevel` não existem proteção DC, limiter, hard ceiling ou
controle true peak. Sources A/B, sobreposição de players e virtual voices podem
somar acima de 0 dBFS antes de chegar ao dispositivo e ao writer.

O master padrão `0.8` equivale a aproximadamente -1,94 dB, insuficiente para
garantir headroom quando duas fontes full-scale em fase são somadas.

### 3.2 Preview fora do MASTER

A Library Preview é adicionada ao buffer depois de `engine.processBlock`.
Portanto não passa pelo master, não entra na gravação, não aparece no medidor do
motor e pode clipar ao ser somada à performance.

### 3.3 Medidor oculta overs

O medidor publica apenas pico por amostra e a GUI limita a indicação a `0..1`.
Não há escala dBFS, true peak, RMS, gain reduction, peak hold ou clip latch. Um
pico pouco acima de 0 dBFS e um overs severo têm a mesma aparência visual.

### 3.4 TRACK MASTER não garante o ceiling

O estágio chamado limiter é um compressor 20:1, sem lookahead, com ataque de
3 ms, seguido de ganho correspondente ao ceiling. Uma prova com os parâmetros
padrão e ceiling de -1 dBFS produziu:

| Pico de entrada | Pico de saída |
|---:|---:|
| 1,0 | -1,02 dBFS |
| 2,0 | **+1,02 dBFS** |
| 4,0 | **+1,33 dBFS** |

O writer PCM16/24 então limita silenciosamente amostras a `[-1, 1]`; isso é
clipping na codificação, não proteção de master.

### 3.5 Saturation zero não é neutra

Com `saturation = 0`, o drive interno ainda é 1 e o sinal atravessa `tanh`
normalizada por `1/tanh(1)`. Isso pode ser necessário para paridade histórica,
mas deve ser nomeado como modo legado; não é bypass neutro. Não há oversampling
nesse estágio não linear.

### 3.6 Loudness não padronizada

O valor chamado `estimatedLufs` é RMS global com offset, sem K-weighting,
blocos/gating e pesos definidos pelo BS.1770. A análise de arquivos longos pode
saltar frames e perder o pico real. Ela deve continuar rotulada como estimativa
até ser substituída ou acompanhada por medição de conformidade.

### 3.7 Exportação sem dither

PCM16 e PCM24 usam arredondamento direto. Falta dither TPDF na redução para
ponto fixo. Float32 deve permanecer sem dither e preservar headroom quando o tap
de gravação escolhido for pré-safety.

## 4. Pontos fortes preservados

- smoothing de 15 ms em master, level, pan e width;
- envelopes de slices e fade de parada;
- FIFO de gravação sem lock e writer fora do callback;
- publicação atômica e formatos PCM16, PCM24 e float32;
- configuração persistente de dispositivo, sample rate e buffer;
- validação de comandos não-finitos e saneamento do decoder WAV float;
- testes determinísticos e execução independente do tamanho de bloco.

Na auditoria, contratos do core, golden render/master, stress de 30 segundos e
gravação de 240.000 frames passaram. A fixture de stress chegou somente a pico
`0.624161`; ela não cobre o pior caso de soma ou true peak.

## 5. Plano de correção

### P0 — segurança e coerência ao vivo

- inserir `OutputStage` comum depois de toda soma, inclusive Preview;
- decidir e documentar taps da gravação;
- limiter estéreo linkado com lookahead/true peak e latência explícita;
- proteção DC, output trim em dB, mute/fades seguros;
- medição pós-soma real, com dBFS, true peak, RMS, GR e clip latch;
- testes de todas as fontes/vozes em fase e no máximo permitido.

### P1 — TRACK/ALBUM MASTER

- separar modo legado/paridade de um modo de qualidade novo;
- garantir bypass neutro dos estágios opcionais;
- oversampling em saturação/clipper e detecção true peak;
- medição BS.1770 validada por fixtures externas;
- dither TPDF em PCM16/24 e análise posterior do arquivo publicado;
- preview A/B com compensação de loudness e latência.

### P2 — palco e multiplataforma

- perfis Low Latency e Safe/Quality com diferenças visíveis;
- telemetria de xrun/dropout, CPU, sample rate e buffer;
- reconnect de dispositivo sem pico e estado seguro ao iniciar;
- soak tests por backend e plataforma.

## 6. Critérios de aceite

O item só pode ser marcado como concluído quando:

- nenhum cenário válido ultrapassar o ceiling true peak configurado;
- Preview, medidor, dispositivo e tap `post-safety` ouvirem o mesmo sinal;
- `NaN`/`Inf`, DC e mudanças de dispositivo não contaminarem a saída;
- bypass e `saturation = 0` tiverem semântica explícita e testada;
- BS.1770 e dither forem verificados por fixtures apropriadas;
- stress de pior caso e ensaio humano passarem sem clipping ou xrun.

Assinatura dourada e paridade WebAudio continuam úteis, mas não satisfazem esses
critérios isoladamente.

## 7. Progresso de implementação após a auditoria

### 2026-08-09 — P0.1: fronteira única da saída ao vivo

- criado `OutputStage` local, independente de GUI, com saneamento de `NaN`/`Inf`,
  bloqueio DC a 5 Hz e ceiling de sample peak em -1 dBFS com ganho estéreo
  ligado e release de 80 ms;
- criado o perfil `liveSafe`, ativado pelo aplicativo JUCE; o perfil `legacy`
  permanece disponível no core para comparação de paridade;
- Library Preview passou a entrar no `AudioEngine` antes do estágio final;
- medidor, dispositivo e FIFO de gravação `post-safety` agora recebem o mesmo
  sinal, incluindo Preview;
- foram adicionados testes de ceiling, ligação estéreo, não-finitos, rejeição de
  DC e igualdade entre Preview gravado e Preview enviado à saída;
- a telemetria passou a publicar sample peak em dBFS, RMS por bloco, pico de
  entrada, redução de ganho, não-finitos e atuação do ceiling; a GUI retém o
  alerta de proteção por dois segundos sem esconder o valor de pico;
- app completo compilado e os nove testes CTest passaram, incluindo golden
  render, stress, recording soak e guard do app.

Naquele marco, o estágio reduzia imediatamente o risco de sample clipping, mas
**não fechava o P0**: tinha latência zero e ainda não usava lookahead nem
detector oversampled. Essa limitação histórica foi substituída nos marcos P0.2
e P0.3 abaixo. Loudness conforme BS.1770 e as fixtures transitórias continuam
fora do escopo já concluído.

### 2026-08-09 — P0.2: detector true peak 4×

- implementado `TruePeakDetector`, FIR polifásico 4×, sem alocação ou lock no
  processamento;
- a telemetria do `OutputStage` passou a medir true peak antes e depois da
  proteção, mantendo sample peak e RMS como medidas distintas;
- os tons sintéticos dos casos 15–19 da
  [EBU Tech 3341](https://tech.ebu.ch/docs/tech/tech3341.pdf) passaram dentro da
  tolerância normativa de `+0,2/−0,4 dBTP`, incluindo o caso de `+3 dBTP` cuja
  amostra discreta não revela o pico reconstruído;
- a bateria completa permaneceu em 9/9 testes e o app JUCE recompilou.

Esse marco isolou o detector antes de sua integração ao limiter. A conformidade
geral ainda não estava encerrada porque faltavam o ceiling pós-limiter e os
sinais transitórios oficiais 20–23.

### 2026-08-09 — P0.3: limiter lookahead integrado

- implementado `LookaheadLimiter` estéreo linkado, com lookahead padrão de
  5 ms, release de 80 ms e margem interna conservadora de 0,2 dB;
- o detector true peak 4× orienta a envoltória antes de o áudio alcançar a
  saída atrasada; o processamento não aloca memória nem toma locks;
- a latência é declarada: 240 amostras a 48 kHz, expostas também por
  `AudioEngine::outputLatencySamples()`;
- o caso EBU 19 de +3 dBTP foi contido próximo do ceiling de -1 dBTP, enquanto
  o caso 16 abaixo do teto permaneceu neutro e sem redução de ganho;
- os casos transitórios 20–23 foram derivados matematicamente da Tech 3341 em
  192 kHz, filtrados e decimados nos quatro offsets; o detector mediu de
  -0,20 a -0,09 dBTP, dentro da tolerância `+0,2/-0,4 dBTP`, e o limiter conteve
  todos próximos de -1 dBTP;
- `OutputStage`, Library Preview, medidor e gravação pós-safety foram validados
  já com a latência do lookahead; o perfil legado continuou preservando as
  assinaturas golden;
- um ensaio de pior soma combinou Sources A/B no nível máximo permitido, duas
  virtual voices em fase, MASTER em 100% e Library Preview; a entrada passou de
  1,5 FS e a saída permaneceu finita, com true peak no máximo em -0,9 dBTP;
- uma medição externa com FFmpeg 6.1.1 confirmou os casos de entrada e mediu as
  saídas exigentes entre -1,0 e -1,2 dBTP; o procedimento passou a integrar o
  CTest e está documentado em [`VALIDACAO_TRUE_PEAK_FFMPEG.md`](VALIDACAO_TRUE_PEAK_FFMPEG.md);
- impulsos severos passaram em 44,1, 48, 96 e 192 kHz, com latência proporcional
  à taxa; `prepare()` em nova sample rate também zerou corretamente estado e
  telemetria antes de retomar o processamento;
- o aplicativo completo compilou e os dez testes CTest passaram, incluindo
  true peak externo, golden render, stress, recording soak e headless guard.

O P0 ainda não deve ser chamado de totalmente conforme: os casos 20–23 usados
são fixtures reproduzíveis derivadas da especificação, não os WAV oficiais. O
download direto do pacote EBU v5 retornou HTTP 403 em 2026-08-09. Ao fim do
P0.3 ainda restavam a comparação com esses WAVs, política explícita de output
trim/mute, transições de reconnect e ensaio humano. O marco seguinte resolve a
parte determinística desses controles.

### 2026-08-09 — P0.4: controle técnico e lifecycle do dispositivo

- `MASTER` foi nomeado `MASTER CREATIVE`, preservando sua função musical;
- criado `OUTPUT TRIM` independente, restrito a atenuação de -24 a 0 dB antes
  do limiter, com rampa de 20 ms e persistência nas preferências locais do app;
- criado MUTE técnico com rampa de 10 ms; trim e mute não entram no Project nem
  alteram o estado artístico do instrumento;
- pedidos da GUI são publicados atomicamente e aplicados pela thread de áudio
  no início do bloco, sem acesso concorrente aos objetos DSP;
- `releaseResources()` e paradas controladas marcam a saída como suspensa; um
  novo `prepareToPlay()` reconfigura taxa/latência em silêncio e só então libera
  fade-in, reduzindo risco de pico na reconexão;
- telemetria e interface distinguem trim, mute e `RECONNECT SAFE`;
- contratos verificam proporção de -6 dB, silêncio após mute, ausência de salto
  grande, retomada, suspensão e re-prepare em 96 kHz;
- app completo compilado e 10/10 testes passaram, incluindo a validação externa
  FFmpeg e as assinaturas golden do perfil legado.
- smoke test do standalone abriu o dispositivo real em 44,1 kHz com o sink do
  sistema temporariamente silenciado; callback, estado `AUDIO CONNECTED` e layout
  de MASTER CREATIVE / OUTPUT TRIM / MUTE foram confirmados. O mute do sistema
  foi restaurado ao estado anterior ao encerrar.

O controle software não pode produzir um fade-out depois que um driver falhou
e deixou de chamar o callback. Portanto a reconexão automática está coberta no
lado determinístico — suspensão e fade-in —, mas desconexão física, xrun e ruído
do hardware continuam exigindo o roteiro humano.

### 2026-08-09 — P1.1: dither TPDF na publicação PCM

- `WavStreamWriter` passou a aplicar TPDF de ±1 LSB antes da quantização PCM16
  e PCM24; float32 permanece sem dither;
- a política é padrão nos caminhos de REC, TRACK MASTER, ALBUM MASTER e
  conversão PCM24 para projeto portátil, sem inserir ruído no barramento float;
- seed configurável e padrão fixo preservam reprodutibilidade; fixtures
  científicas e WAVs dourados podem pedir `none` explicitamente;
- canais esquerdo e direito consomem valores sucessivos, sem compartilhar o
  mesmo ruído de dither;
- o writer agora também saneia `NaN`/`Inf` antes de codificar qualquer formato;
- contratos cobrem determinismo, seeds distintas, distribuição TPDF em silêncio,
  média próxima de zero, PCM24, bypass explícito e neutralidade float32;
- decisão e evidências estão documentadas em
  [`DITHER_TPDF.md`](DITHER_TPDF.md).

Este marco resolve a ausência de dither do encoder, mas não encerra P1:
BS.1770, análise pós-codificação, modo de master de qualidade, oversampling da
saturação e preview A/B com compensação ainda permanecem abertos.
