# Contrato de paridade com a v0.28.1

O protótipo JUCE não substitui o runtime atual até cumprir estes contratos.

> Auditoria corretiva de 30/07/2026: esta lista descreve principalmente o
> núcleo DSP. A matriz de interface e workflow em
> `AUDITORIA_PARIDADE_PD_V0281_JUCE.md` identificou lacunas ainda reais:
> TAKE Timeline/receitas/metadados, ALBUM PROJECT builder, NVL/PTN, previews e
> partes do FORM Advanced. Portanto, “implementado” abaixo
> não deve ser interpretado como paridade integral do produto.

> Atualização JUCE v0.1.0: a primeira etapa da TAKE Timeline agora registra
> novos WAVs finalizados, preserva review/metadados/recipe em catálogo privado
> e oferece TAKE → SOURCE A/B. A descoberta/importação de WAVs anteriores, o
> preset das próximas gravações e a escrita RIFF posterior com backup também
> estão implementados. O cursor exato do Assisted continua pendente.

> Atualização de interface de 03/08/2026: `LANG`, `TUTORIAL`, `LEARN` e `ABOUT`
> foram incorporados ao cabeçalho nativo; LEARN usa o painel fixo do
> log e o tutorial preserva os dez capítulos em EN/PT/FR/ES. A tradução global
> de todos os rótulos e a granularidade completa das 106 notas LEARN do Web
> continuam parciais.

## Estado do núcleo C++ (ainda sem aprovação auditiva)

- implementado: bancos A/B fixos, divisão, edição de limites, BLADE e undo;
- implementado: patterns A/B/GAP e sequencer GRID/FREE/JITTER por amostras;
- implementado: MEMORY, MUTATION, EROSION e DECONSTRUCT determinísticos;
- validado: fixture estrutural C++ idêntica à execução JavaScript v0.28.1;
- implementado: STUTTER/BURST sample-accurate, MICRO seguro e reverse manual;
- validado: shuffle BURST idêntico à fixture JavaScript com RNG injetado;
- implementado: scheduler fixo de oito eventos para fragmentos no callback;
- implementado: player interpolado normal/reverse com envelopes de declick;
- implementado: mixer A/B, pan, width, balance, mute/solo e MASTER suavizado;
- implementado: duas virtual voices com pitch, envelope, foco, pattern e headroom;
- implementado: Heritage Pitch com interpolação de quatro pontos equivalente ao `vd~`;
- validado: fixture de impulso C++ comparada à resposta do patch Pure Data;
- implementado: fila UI → áudio sem locks e render offline determinístico;
- implementado: snapshot Project v2, migração v1 e FIFO de gravação pós-MASTER;
- implementado: encoder WAV 16/24/float e validação segura de caminhos portáteis;
- implementado: writer WAV em thread separada, com finalização controlada;
- implementado: publicação atômica da gravação e limpeza de parciais em falha;
- validado: soak virtual de gravação PCM24 por 60 segundos/2.880.000 frames,
  com zero drops, contagem exata, reabertura do WAV e remoção imediata;
- implementado: limite de REC por espaço, duração e capacidade RIFF;
- implementado: decoder WAV PCM/float e cache de waveform estéreo;
- implementado: metadados RIFF e seed/estado/cursor do Assisted Performer;
- implementado: codec JSON Project v1/v2 com limites de tamanho e profundidade;
- implementado: inspetor CLI read-only de projetos com relatório de migração
  canônica, limites e erro explícito para conteúdo inválido;
- validado: fixtures documentais v1 e v2 inspecionadas em CTest, incluindo
  migração v1 → modelo canônico v2; ainda falta um projeto real do usuário;
- validado: serialização canônica v2 idempotente e rejeição automatizada de
  JSON truncado e de versão futura não suportada;
- implementado: ZIP portable pack com CRC, traversal e allowlist de arquivos;
- pendente: aprovação auditiva humana do Heritage Pitch contra a referência Pure Data;
- implementado: build Linux completo do shell com JUCE 8.0.13 e CTest aprovado;
- implementado: controles nativos de transporte, timing, patterns e Heritage Pitch;
- implementado: hierarquia visual herdada do layout Pure Data/web, mantendo
  PREPARE e waveform antes dos controles avançados;
- implementado: navegação fixa TOP/PREPARE, WAVEFORM/METERS, PERFORM/CREATE e
  MIX/VOICES, disponível mesmo durante a rolagem;
- implementado: layout dual adaptativo sem scrollbar quando as duas telas têm
  espaço suficiente, com PERFORM no monitor secundário e fallback rolável;
- decisão nativa: VIEW 100/115/130/145 não foi mantido; DPI do sistema,
  redimensionamento responsivo, layout dual e scrollbar substituem esse zoom;
- implementado parcialmente: seletor EN/PT/FR/ES, tutorial nativo de dez
  capítulos e LEARN contextual por ponteiro/foco no painel ACTIVITY LOG;
- implementado: intensidade/seed do JITTER e balance global A/B no shell;
- implementado: mixer A/B e editor de slices/BLADE ligados à fila realtime;
- implementado: overlay dos limites de slices sobre a waveform nativa;
- implementado: playhead A/B derivado da thread de áudio, com cursores sobre as
  duas waveforms, tempo atual/duração, movimento reverse e retirada após STOP;
- implementado: controles completos e patterns de 16 passos das virtual voices;
- implementado: comparador WAV para RMS, erro máximo, correlação e SNR;
- implementado: compensação explícita de latência no comparador WAV;
- implementado: guarda/teste headless Linux anterior à inicialização gráfica;
- implementado: medidores pós-MASTER e telemetria da gravação sem locks;
- implementado: telemetria atômica/destaque do passo do transporte;
- implementado: seleção PCM16/PCM24/float32 e diagnóstico ao finalizar REC;
- implementado: painel/persistência local de dispositivo, buffer e sample rate;
- implementado: render offline streaming de portable projects para PCM24;
- implementado: referências de source compatíveis com o Project v2 web;
- implementado: recarga restrita a WAVs relativos sob a pasta do projeto;
- implementado: métricas TRACK MASTER compatíveis com a estimativa web;
- implementado: planejamento ALBUM MASTER de trims, fades e gaps por frames;
- implementado: codec do manifesto ALBUM MASTER v1 e envelopes lineares;
- validado: manifesto real v0.28.1 com duas faixas e 408.000 frames;
- implementado: batch ALBUM MASTER PCM24 com preflight e publicação atômica;
- implementado: decoder PCM24 `WAVE_FORMAT_EXTENSIBLE`;
- validado: batch misto PCM16/PCM24 extensível com duas faixas reais;
- validado: analisador MASTER C++ executado sobre WAV real de 192.000 frames;
- implementado: cadeia TRACK MASTER inicial com EQ, dinâmica, saturação e width;
- validado: render TRACK MASTER atômico PCM24 de 176.400 frames;
- implementado: leitura/escrita segura da receita MASTER v1;
- implementado: assinatura dourada determinística exclusiva do TRACK MASTER;
- validado: render PCM24 real controlado por receita da interface web;
- validado: comparação objetiva da cadeia MASTER com WebAudio em 353.708
  frames, com diferenças de 0,176 dB peak, 0,136 LUFS e 0,0009 de correlação;
- pendente: aprovação auditiva humana da cadeia MASTER contra WebAudio;
- registrado separadamente: `AUDITORIA_ENGENHARIA_SAIDA_AUDIO.md`; a paridade
  acima não comprova lookahead/true peak ceiling, BS.1770, dither nem proteção
  do barramento ao vivo;
- validado: portable ZIP → render PCM24 estéreo de 12.000 frames;
- validado: stress DSP determinístico combinado de trinta segundos em blocos
  64/511, incluindo Assisted, FORM, TRACE, pitch, mixer e virtual voices;
- validado: soak virtual do mesmo cenário por 600 segundos em blocos 64/511,
  com resultado idêntico entre geometrias, transporte ativo e amostras finitas;
- validado: contratos do núcleo sob AddressSanitizer e
  UndefinedBehaviorSanitizer, sem erro de memória ou comportamento indefinido;
- limitação do ambiente: LeakSanitizer não pode executar sob o supervisor atual
  (`ptrace`) e, portanto, não foi contabilizado como validação de vazamentos;
- implementado: sincronização de comandos pendentes antes de salvar projetos;
- implementado: persistência Project v2 da base reversível e células MEMORY;
- implementado: controles nativos de MEMORY, MUTATION, EROSION, DECONSTRUCT,
  STUTTER, BURST, MICRO e REVERSE;
- implementado: modelo FORM Director com cinco cenas v0.28.1, edição limitada,
  lock, add/duplicate/delete/move, avanço por barras e teto de 16 cenas;
- implementado: persistência Project v2 interoperável de FORM Director;
- implementado: TRACE XY fixo de até 512 pontos, throttling de 38 ms,
  normalização BPM/pitch e persistência Project v2;
- implementado: reprodução TRACE LOOP sample-accurate, incluindo o intervalo
  de ciclo de 20 ms da referência e aplicação de BPM/Heritage Pitch;
- implementado: captura TRACE pela interface, throttling, clear e loop;
- implementado: contexto FORM → Assisted com as equações exatas de intensity
  e energy da v0.28.1;
- implementado: avanço FORM ao concluir frases de oito passos e troca
  automática dos bancos LONG/MEDIUM/SHORT/MICRO;
- implementado: edição FORM nativa de transição, bancos, seis dimensões,
  lock, add, duplicate, delete e move;
- implementado: nome de cena, undo/redo estrutural de 64 passos e captura
  explícita de bancos A/B com a mesma semântica da v0.25 Web;
- implementado: bancos nomeados LONG/MEDIUM/SHORT/MICRO/MANUAL/REGION por
  fonte, recall por cena e extensão opcional interoperável no Project v2;
- implementado: armazenamento textual fixo e histórico pré-alocado das cenas,
  sem alocação durante comandos no callback;
- implementado: planejador determinístico do Assisted Performer por frase;
- implementado: escolha de tempo dirigida por FORM, mutation/erosion/deconstruct,
  Heritage Pitch e STUTTER/BURST/REVERSE com probabilidades v0.28.1;
- implementado: execução Assisted no callback ao concluir cada frase;
- implementado: controles nativos de AUTO, vocabulário, BPM mínimo/máximo,
  variation, seed e rewind;
- implementado: persistência Project v2 das configurações do Assisted;
- implementado: importação explícita e recursiva de pastas de takes WAV,
  deduplicação por caminho, dados técnicos e leitura de tags RIFF disponíveis;
- implementado: preset persistente de metadados derivado do take selecionado e
  aplicado exclusivamente a gravações futuras;
- implementado: escrita posterior opcional de `RIFF LIST/INFO` a partir do
  catálogo, com confirmação, parcial validado, backup e troca recuperável;
- implementado: ALBUM PROJECT v1 persistente com deduplicação de takes, ordem,
  receipt/review, duração total, exportação e render direto pelo ALBUM MASTER;
- implementado: decisões Assisted de source e seleção de pattern;
- implementado: recombinação Assisted de pattern com reverse, interleave,
  substituição limitada, MEMORY e mínimo de células sonoras;
- implementado: region select/whole com redivisão 4/8/16/32/64;
- implementado: AUTO CUTS não destrutivo com nudge, micro, blade, undo e redivide;
- implementado: AUTO MIX conservador alterando apenas balance, pan e width;
- implementado: vocabulário completo do Assisted na interface e Project v2;
- implementado: oito slots nomeados de Motif Memory com CAPTURE, RECALL, VARY,
  DELETE, locks dimensionais e persistência interoperável Project/Portable v2;
- implementado: catálogo TAKE v1 persistente, limitado e validado;
- implementado: janela TAKE Timeline com status, rating, tags, notas e recipe;
- implementado: retorno não destrutivo de TAKE para SOURCE A/B;
- pendente: cursor Assisted exato na recipe e aceitação humana da escrita RIFF
  com cópias de takes reais;
- implementado: telemetria Assisted de source/pattern/mixer de volta à UI;
- validado parcialmente: ensaio visual/interativo do shell em sessão gráfica,
  incluindo waveform A/B, edição por arraste, TRACE XY, SOURCE MIXER lateral,
  métricas MASTER em lista, controles de estado consolidados e PERFORM;
- pendente: ensaio humano prolongado com áudio real e dois monitores;

Os três ensaios humanos restantes têm roteiro e critérios objetivos em
`FINAL_ACCEPTANCE_CHECKLIST.md`.

## Fonte e slices

- dois materiais A/B;
- oito slices contíguos por padrão;
- bancos entre 1 e 128 slices;
- limites normalizados entre 0 e 1;
- divisão da região em 4, 8, 16, 32 ou 64;
- edição independente de START e END;
- BLADE e undo de cortes;
- arquivos originais imutáveis.

## Sequencer

- dez patterns com oito células;
- 0–127 representa A;
- 128–255 representa B;
- 256 representa GAP;
- GRID, FREE e JITTER;
- STOP encerra todas as fontes e invalida agendamentos pendentes.

## Sinal

```text
SOURCE A/B players
→ SOURCE MIX
→ Heritage Pitch
→ MASTER
→ recorder/output
```

- níveis A/B de 0 a 1,25;
- pan de -1 a +1;
- width de 0 a 2;
- mute, solo e balance não destrutivos;
- rampas/declick equivalentes à referência;
- duas vozes virtuais com envelope, pitch, nível e pan.

## Estado

- Project v1 continua legível;
- Project v2 preserva todas as propriedades atuais;
- seeds do Assisted Performer permanecem determinísticos;
- áudio original não é incorporado no projeto leve;
- portable pack continua validado contra path traversal e arquivos indevidos.

## Janelas

- uma autoridade de áudio e estado;
- main: edição/composição;
- segundo monitor: PERFORM;
- MASTER: suplementar e fora da distribuição dos módulos;
- fechar uma janela remota não interrompe nem duplica o motor.

## Critério de substituição

A v0.28.1 só poderá ser substituída depois de:

1. testes unitários e WAVs dourados aprovados;
2. comparação auditiva aprovada;
3. gravação real prolongada sem xruns;
4. projetos v1/v2 abertos e salvos corretamente;
5. TRACK e ALBUM MASTER comparados;
6. teste real em dois monitores;
7. decisão de licença e distribuição registrada.
