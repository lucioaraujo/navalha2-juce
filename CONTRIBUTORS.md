# NAVALHA / NAVALHA 2 — Contributors

## Original project

**Glerm Soares** — author and developer of the original Navalha Pure Data instrument.

The Navalha 2 work preserves the original instrument's identity, legacy preset concepts and characteristic slicing/sequencing workflow while modernizing the runtime and interface.

## Navalha 2 upgrades

**Lúcio de Araújo** — conception and direction of the Navalha 2 upgrades; definition of desired musician-facing workflow; heritage-interface decisions; proposal of source-region slicing; hands-on Linux Mint / Pure Data testing; compatibility validation; iterative bug reports and musical evaluation.

**OpenAI / ChatGPT** — implementation assistance, code generation/refactoring, migration tooling, documentation, static validation and test-package preparation under Lúcio de Araújo's direction.

## Attribution principle

Navalha 2 documentation should distinguish clearly between:

- features inherited from Glerm Soares's original Navalha;
- compatibility/restoration work;
- newly proposed Navalha 2 functions and interface behavior.

No statement in this file changes the legal authorship or licensing status of the original project.

- Direção do modo **Viewport Fit / interface de tela única**: Lúcio de Araújo.

- Direção de **Heritage Pitch / 24 posições, modo AUDITION e comparação BYPASS–LEGACY**: Lúcio Araújo.
- Crédito de UI adotado: `upgrades 2026 · Lúcio Araújo`, mantendo Glerm Soares como autor do Navalha original.

- Direção da **Audio Library** como biblioteca de matéria sonora para corte — e não navegador/deck de DJ: Lúcio Araújo.
- Direção conceitual: futuras fontes A/B são matérias-primas de fragmentação e recombinação, não dual decks.
- Autorização de Glerm Soares para GPL-3.0-or-later registrada em julho de 2026.

- Prioridade de **Audio Stability / De-click** antes do Dual Source, a partir de cliques percebidos em uso: Lúcio Araújo.
- Regra preservada: corrigir descontinuidades no gesto/DSP, sem aplicar filtro destrutivo às fontes ou ao master.

- Direção para interface multilíngue EN/PT/FR/ES, inglês como padrão: Lúcio Araújo.
- Direção para tutorial didático HTML embutido durante o desenvolvimento e manual PDF somente na versão final: Lúcio Araújo.
- Forma oficial do cabeçalho/crédito Heritage adotada na v0.10.2 por decisão de Lúcio Araújo.

- Direção conceitual e de interface do **Dual Material**: duas sources como matérias sonoras para corte/recombinação, explicitamente não como decks de DJ — Lúcio Araújo.
- Direção de Cross-Source Patterns (`Axx/Bxx`) e GAP como evento musical — Lúcio Araújo.

- Direção v0.12: agrupar STUTTER, BURST, MICROSLICE e REVERSE SLICE na mesma versão: Lúcio Araújo.
- Direção de interface: destacar PLAY, STOP e REC por cores; REC permanece reservado até o Performance Recorder: Lúcio Araújo.

- Direção v0.13: tratar gesto físico e resposta imediata como princípio central do Navalha 2 enquanto instrumento: Lúcio Araújo.
- Direção de interface v0.13: evitar controles clicáveis pequenos ou escondidos; priorizar legibilidade e intuitividade, aceitando rolagem limitada em telas baixas: Lúcio Araújo.
- Ideia e direção do XY MOD para controle gestual simultâneo de BPM × pitch: Lúcio Araújo.

- Direção v0.14: TRACE/LOOP como memória de gesto de controle separada da futura gravação de áudio: Lúcio Araújo.
- Direção para ampliar a noção de instrumento através de gestos espaciais, estruturais e temporais: Lúcio Araújo.

- Direção v0.15: habilitar o REC e definir uma pasta separada/persistente para as performances: Lúcio Araújo.
- Direção v0.15: linha do tempo dos takes e preset/edição básica de metadados das gravações: Lúcio Araújo.
- Direção v0.15: manter explícita a escolha entre WAV 16-bit PCM, 24-bit PCM e 32-bit float: Lúcio Araújo.

- Direção v0.15.2: Modo Aprendizagem contextual e painel de explicação: Lúcio Araújo.
- Direção v0.15.2: terminal/log visível com comandos e eventos: Lúcio Araújo.
- Direção v0.15.2: módulos normal/minimizar/maximizar: Lúcio Araújo.
- Direção v0.15.2: Library/waveform menores, desktop sem scroll quando possível, mobile em paisagem e REC limitado a 5 minutos: Lúcio Araújo.

- Correção de interface v0.15.3: retirar OPEN WAV visível, impedir sobreposição dos controles de módulos e incluir restauração integral do layout: Lúcio Araújo.

- Direção v0.15.4: dividir a coluna esquerda entre Audio Library e Activity Log/Modo Aprendizagem: Lúcio Araújo.

- Direção v0.15.9: retorno a layout fixo, sem minimizar/maximizar/redimensionar módulos, priorizando clareza, equilíbrio e waveform suficientemente visível: Lúcio Araújo.

- Direção v0.16.0: layout standard fixo, waveform novamente ampla, sem ajustes de módulos e com margem inferior de segurança para painéis do Linux Mint: Lúcio Araújo.

- Direção v0.16.1: zoom 100% como referência oficial, cabeçalho e comandos superiores alinhados, interface sem scroll estrutural e waveform suficientemente visível: Lúcio Araújo.

- Direção v0.17.0: formalização Project v2 e Portable Pack v2 para SOURCE A/B, estado gestual e transporte não destrutivo das duas matérias sonoras: Lúcio Araújo.

- Direção v0.17.2: usar o mesmo botão REC para iniciar e finalizar a gravação, com retorno visual claro ao estado normal, e corrigir a permanência do Metadata Preset durante a edição: Lúcio Araújo.
- Direção visual v0.17.2–v0.17.3: aprovação do mascote original sem alteração artística, do letreiro pixelado exato com `NAVALHA` branco e `2` amarelo, da composição horizontal do cabeçalho e do uso exclusivo da cabeça no favicon e no ícone do aplicativo: Lúcio Araújo.

- Direção v0.17.3: integrar automaticamente os TAKEs finalizados à Audio Library, permitir o retorno não destrutivo de um TAKE a SOURCE A/B e ampliar a legibilidade geral sem reconstruir a geometria fixa da interface: Lúcio Araújo.

- Direção visual v0.17.4: adotar o estudo NAVALHA Arcade — preto, amarelo, aço e vermelho-sangue — na geometria fixa existente; ampliar ligeiramente apenas o letreiro aprovado em relação ao mascote; corrigir a nitidez do ícone da barra do Linux Mint sem redesenhar a cabeça: Lúcio Araújo.

- Validação e direção v0.17.5: ampliar um pouco mais o letreiro aprovado e
  separá-lo melhor do mascote; fazer o STOP encerrar também todo movimento e
  fluxo de eventos remanescente no Activity Log: Lúcio Araújo.

- Continuidade v0.17.6: transformar a lista de TAKEs em histórico persistente
  após reiniciar, mantendo o retorno não destrutivo das gravações a SOURCE A/B:
  desenvolvimento sob a direção de Lúcio Araújo.

- Concepção e direção v0.18.0: criar um performer automático assistido que
  produza decisões randômicas limitadas sobre velocidade, repetição, patterns,
  fragmentações e edição de slices, preservando a escolha humana das faixas e o
  controle exclusivo de PLAY, STOP e REC; incluir intervenção imediata e
  reversibilidade dos cortes automáticos: Lúcio Araújo.
