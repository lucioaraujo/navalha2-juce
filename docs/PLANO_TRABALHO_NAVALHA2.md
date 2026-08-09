# Navalha 2 — plano de trabalho

Atualizado em 9 de agosto de 2026.

## Concluído recentemente

- layout single/dual auditado e compilado;
- `HERITAGE OFF` e `AUDITION` ampliados no single monitor;
- `SOURCE A` e `SOURCE B` independentes no PERFORM;
- sincronização dos gráficos `XY MOD` principal/PERFORM;
- auto-stop de gravação após cinco minutos;
- indicador renomeado para `TRANSPORT: STOP/PLAY`;
- fila de comandos ampliada para absorver rajadas válidas de macros e PERFORM;
- saída live-safe, dither TPDF PCM16/24 e site público inicial;
- playhead/readout temporal A/B ligado à telemetria real do motor;
- FORM Advanced concluído com nome, undo/redo e captura persistente A/B;
- 10/10 testes automatizados passando.

## Prioridade de hoje

1. Descobrir/importar takes anteriores e fechar preset/escrita RIFF posterior.
2. Criar ALBUM PROJECT builder integrado à TAKE Timeline.
3. Adicionar preview A/B no MASTER e fluxo de matching relativo.
4. Executar o roteiro humano com áudio real e dois monitores.

## Próximas etapas técnicas

- corrigir e validar Portable Project v2;
- concluir revisão textual e traduções EN/PT/FR/ES;
- completar a auditoria de paridade PD→JUCE;
- testar takes, TRACK MASTER e ALBUM MASTER com áudio real;
- gerar `.deb` para validação interna;
- preparar builds Windows/macOS/Linux somente após aceitação humana.

## Lacunas de paridade ainda conhecidas

- mixer BASIC/ADVANCED recolhível;
- importação de takes antigos;
- ALBUM PROJECT builder;
- preview A/B no painel MASTER;
- compatibilidade histórica `.nvl`/`.ptn`;
- tradução global de controles, mensagens e tooltips.
