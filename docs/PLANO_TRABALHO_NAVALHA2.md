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
- 9/9 testes automatizados passando.

## Prioridade de hoje

1. Reiniciar o binário atual e validar visualmente o indicador de transporte
   e os botões Heritage/Audition em single monitor.
2. Testar PERFORM em dual monitor com SOURCE A/B carregados: PLAY, STOP, RESET,
   AUTO, REPEAT, macros, FORM e XY MOD.
3. Separar e corrigir as mensagens `COMMAND QUEUE FULL` e `INVALID VALUE`.
4. Repetir REC→STOP com sinal audível e confirmar o auto-stop de cinco minutos.
5. Registrar no relatório de validação os resultados e eventuais falhas.

## Próximas etapas técnicas

- corrigir e validar Portable Project v2;
- concluir revisão textual e traduções EN/PT/FR/ES;
- completar a auditoria de paridade PD→JUCE;
- testar takes, TRACK MASTER e ALBUM MASTER com áudio real;
- gerar `.deb` para validação interna;
- preparar builds Windows/macOS/Linux somente após aceitação humana.

## Lacunas de paridade ainda conhecidas

- playhead temporal na waveform;
- mixer BASIC/ADVANCED recolhível;
- FORM com nome, undo/redo e capture bank;
- importação de takes antigos;
- ALBUM PROJECT builder;
- preview A/B no painel MASTER;
- compatibilidade histórica `.nvl`/`.ptn`;
- tradução global de controles, mensagens e tooltips.
