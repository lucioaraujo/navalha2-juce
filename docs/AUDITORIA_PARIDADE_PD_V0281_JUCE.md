# Auditoria de paridade PD/Web v0.28.1 → JUCE/C++

Data: 31 de julho de 2026  
Referência: `navalha_app_project_v2_v0.28.1.zip`  
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

Bloqueadores reais para declarar paridade funcional completa:

1. TAKE Timeline, metadados, classificação, rating, tags e recipe por take;
2. construtor de ALBUM PROJECT a partir dos takes;
3. importação/exportação histórica `.nvl`/`.ptn`;
4. pré-escuta independente da Audio Library;
5. completar FORM Advanced com nome, undo/redo e captura explícita dos bancos;
6. preview/audição dentro de MASTER e matching relativo do fluxo v0.28;
7. validações humanas de áudio real, dois monitores e sessão prolongada.

## Matriz funcional

| Área | Estado | Evidência / diferença |
|---|---|---|
| SOURCE A/B independentes | Completo | buffers, bancos, arquivos e ondas independentes |
| WAV/AIFF | Completo | WAV nativo; AIFF convertido sem alterar o original |
| Audio Library: pasta, busca e drag | Completo | seletor gráfico, filtro, LOAD A/B e drag para cada onda |
| Audio Library: preview e STOP PREVIEW | Ausente | seleção informa o arquivo, mas não há pré-escuta independente |
| Região por arraste na waveform | Completo | restaurado conforme os handlers da v0.28.1 |
| EDIT SLICE por arraste + SET | Completo | faixa selecionada visualmente e commit explícito |
| BLADE por clique e UNDO | Completo | corte direto na onda, não destrutivo |
| DIVIDE 4/8/16/32/64 | Completo | botões explícitos imediatamente sob a waveform |
| Duas ondas simultâneas | Completo | A em cima, B embaixo; divisor vertical ajustável |
| Playhead e readout temporal sobre a onda | Ausente | limites normalizados existem; falta cursor/tempo em reprodução |
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
| REC pós-MASTER 16/24/float | Completo | writer assíncrono, limites, publicação atômica e telemetria |
| Pasta padrão de gravação persistente | Parcial | usuário escolhe o WAV ao iniciar cada REC |
| Metadados de gravação/preset | Ausente | writer suporta tags, mas não há editor/preset no shell |
| TAKE Timeline | Ausente | sem histórico visual ou seleção de takes |
| Review/status/rating/tags | Ausente | estados EXPERIMENT…MASTER não foram modelados na UI JUCE |
| Recipe JSON por TAKE | Ausente | receitas MASTER existem; receita reproduzível de performance não |
| TAKE → SOURCE A/B | Ausente | é possível carregar WAV manualmente, sem fluxo/cópia de trabalho de TAKE |
| ALBUM PROJECT builder | Ausente | não há ordenação de takes e export de manifesto a partir da main |
| ALBUM MASTER por manifesto | Completo | load, preflight, gaps, fades, trims e batch PCM24 |
| TRACK MASTER | Completo técnico | análise, receita e render; aprovação auditiva ainda pendente |
| MASTER preview/audition | Ausente | processamento é analyze/render, sem reprodução A/B no painel |
| Matching relativo de álbum | Parcial | manifesto aceita trim/análise; falta o fluxo interativo da v0.28 |
| Project v2 leve | Completo | referências seguras, estado e recarga |
| Portable Project v2 | Completo | ZIP validado com cópia de SOURCE A/B |
| Migração Project JSON v1 | Completo | fixtures e CTest |
| Import/export `.nvl`/`.ptn` | Ausente | Project v1 JSON não substitui os formatos históricos |
| STORE/EXPORT legado | Ausente | edição atual é imediata, mas não há compatibilidade documental |
| Janela PERFORM com um motor | Completo | transporte, source, REC, macros, gestos, FORM, XY e estado compartilhado |
| Pop-out de qualquer módulo | Parcial | PERFORM e MASTER têm janelas; módulos arbitrários não |
| Workspaces PLAY/EDIT/COMPOSE | Parcial | navegação posiciona módulos; não isola completamente cada contexto |
| Segundo monitor adaptativo | Parcial | janela única, redimensionável e auto-posicionada; falta escala explícita 100–145% |
| Tutorial, LEARN e ajuda contextual | Ausente | documentação externa existe; não há ajuda embutida |
| Idiomas PT/EN/FR/ES | Específico Web, ausente | shell nativo usa inglês |
| APP info/paths/diagnóstico | Parcial | Audio Setup e status existem; falta painel consolidado |

## Auditoria de layout e usabilidade

### EDIT / PREPARE

- Biblioteca, arquivo selecionado e log formam uma coluna contextual à direita;
  o SOURCE MIXER continua essa coluna imediatamente abaixo do log.
- As barras coloridas à esquerda agora identificam os módulos.
- `AUDIO CONNECTED/DISCONNECTED` voltou como indicador clicável para Audio Setup.
- O botão `AUDIO SETUP` duplicado foi removido; o próprio indicador abre a
  configuração.
- `BYPASS/LEGACY` tornou-se um único `HERITAGE ON/OFF`; o slider permanece para
  misturas intermediárias.
- MASTER OUT e REC FORMAT ficam acima da waveform.
- Sob a waveform ficam apenas seleção, BLADE, WHOLE, UNDO e divisões.
- SOURCE A/B são simultâneas e o operador pode ampliar uma delas.
- Falta adicionar playhead/tempo e pré-escuta da biblioteca.

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

## Ordem recomendada para concluir

1. TAKE Timeline + metadados + recipe + TAKE → SOURCE.
2. ALBUM PROJECT builder integrado aos takes.
3. Workspaces reais e mixer BASIC/ADVANCED.
4. Compatibilidade `.nvl`/`.ptn`.
5. Preview da Library, playhead temporal e preview MASTER.
6. FORM undo/redo/nome/capture bank.
7. Ajuda embutida e idiomas.
8. Aceitação auditiva, dois monitores e soak real.

Nenhum item marcado como ausente deve ser descrito como implementado em
`PARIDADE_V0281.md` até existir no núcleo e estar acessível na interface.
