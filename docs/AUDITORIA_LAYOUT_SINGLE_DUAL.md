# Auditoria de layout e acessibilidade — JUCE

Data: 3 de agosto de 2026<br>
Escopo: janela principal, modos single/dual e janelas suplementares do Navalha 2 JUCE.

## Resultado executivo

Os módulos principais estão presentes nos dois modos. O modo dual usa a área
visível dos monitores adequados e mantém a coluna contextual fixa; o modo single
usa a mesma hierarquia com rolagem. As janelas PERFORM, TAKES / MASTER,
TUTORIAL, ABOUT e AUDIO SETUP têm bounds próprios e não dependem da rolagem da
janela principal.

Foi corrigido um caso real de visibilidade: HERITAGE OFF e AUDITION não
recebiam novos bounds quando o layout single era criado diretamente. Agora são
posicionados explicitamente no cabeçalho single, evitando que fiquem ocultos ou
herdem a posição do modo dual.

## Janela principal

### Single view

- Cabeçalho fixo: EDIT / PREPARE, COMPOSE / FORM, PLAY / PERFORM,
  MIX / VOICES, TAKES / MASTER, DUAL MONITOR e utilitários.
- MIX / VOICES permanece disponível; no dual amplo é ocultado porque o bloco
  VOICES fica fixo na área principal.
- TOP / TRANSPORT, PREPARE / WAVEFORM, PERFORM / CREATE e VIRTUAL VOICES são
  empilhados verticalmente.
- Relógio, STOP, PLAY, REC e RESET permanecem na coluna contextual fixa.
- AUDIO CONNECTED, MASTER OUT, REC FORMAT e telemetria ficam na faixa superior
  de PREPARE / WAVEFORM.
- XY MOD, edição de slices, FORM, Assisted, Motif Memory, locks e voices
  recebem bounds na sequência rolável.
- SOURCE MIXER, Audio Library, Activity Log e LEARN permanecem acessíveis na
  coluna contextual.
- A rolagem continua necessária em resoluções menores e é intencional.

### Dual view

- PREPARE / WAVEFORM e CREATE / VOICES são distribuídos sem rolagem quando a
  área visível tem pelo menos 1400 × 850 px.
- O fallback volta automaticamente à rolagem quando a resolução não comporta
  a distribuição.
- A coluna contextual direita mantém Audio Library, Activity Log / LEARN,
  SOURCE MIXER, A/B BALANCE, MASTER CREATIVE e a saída técnica
  OUTPUT TRIM/MUTE com fundo neutro.
- O módulo CREATE / VOICES usa fundo vermelho translúcido; PREPARE / WAVEFORM
  usa amarelo translúcido; TOP / TRANSPORT usa cinza/steel.
- AUDIO CONNECTED, estado do motor, medidores, REC FORMAT e telemetria ocupam
  a faixa central do cabeçalho.
- HERITAGE OFF, PITCH 0 e AUDITION ficam juntos no bloco de pitch.
- XY MOD permanece visível na área de CREATE / VOICES.
- MIX / VOICES é ocultado no cabeçalho para não duplicar o bloco fixo de VOICES.

## Janelas suplementares

| Janela | Verificação |
|---|---|
| PERFORM | Transporte, source, pattern, gestos, FORM, XY MOD e estado compartilhado recebem bounds; XY MOD permanece visível mesmo no tamanho mínimo permitido. |
| TAKES / MASTER | Layout amplo divide TAKES e MASTER em 50/50; layout estreito usa abas TAKE TIMELINE / MASTERING. SEND TO MASTER conecta o take selecionado ao Master. |
| TUTORIAL | Sumário, capítulos, corpo de texto e PREVIOUS/NEXT têm bounds; navegação respeita os quatro idiomas. |
| ABOUT | Título, informações, status e licenças têm bounds e usam o mesmo LookAndFeel. |
| AUDIO SETUP | Cabeçalho, estado do dispositivo, seletor JUCE e nota de segurança têm bounds; o seletor nativo é a exceção deliberada de estilo. |

## Auditoria de funções e objetos

- Botões principais, toggles, ComboBox, sliders, biblioteca, edição de waveform,
  FORM, Assisted, Motif, mixer, voices, gravação, MASTER CREATIVE e OUTPUT
  TRIM/MUTE têm callbacks associados no construtor.
- O quadro XY MOD tem interação por arraste, gravação de trace, loop, clear e
  atualização compartilhada com PERFORM.
- Objetos ocultos de forma intencional:
  - LEARN, título e corpo do painel de aprendizagem, até LEARN ser ativado;
  - controles de TRACK MASTER ou ALBUM MASTER conforme o modo selecionado;
  - `transportInfo` no dual, pois a telemetria foi promovida ao cabeçalho;
  - MIX / VOICES no dual, pois VOICES já está fixo no layout;
  - janelas suplementares após serem fechadas.
- Não foram encontrados controles interativos sem callback ou componentes
  adicionados à hierarquia sem bounds após a correção do cabeçalho single.

## Limitações funcionais que não são falhas de layout

Continuam fora do escopo desta auditoria visual: BASIC/ADVANCED recolhível do
mixer, construtor de álbum a partir dos takes, importação histórica
`.nvl`/`.ptn`. O preview A/B e o matching de álbum foram incorporados depois
desta inspeção; compilam no layout atual, mas sua revisão visual e auditiva
continua no próximo ensaio humano.

## Validação

- Build local concluído.
- 9/9 testes CTest passaram.
- Verificação visual realizada em single, dual, PERFORM e TAKES / MASTER.

### Correções recentes do PERFORM

- `SOURCE A` e `SOURCE B` são controles independentes, com estado sincronizado
  com a janela principal;
- os gráficos `XY MOD` principal e PERFORM compartilham BPM e pitch;
- `COMMIT` e `RESTORE` informam quando não existe transformação ativa, evitando
  a aparência de botão sem função.
