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
- importação de takes anteriores auditada e preset de metadados implementado;
- ALBUM PROJECT integrado ao catálogo, persistente, ordenável e renderizável;
- escrita RIFF posterior integrada à TAKE Timeline com confirmação, parcial
  validado e backup recuperável;
- TRACK MASTER A/B e matching relativo do ALBUM PROJECT implementados com
  estimativa interna explicitamente não certificada;
- 10/10 testes automatizados passando.

## Prioridade de hoje

1. Executar o roteiro humano com áudio real, acervo de takes e dois monitores,
   incluindo MASTER A/B/matching e escrita/recuperação RIFF.
2. Integrar o rascunho ALBUM PROJECT ao Project v2 artístico.
3. Projetar workspaces reais e mixer BASIC/ADVANCED.
4. Corrigir e validar Portable Project v2 com um ZIP real produzido pelo JUCE.

## Próximas etapas técnicas

- corrigir e validar Portable Project v2;
- concluir revisão textual e traduções EN/PT/FR/ES;
- completar a auditoria de paridade PD→JUCE;
- testar takes, TRACK MASTER e ALBUM MASTER com áudio real;
- gerar `.deb` para validação interna;
- preparar builds Windows/macOS/Linux somente após aceitação humana.

## Lacunas de paridade ainda conhecidas

- mixer BASIC/ADVANCED recolhível;
- compatibilidade histórica `.nvl`/`.ptn`;
- tradução global de controles, mensagens e tooltips.
