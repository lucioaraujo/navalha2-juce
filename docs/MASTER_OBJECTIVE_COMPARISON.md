# Comparação objetiva TRACK MASTER

Data: 30 de julho de 2026  
Fixture: `validation-output/recordings/NAVALHA_2026-07-29_17-14-37.wav`  
Frames: 353.708, estéreo, 44.100 Hz

A referência foi renderizada pelo Chrome headless com `OfflineAudioContext` e
a cadeia literal de `app/master.js`. O mesmo WAV foi processado pela cadeia
C++ e analisado pelo medidor compartilhado do JUCE.

| Métrica | WebAudio | C++ | Diferença |
|---|---:|---:|---:|
| Peak dBFS | -8,4348 | -8,6104 | -0,1756 dB |
| RMS dBFS | -12,8867 | -13,0232 | -0,1364 dB |
| LUFS estimado | -13,5777 | -13,7142 | -0,1364 |
| Crest dB | 4,4519 | 4,4128 | -0,0391 dB |
| Correlação | 0,903711 | 0,902815 | -0,000896 |

Critério automatizado desta etapa: diferença absoluta inferior a 0,25 dB em
peak/RMS/LUFS estimado e inferior a 0,01 na correlação. A cadeia C++ atende ao
critério. Isso não substitui comparação auditiva humana nem certificação EBU.

O harness reproduzível está em
`juce/validation/master_webaudio_reference.html`.
