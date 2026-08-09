# Validação cruzada de true peak com FFmpeg

Data: 9 de agosto de 2026.

## Objetivo

Comparar o detector e o limiter do Navalha 2 com uma implementação externa,
sem usar a mesma rotina DSP como autoridade dos dois lados. A referência
externa deste passe é o filtro `ebur128=peak=true` do FFmpeg 6.1.1.

Os sinais seguem os casos 15–23 da
[EBU Tech 3341](https://tech.ebu.ch/docs/tech/tech3341.pdf). Os casos 15–19 são
tons definidos diretamente pela tabela; 20–23 são derivados matematicamente a
192 kHz, filtrados e decimados nos quatro offsets. Eles não são cópias dos WAVs
distribuídos pela EBU.

## Procedimento reproduzível

- `navalha_validation` concentra a geração das fixtures;
- `navalha_render_true_peak_fixtures` exporta entrada e saída limitada como WAV
  float32, preservando amostras acima de 0 dBFS sem clipping de quantização;
- `validation/ValidateTruePeakFfmpeg.cmake` mede os 18 arquivos com FFmpeg e
  falha fora das tolerâncias declaradas;
- quando FFmpeg está disponível, CMake registra automaticamente o teste
  `navalha_true_peak_ffmpeg` no CTest.

```sh
cmake --build .local-build/juce-app-native \
  --target navalha_render_true_peak_fixtures
ctest --test-dir .local-build/juce-app-native \
  -R navalha_true_peak_ffmpeg --output-on-failure
```

## Resultado

| Caso | Esperado | Navalha entrada | FFmpeg entrada | Navalha limitado | FFmpeg limitado |
|---:|---:|---:|---:|---:|---:|
| 15 | -6,0 | -6,19 | -6,0 | -6,19 | -6,0 |
| 16 | -6,0 | -6,19 | -6,0 | -6,19 | -6,0 |
| 17 | -6,0 | -6,09 | -6,0 | -6,09 | -6,0 |
| 18 | -6,0 | -6,06 | -6,0 | -6,06 | -6,0 |
| 19 | +3,0 | +2,82 | +3,0 | -1,20 | -1,0 |
| 20 derivado | 0,0 | -0,19 | -0,1 | -1,20 | -1,1 |
| 21 derivado | 0,0 | -0,09 | -0,1 | -1,20 | -1,2 |
| 22 derivado | 0,0 | -0,20 | -0,1 | -1,20 | -1,1 |
| 23 derivado | 0,0 | -0,09 | -0,1 | -1,20 | -1,2 |

Valores em dBTP. O detector interno permaneceu dentro da tolerância EBU de
`+0,2/-0,4 dBTP`. A leitura externa confirmou transparência abaixo do teto e
mediu todas as saídas limitadas exigentes entre -1,0 e -1,2 dBTP.

## Limite da evidência

Este cruzamento é independente do DSP do Navalha, mas usa as mesmas fixtures
matemáticas geradas localmente. Ainda falta comparar os casos 20–23 com o
[pacote oficial EBU](https://tech.ebu.ch/publications/ebu_loudness_test_set),
cujo download direto retornou HTTP 403 neste passe, e realizar o ensaio humano
em dispositivo físico. Por isso o relatório não declara conformidade geral nem
autoriza sozinho a classificação de master final.
