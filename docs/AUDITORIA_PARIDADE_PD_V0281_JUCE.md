# Auditoria de paridade PD/Web v0.28.1 → JUCE/C++

Data: 3 de agosto de 2026<br>
Atualização da matriz: 9 de agosto de 2026<br>
Referência: `navalha_app_project_v2_v0.28.1.zip`<br>
Escopo: código PD, interface Web v0.28.1, documentação funcional e candidato
JUCE/C++ atual.

## Critério

- **Completo**: existe no núcleo C++ e está acessível pela interface nativa.
- **Parcial**: o núcleo ou uma parte da interface existe, mas falta uma etapa
  funcional presente na v0.28.1.
- **Ausente**: não há implementação equivalente utilizável no candidato JUCE.
- **Específico Web**: conveniência do shell Web, sem impacto direto no motor de
  áudio; pode ser redesenhada nativamente em vez de copiada.

## Resumo executivo

O motor de performance, edição não destrutiva, FORM, Assisted, mix, vozes,
gravação, Motif Memory e MASTER já foi majoritariamente transposto. As lacunas
maiores não estão no callback de áudio; estão na gestão do trabalho produzido
e em algumas conveniências de operação.

A interface JUCE agora também contém o conjunto utilitário fixo `LANG`,
`? TUTORIAL`, `ABOUT` e `LEARN`, a distribuição adaptativa real em dois
monitores e o fallback rolável para telas menores. Isso reduz lacunas de
operação, mas não transforma esta auditoria em declaração de paridade total:
o tutorial nativo é uma síntese dos dez capítulos e a tradução integral de
todos os rótulos do instrumento ainda não foi feita.

Bloqueadores reais para declarar paridade funcional completa, após a primeira
etapa da TAKE Timeline em JUCE v0.1.0:

1. descoberta/importação de takes anteriores e preset/escrita RIFF de metadados;
2. construtor de ALBUM PROJECT a partir dos takes;
3. importação/exportação histórica `.nvl`/`.ptn`;
4. completar FORM Advanced com nome, undo/redo e captura explícita dos bancos;
5. preview/audição dentro de MASTER e matching relativo do fluxo v0.28;
6. validações humanas de áudio real, dois monitores e sessão prolongada.

Contagem não ponderada da matriz nesta atualização: 33 áreas completas,
completas técnicas ou implementadas no fluxo equivalente; 12 parciais; 6
ausentes; e uma conveniência Web substituída deliberadamente por comportamento
nativo. Os estados “completo técnico” continuam exigindo aceitação humana onde
indicado e não autorizam declarar substituição integral do produto.

## Matriz funcional

| Área | Estado | Evidência / diferença |
|---|---|---|
| SOURCE A/B independentes | Completo | buffers, bancos, arquivos e ondas independentes |
| WAV/AIFF | Completo | WAV nativo; AIFF convertido sem alterar o original |
| Audio Library: pasta, busca e drag | Completo | seletor gráfico, filtro, LOAD A/B e drag para cada onda |
| Audio Library: preview e STOP PREVIEW | Completo técnico | PREVIEW/STOP e duplo clique reproduzem WAV/AIFF de forma independente a 70%, sem carregar SOURCE A/B; requer aceitação auditiva humana |
| Região por arraste na waveform | Completo | restaurado conforme os handlers da v0.28.1 |
| EDIT SLICE por arraste + SET | Completo | faixa selecionada visualmente e commit explícito |
| BLADE por clique e UNDO | Completo | corte direto na onda, não destrutivo |
| DIVIDE 4/8/16/32/64 | Completo | botões explícitos imediatamente sob a waveform |
| Duas ondas simultâneas | Completo | A em cima, B embaixo; divisor vertical ajustável |
| Playhead e readout temporal sobre a onda | Completo | telemetria lock-free publica a posição A/B mais recente do player principal; cursores e tempo atual/duração aparecem nas duas ondas, avançam também em reverse e somem ao terminar/STOP |
| Patterns 10 × 8 e códigos A/B/GAP | Completo | edição, macros e persistência |
| GRID/FREE/JITTER + seed | Completo | scheduler por amostras e controles nativos |
| STUTTER/BURST/MICRO/REVERSE | Completo | núcleo realtime e acesso main/PERFORM |
| MEMORY/MUTATION/EROSION/DECONSTRUCT | Completo | base reversível, commit/restore e persistência |
| TRACE XY/LOOP | Completo | quadro main interativo, caminho visível, captura ao arrastar, clear, loop e Project v2 |
| XY BPM × PITCH | Completo | main/PERFORM compartilham a mesma sessão |
| Heritage Pitch | Completo técnico | ainda requer aprovação auditiva humana |
| Source Mixer A/B | Completo | painel vertical sob ACTIVITY LOG; level, pan, width, mute, solo e balance |
| Mixer BASIC/ADVANCED recolhível | Ausente | todos os controles continuam visíveis na main |
| AUTO MIX conservador | Completo no Assisted | não há botão BASIC separado como no Web |
| Duas Virtual Voices | Completo | source, divisão, foco, pattern, pitch, envelope, level e pan |
| FORM básico | Completo | arm, hold, next, reset, barras, energia e variação |
| FORM Advanced: cenas/macros | Parcial | transição, bancos, dimensões, lock/add/copy/delete/move existem |
| FORM nome/undo/redo/capture bank | Ausente | funções presentes no diálogo Web não aparecem no JUCE |
| Assisted Performer | Completo | vocabulário, BPM, variation, seed, rewind, next/keep/restore |
| Motif locks | Completo | oito categorias persistidas e respeitadas pelo Assisted |
| Oito slots de Motif Memory | Completo | snapshot nomeado, CAPTURE/RECALL/VARY/DELETE e Project/Portable v2 |
| REC pós-MASTER 16/24/float | Completo técnico | writer assíncrono, publicação atômica, telemetria e auto-stop de 5 minutos; falta validação humana com sinal audível |
| Pasta padrão de gravação persistente | Parcial | usuário escolhe o WAV ao iniciar cada REC |
| Metadados de gravação/preset | Parcial | catálogo edita título/artista/projeto/ano/comentário; falta preset e escrita RIFF posterior |
| TAKE Timeline | Parcial | novos WAVs finalizados entram no catálogo persistente e janela nativa; falta descobrir/importar gravações anteriores |
| Review/status/rating/tags | Completo | estados EXPERIMENT…MASTER, rating 0–5, tags e notas persistem no catálogo privado |
| Recipe JSON por TAKE | Parcial | snapshot pré-REC exportável; cursor interno Assisted é declarado indisponível |
| TAKE → SOURCE A/B | Completo | recarrega o WAV pelo decoder seguro sem alterar o take original |
| ALBUM PROJECT builder | Ausente | não há ordenação de takes e export de manifesto a partir da main |
| ALBUM MASTER por manifesto | Completo | load, preflight, gaps, fades, trims e batch PCM24 |
| TRACK MASTER | Completo técnico | análise, receita e render; aprovação auditiva ainda pendente |
| MASTER preview/audition | Ausente | processamento é analyze/render, sem reprodução A/B no painel |
| Matching relativo de álbum | Parcial | manifesto aceita trim/análise; falta o fluxo interativo da v0.28 |
| Project v2 leve | Completo | referências seguras, estado e recarga |
| Portable Project v2 | Parcial | serialização e cópia de SOURCE A/B existem; falta validar a abertura de um ZIP produzido pelo próprio JUCE em condições reais |
| Migração Project JSON v1 | Completo | fixtures e CTest |
| Import/export `.nvl`/`.ptn` | Ausente | Project v1 JSON não substitui os formatos históricos |
| STORE/EXPORT legado | Ausente | edição atual é imediata, mas não há compatibilidade documental |
| Janela PERFORM com um motor | Completo | transporte, source, REC, macros, gestos, FORM, XY e estado compartilhado |
| Pop-out de qualquer módulo | Parcial | PERFORM e MASTER têm janelas; módulos arbitrários não |
| Workspaces PLAY/EDIT/COMPOSE | Parcial | navegação posiciona módulos; não isola completamente cada contexto |
| Segundo monitor adaptativo | Parcial validado visualmente | distribuição automática sem rolagem em duas telas adequadas, janela PERFORM no monitor secundário, preferência persistente e fallback rolável; falta ensaio humano prolongado |
| VIEW 100/115/130/145 do Web | Específico Web, não adotado | removido por decisão de UX: redimensionamento JUCE, DPI do sistema, layout dual e scrollbar substituem o zoom interno sem criar combinações ambíguas |
| Tutorial, LEARN e ajuda contextual | Parcial | tutorial nativo com 10 capítulos e LEARN persistente por mouse/foco no painel fixo; cobertura JUCE é ampla, mas ainda não replica os 106 tópicos do Web |
| Idiomas PT/EN/FR/ES | Parcial | seletor persistente; tutorial e LEARN têm quatro idiomas; tradução global de controles, mensagens, tooltips e revisão textual ainda pendentes |
| ABOUT | Completo de interface | janela Arcade nativa contém versão, referência, autoria, licenças e aviso de paridade parcial; diagnóstico permanece corretamente separado |

## Auditoria de layout e usabilidade

### EDIT / PREPARE

- Biblioteca, arquivo selecionado e log formam uma coluna contextual à direita;
  o SOURCE MIXER continua essa coluna imediatamente abaixo do log.
- O relógio e `STOP / PLAY / REC / RESET` iniciam essa coluna logo abaixo da
  navegação principal; os quatro comandos têm a mesma largura e permanecem
  visíveis independentemente da rolagem.
- Toda essa coluna contextual usa fundo preto uniforme; o vermelho fica
  restrito ao módulo `PERFORM / CREATE` e à sua aba lateral.
- O último módulo termina a 4 px do rodapé; foi removida a faixa preta vazia
  que antes separava excessivamente o conteúdo da linha de créditos.
- A busca da biblioteca foi renomeada de `FILTER FILES` para `SEARCH FILES`
  (`BUSCAR ARQUIVOS` em português), e o LOG recebeu mais altura, inclusive
  quando o painel LEARN está ativo.
- A coluna foi ampliada para 340 px. Os nomes da Audio Library mantêm tamanho
  legível com reticências, e a faixa estreita à direita indica que toda a linha
  pode ser arrastada sem ocupar o espaço antes consumido pelo botão `DRAG`.
- O SOURCE MIXER usa exatamente a altura de seus controles e permanece colado
  à base, liberando 24 px adicionais para biblioteca e LOG.
- As barras coloridas à esquerda agora identificam os módulos.
- `AUDIO CONNECTED/DISCONNECTED` voltou como indicador clicável para Audio Setup.
- O botão `AUDIO SETUP` duplicado foi removido; o próprio indicador abre a
  configuração.
- `BYPASS/LEGACY` tornou-se um único `HERITAGE ON/OFF`; o slider permanece para
  misturas intermediárias.
- Em layout dual amplo, `AUDIO CONNECTED`, estado do motor, `MASTER OUT`,
  medidores, `REC FORMAT` e telemetria ocupam o centro livre do cabeçalho.
- Nesse layout, JITTER e TIMING SEED compartilham a linha de tempo; no fallback
  estreito continuam em uma segunda linha para evitar cortes.
- Em single monitor, `HERITAGE ON/OFF` e `AUDITION` aproveitam a linha superior.
  Em dual monitor, permanecem junto ao bloco de pitch porque o cabeçalho abriga
  estado de áudio, medidores e gravação. `PITCH 0` fica junto ao valor nos dois.
- No fallback de tela menor, essas duas linhas continuam imediatamente acima
  da waveform para não comprimir os botões de projeto e transporte.
- Sob a waveform ficam apenas seleção, BLADE, WHOLE, UNDO e divisões.
- SOURCE A/B são simultâneas e o operador pode ampliar uma delas.
- Playhead/tempo e pré-escuta independente da biblioteca estão transpostos; a
  aceitação auditiva da pré-escuta permanece humana.

### PERFORM

- A janela deixou de ser um grande XY quase vazio.
- Controles de alta frequência ficam no topo: STOP/PLAY/RESET/REC, SOURCE A/B,
  AUTO/REPEAT/NEXT.
- SOURCE A/B usa um único botão de estado que alterna a fonte ativa.
- Ordem de pattern e gestos instantâneos têm linhas próprias.
- FORM ao vivo está próximo dos gestos e não do editor avançado.
- O XY continua grande o suficiente para gesto contínuo.
- O conteúdo pesado da janela é pré-carregado sem criar uma janela visível;
  a primeira abertura medida ficou em aproximadamente 316 ms.
- LEVEL/MUTE/SOLO permanecem decisões humanas; uma futura camada BASIC/ADVANCED
  reduzirá a densidade da main.

### Auditoria de controles com dupla função

Consolidados:

- `REC` ↔ `STOP REC`;
- `MEMORY` ↔ `MEMORY ON`;
- `ARM FORM` ↔ `FORM ON`, `HOLD` ↔ `RELEASE` e `LOCK` ↔ `UNLOCK`;
- `RECORD TRACE` ↔ `STOP TRACE` e `TRACE LOOP` ↔ `STOP LOOP`;
- `HERITAGE OFF` ↔ `HERITAGE ON`;
- `SOURCE A` ↔ `SOURCE B` na janela PERFORM;
- `AUDIO CONNECTED/DISCONNECTED` também abre Audio Setup.

Mantidos separados por segurança ou por não representarem um estado binário:

- `STOP` e `PLAY`: STOP deve continuar sempre visível e previsível ao vivo;
- `COMMIT` e `RESTORE`: ações sobre snapshots, não liga/desliga;
- `MUTE` e `SOLO`: estados de mixagem independentes entre canais;
- `SELECT REGION`, `EDIT SLICE` e `BLADE`: modos exclusivos, mas precisam de
  acesso direto junto à onda;
- `CAPTURE`, `RECALL`, `VARY` e `DELETE`: operações distintas de Motif Memory.

### COMPOSE / FORM / MIX / MASTER

- A main ainda mostra módulos demais num único documento rolável.
- TRACE XY voltou a ter superfície direta na main e grava do pressionar ao soltar.
- As métricas TRACK MASTER usam lista de seis linhas com valores destacados,
  em vez de texto monoespaçado solto.
- Próximo passo recomendado: workspaces verdadeiros que mostrem somente:
  - EDIT: library, waveform, slices e output;
  - PLAY: transporte, pattern, mix essencial e gestos;
  - COMPOSE: FORM, Assisted avançado, motif, voices e takes;
  - MASTER: janela separada.

### Cabeçalho, ajuda e tipografia

- O ZIP oficial da v0.28.1 foi inspecionado diretamente. Seu `index.html`
  contém 129 botões com `id`, 106 chaves `data-learn`, quatro idiomas e dez
  capítulos de tutorial; a captura de tela não foi usada como único contrato.
- O cabeçalho fixo JUCE contém os sete acessos de workspace/janelas e, no mesmo
  nível, o grupo utilitário `LANG`, `? TUTORIAL`, `ABOUT` e `LEARN`.
- LEARN não usa um tooltip flutuante: a explicação aparece dentro do painel
  fixo `ACTIVITY LOG`, como no desenho PD, e responde a ponteiro e foco.
- O módulo vermelho usa fundo vermelho translúcido de 11% até encostar
  exatamente na borda esquerda da coluna contextual. `ACTIVITY LOG / LEARN`,
  `SOURCE MIXER` e Audio Library permanecem neutros; a antiga aba cinza
  vertical do mixer foi removida.
- `A/B BALANCE` e `MASTER OUTPUT` usam alinhamento central na coluna de saída
  global; os canais SOURCE A/B permanecem laterais.
- A família tipográfica foi unificada em `DejaVu Sans Mono`, incluindo botões,
  labels, ComboBox e menus. Pesos/tamanhos variam apenas por hierarquia; textos
  auxiliares compactos permanecem menores para caber no layout dual.
- O rodapé foi reduzido a 16 px e explicita a autoria sem usar “Arcade” como
  nome do projeto: NAVALHA por Glerm Soares (2009), NAVALHA 2 como upgrade por
  Lúcio Araújo (2026).
- Toda janela interna usa o mesmo LookAndFeel: main, PERFORM, TAKE, MASTER,
  TUTORIAL, ABOUT e AUDIO SETUP. Avisos operacionais aparecem no painel de
  status/log em vez de alertas genéricos. Seletores de arquivo e pasta são a
  única exceção deliberada: continuam nativos do sistema por segurança,
  acessibilidade e integração com permissões.

## Ordem recomendada para concluir

1. Importação/descoberta de takes anteriores, preset e escrita RIFF opcional.
2. ALBUM PROJECT builder integrado aos takes.
3. Workspaces reais e mixer BASIC/ADVANCED.
4. Compatibilidade `.nvl`/`.ptn`.
5. Preview MASTER; playhead temporal e pré-escuta da Library já foram transpostos.
6. FORM undo/redo/nome/capture bank.
7. Completar tradução global e ampliar LEARN dos grupos nativos até a
   granularidade dos 106 tópicos Web quando houver controle equivalente.
8. Aceitação auditiva com sinal real, incluindo o auto-stop de 5 minutos,
   dois monitores e soak prolongado.
9. Gerar `.deb` para validação interna; somente depois preparar publicação
   multiplataforma.

Nenhum item marcado como ausente deve ser descrito como implementado em
`PARIDADE_V0281.md` até existir no núcleo e estar acessível na interface.
