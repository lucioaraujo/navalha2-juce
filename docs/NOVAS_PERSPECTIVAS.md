# Navalha 2 — novas perspectivas

Documento de pesquisa e orientação para as próximas fases do projeto.

## 1. Propósito

O Navalha 2 não deve ser tratado apenas como uma migração de interface ou como
um sampler convencional. Ele pode evoluir como instrumento musical, ambiente de
performance, laboratório de escuta e objeto de pesquisa sobre tempo, recorte,
memória e recombinação sonora.

As próximas implementações devem ser avaliadas simultaneamente por quatro
critérios: qualidade musical, clareza de uso, consistência técnica e capacidade
de abrir novas práticas artísticas.

## 2. Créditos e referências

### Créditos a preservar

- **Glerm Soares** — autoria original do Navalha e da referência funcional
  Pure Data/web v0.28.1.
- **Lúcio Araújo** — Navalha 2, reescrita JUCE/C++, evolução do conceito,
  implementação, documentação e direção artística do upgrade.
- **JUCE** — framework utilizado na implementação C++; consultar e reproduzir
  corretamente os notices e condições da versão usada.

Créditos de técnicas, circuitos, algoritmos, instrumentos históricos, módulos e
obras de terceiros devem ser registrados individualmente em `CREDITS.md` e
`THIRD_PARTY_NOTICES.md`, nunca dissolvidos em uma lista genérica.

### Referências bibliográficas a desenvolver

A bibliografia final deve combinar fontes primárias e textos de apoio nas áreas:

- síntese sonora, sampling e música eletroacústica;
- composição algorítmica, acaso, repetição e processos;
- lutheria eletrônica e instrumentos musicais digitais;
- Pure Data, Max/MSP, SuperCollider e programação musical;
- percepção, psicoacústica, ritmo, gesto e espacialização;
- design de interação, visualização e acessibilidade;
- estética, filosofia da técnica, memória e arquivo;
- matemática aplicada a sinais, probabilidade, interpolação e sistemas discretos.

Cada referência deverá conter autor, título, edição, ano, editora ou URL, data
de acesso quando aplicável e uma nota explicando sua relação com o Navalha 2.

## 3. Territórios artísticos e musicais

### Explorações prioritárias

- composição a partir de microgestos, ruídos urbanos, respiração, voz e
  instrumentos acústicos;
- transformação de uma mesma fonte em múltiplas narrativas formais;
- diálogo entre improvisação humana e decisões limitadas do Assisted Performer;
- ritmos não ocidentais, assimétricos, polimétricos e flutuantes;
- continuidade entre ruído, textura, pulso, silêncio e evento;
- montagem de memória: takes, versões, retorno de material e esquecimento;
- performance multicanal e espacialização futura;
- uso pedagógico para explicar forma, timbre, tempo e edição não destrutiva.

### Critério de originalidade

Uma nova função deve ser considerada original quando não for apenas uma cópia de
um efeito conhecido, mas uma combinação coerente entre recorte, temporalidade,
gesto e decisão musical. Cada proposta deve responder:

1. que prática musical ela torna possível?
2. que escuta ela estimula?
3. que controle o músico realmente possui?
4. que surpresa ou indeterminação é preservada?
5. ela amplia a musicalidade ou apenas aumenta a quantidade de parâmetros?

## 4. Engenharia de áudio e sound design

- renderização e playback sample-accurate com menor custo de CPU;
- planejamento de vozes e alocação sem bloqueios;
- medição de xruns, latência, headroom e estabilidade por dispositivo;
- envelopes e crossfades adaptativos para evitar cliques;
- análise de transientes, densidade, energia e centroide espectral;
- waveshaping e degradação controlada como materiais composicionais;
- spatialização e roteamento modular em fase posterior;
- cache de waveforms e análise incremental para grandes bibliotecas;
- comparação objetiva entre render offline e saída realtime;
- presets de segurança para evitar clipping e perda de sinal.

## 5. Matemática e algoritmos

- estruturas de dados para slices e padrões com persistência reversível;
- geração determinística com seeds reproduzíveis;
- probabilidades condicionais ligadas à forma musical;
- interpolação temporal e mapeamento contínuo de gestos;
- modelos de energia, tensão, densidade e variação;
- métricas de similaridade entre takes sem apagar diferenças expressivas;
- otimização de busca em bibliotecas e pré-cálculo de waveforms;
- algoritmos que preservem limites musicais em vez de aleatoriedade sem direção;
- análise de custo computacional por voz, fonte e evento;
- testes property-based para limites, ranges e invariantes do motor.

## 6. Código, hardware e performance

- separar claramente thread de áudio, thread de interface e tarefas offline;
- manter o callback realtime sem alocação, locks ou I/O;
- medir antes de otimizar e registrar cada regressão;
- ampliar testes de stress para múltiplas taxas de amostragem e buffers;
- oferecer fallback para hardware mais lento;
- preparar uma futura arquitetura de console físico sem contaminar o desktop;
- definir interfaces de protocolo entre motor, controles e eventual hardware;
- documentar consumo de memória, CPU, latência e limites de gravação;
- manter builds reprodutíveis e artefatos identificáveis por versão.

## 7. Design ainda não explorado

- hierarquia visual que diferencie fonte, transformação, decisão e resultado;
- estados de controle mais evidentes sem excesso de cor ou ornamentação;
- histórico visual de gestos e transformações;
- comparação A/B e retorno seguro a estados anteriores;
- modos pedagógico, performático e técnico sem duplicar a interface;
- acessibilidade de contraste, foco, teclado e escalas de fonte;
- documentação contextual multilíngue sem interromper a performance;
- layouts responsivos para single monitor, dual monitor e futuro console;
- visualização de forma musical em vez de apenas valores numéricos;
- linguagem visual própria, reconhecível e distinta de DAWs tradicionais.

## 8. Filosofia, cultura e pedagogia

O Navalha 2 pode ser apresentado como uma ferramenta de relação com materiais,
não como uma máquina de resultados automáticos. A pedagogia deve mostrar que:

- cortar também é compor;
- repetir nunca significa repetir exatamente;
- limites podem produzir invenção;
- o acaso precisa de escuta e seleção;
- a tecnologia deve revelar decisões, não ocultá-las;
- a interface pode ensinar por meio do uso, do feedback e da comparação;
- a documentação deve acolher tanto o músico experiente quanto o estudante.

## 9. Tradução e comunicação

- inglês como versão principal de circulação internacional;
- português como versão autoral e de precisão conceitual;
- francês como versão cultural e de uso local;
- espanhol para ampliar o alcance latino e internacional;
- glossário fixo para termos como SOURCE, SLICE, TAKE, MASTER, FORM e GESTURE;
- revisão humana de conceitos, não apenas tradução literal de strings;
- exemplos musicais e capturas legendadas em todas as línguas;
- registro público de mudanças importantes e decisões de design.

## 10. Metodologia de trabalho

1. formular a hipótese musical ou técnica;
2. descrever o comportamento esperado antes do código;
3. implementar o menor protótipo verificável;
4. testar limites, musicalidade, custo e compreensão;
5. registrar decisões, falhas e alternativas descartadas;
6. validar com áudio real e, quando possível, escuta humana;
7. documentar em quatro línguas após estabilizar o conceito;
8. somente então incorporar a função ao roadmap público.

## 11. Atualizações e comunicação

- changelog por versão;
- relatório de paridade atualizado sem declarar funções ausentes como completas;
- notas de decisão para alterações conceituais importantes;
- exemplos de áudio e projetos de teste preservados;
- issues públicas classificadas por bug, paridade, design, documentação e ideia;
- releases somente quando houver critérios de aceitação registrados.

## 12. Regra de prioridade

Uma implementação nova deve subir no roadmap quando melhorar pelo menos dois
destes eixos sem prejudicar os demais: musicalidade, expressividade, clareza,
robustez, desempenho, acessibilidade e valor pedagógico.
