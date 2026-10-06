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
- preparar builds Windows/macOS/Linux somente após aceitação humana;
- **tutorial guiado, sequencial** (pendência registrada em 18 ago. 2026,
  autor: "registre como uma pendencia do navalha"). Hoje só existe o
  LEARNING MODE (`UiHelp.h`, 120 entradas, 4 idiomas) - ajuda contextual
  por controle, ligada sob demanda, sem ordem nem narrativa. Não é a
  mesma coisa que uma janela de tutorial em capítulos com navegação
  anterior/próximo, tipo a do ANTITOTEM (`TutorialWindow`/
  `tutorialChapters`) - útil pra quem já sabe o que está procurando, não
  pra um primeiro contato guiado com o instrumento. Não é uma correção de
  nada desatualizado, é uma feature nova a ser avaliada, não teve escopo
  definido ainda (quantos capítulos, cobre só o painel principal ou
  também ALBUM PROJECT/FORM/TAKE Timeline, complementa ou substitui o
  Learning Mode).

## Lacunas de paridade ainda conhecidas

- validação humana da compatibilidade histórica `.nvl`/`.ptn`;
- tradução global de controles, mensagens e tooltips.

## 6 out. 2026 — distribuição multiplataforma (v0.1.1)

Aplica o padrão RASGO (`RASGO_DOCUMENTATION/PADRAO_DISTRIBUICAO_MULTIPLATAFORMA.md`).
O pedido do autor foi: "faça a mesma coisa no navalha 2 e no antitotem".

**Defeitos da v0.1.0**, achados ao inspecionar os pacotes publicados:

- o `.exe` dependia do Visual C++ Redistributable (`MSVCP140`, `VCRUNTIME140`)
  e não abre num Windows sem ele;
- o `.app` não era selado: só havia a assinatura do linker na fatia arm64.

**Correções** (branch `v0.1.1-distribuicao`):

- runtime estático, com uma guarda na CI;
- "Executar" no fim do instalador;
- `.zip` portátil;
- `.app` selado ad-hoc, com as duas fatias conferidas;
- AppImage e `.tar.gz` (`packaging/linux/`), com scripts genéricos idênticos
  aos do Antitotem. O binário "Navalha 2" tem espaço no nome, e o linuxdeploy
  não lia esse `Exec`; dentro do AppImage ele passa a se chamar `navalha2`;
- teste de abertura em Debian 12, Ubuntu 24.04, Fedora e Arch, conferindo
  que o processo está vivo e que a janela "Navalha 2" existe;
- release automática a partir de tags.

**Documentação:**

- `docs/INSTALLATION.md` com passo a passo em EN e PT e os requisitos. Os
  requisitos são valores de referência, não medidos;
- os guias de requisitos do Linux, nas 4 línguas, apontam para o AppImage;
- o site está pronto para a v0.1.1 e só vai ao ar com a versão final.

**Validação:**

- CI 37393157573 e a da release, todas verdes;
- pacotes finais inspecionados: sem DLL do Visual C++, `.app` com
  `_CodeSignature` e as duas fatias assinadas.

**Publicado:** `v0.1.1-rc1`, como pré-release.

**Pendente:**

1. o autor testar em Windows e macOS reais;
2. a v0.1.1 final: tag e merge no `main`, o que publica o site.
