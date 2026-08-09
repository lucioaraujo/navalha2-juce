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
- ALBUM PROJECT integrado ao Project v2 e ao Portable Project v2, preservando
  compatibilidade com projetos anteriores que não carregam álbum;
- SOURCE MIXER com modo BASIC persistente e ADVANCED sob demanda; pan, width e
  saída técnica deixam de sobrecarregar o fluxo comum sem serem removidos;
- workspaces EDIT, PLAY, COMPOSE e MIX agora filtram o canvas para a tarefa
  atual, sem duplicar sessão nem motor de áudio; PERFORM continua separado;
- o último diretório de REC é persistido, sem escolher automaticamente um
  arquivo ou enfraquecer a confirmação explícita do destino;
- pacote Debian interno gerável por CPack, com binário, atalho, ícone e
  dependências de runtime calculadas automaticamente;
- workflow GitHub Ubuntu 22.04 para gerar `.deb` amd64 mais compatível, com
  checkout fixado do JUCE 8.0.13 e da referência PD;
- 10/10 testes automatizados passando.

## Prioridade de hoje

1. Executar o roteiro humano com áudio real, acervo de takes e dois monitores,
   incluindo MASTER A/B/matching e escrita/recuperação RIFF.
2. Validar Portable Project v2 com um ZIP real produzido pelo JUCE.
3. Concluir revisão textual e traduções EN/PT/FR/ES.
4. Validar intercâmbio histórico `.nvl`/`.ptn` com arquivos reais, sem travar o
   restante do uso.
5. Enviar o `.deb` junto de `INSTALACAO_DEB_INTERNA.md` para a validação em
   outra máquina Linux amd64.

## Próximas etapas técnicas

- validar Portable Project v2 com material de uso;
- concluir revisão textual e traduções EN/PT/FR/ES;
- completar a auditoria de paridade PD→JUCE;
- testar takes, TRACK MASTER e ALBUM MASTER com áudio real;
- instalar e usar o `.deb` interno em uma sessão humana, quando conveniente;
- adicionar suporte FLAC sem perda para importação e exportação, após validar a
  cadeia de codec e os metadados no Linux;
- oferecer perfis explícitos de exportação de master: WAV PCM24 como padrão,
  WAV float32 para arquivo técnico e FLAC sem perda; manter MP3/OGG/AAC como
  formatos futuros de distribuição, não como master padrão;
- preparar builds Windows/macOS/Linux somente após aceitação humana.

## Lacunas de paridade ainda conhecidas

- validação humana da compatibilidade histórica `.nvl`/`.ptn`;
- tradução global de controles, mensagens e tooltips.
