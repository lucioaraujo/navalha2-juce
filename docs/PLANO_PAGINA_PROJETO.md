# Página pública do Navalha 2

## Objetivo

Criar uma página HTML pública para apresentar o Navalha 2 de forma conceitual,
histórica e técnica. A página deve explicar o projeto antes de apresentar os
controles, deixando claro que a versão JUCE é uma migração em desenvolvimento e
que a v0.28.1 Pure Data/web permanece a referência funcional.

## Estrutura editorial

1. **Cabeçalho**
   - nome Navalha 2;
   - subtítulo curto;
   - estado da versão;
   - links para `navalha2pd` e `navalha2juce`.

2. **Conceito**
   - Navalha como instrumento de recorte, recombinação e performance;
   - relação entre fonte, slice, padrão, gesto e forma;
   - diferença entre sampler tradicional e instrumento performático;
   - autoria original de Glerm Soares e natureza do upgrade Navalha 2.

3. **Histórico**
   - origem Pure Data/web;
   - referência v0.28.1;
   - separação PD/JUCE;
   - evolução da migração C++;
   - estado atual e próximos marcos.

4. **Visão geral de funcionamento**
   - SOURCE A/B;
   - waveform e slices;
   - GRID, FREE e JITTER;
   - padrões e macros;
   - Assisted Performer;
   - FORM, TRACE e XY MOD;
   - mixer, virtual voices e MASTER.

5. **Tutorial detalhado**
   - preparação do dispositivo de áudio;
   - carregamento e seleção de fontes;
   - edição não destrutiva;
   - criação de padrões;
   - performance e transformação;
   - gravação REC/STOP e limite de cinco minutos;
   - takes, TRACK MASTER e ALBUM MASTER;
   - single monitor e dual monitor.

6. **Ficha técnica por função**
   Para cada função: finalidade, controles, intervalo, unidade, estado inicial,
   dependências, resultado audível e limitações conhecidas.

7. **Galeria**
   - screenshots single monitor;
   - screenshots dual monitor;
   - PERFORM;
   - TAKES / MASTER;
   - AUDIO SETUP;
   - imagens com legendas e resolução informada.

8. **Arquitetura e ficha técnica do software**
   - JUCE/C++;
   - motor de áudio;
   - sample rate e formatos;
   - Project v2 e Portable Project;
   - sistemas previstos;
   - limites e requisitos.

9. **Licença, créditos e referências**
   - GPL-3.0-or-later do código Navalha 2;
   - notices de terceiros;
   - autoria original e upgrade;
   - referência Pure Data/web;
   - dependência JUCE e suas condições.

10. **Estado do projeto**
    - concluído;
    - em validação;
    - pendente;
    - roadmap multiplataforma e console físico futuro.

## Diretrizes de layout

- inglês como versão principal, com PT/FR/ES posteriormente;
- página responsiva para desktop, tablet e celular;
- identidade visual coerente com o Navalha 2, sem reproduzir a interface inteira
  como uma imagem;
- contraste alto, navegação por seções e sumário fixo;
- screenshots acompanhados de texto alternativo;
- código, comandos e nomes de controles preservados em fonte monoespaçada;
- nenhuma afirmação de paridade total enquanto houver itens parciais na auditoria.

## Materiais necessários

- screenshots finais single/dual e janelas suplementares;
- logotipo e favicon;
- versão e data da publicação;
- ficha de requisitos por sistema;
- textos conceituais aprovados;
- créditos e notices revisados;
- links definitivos dos dois repositórios públicos.

## Etapas

1. consolidar conteúdo conceitual e histórico;
2. fechar a ficha técnica e o tutorial;
3. selecionar e legendar screenshots;
4. criar o HTML/CSS responsivo em `docs/site/`;
5. revisar EN/PT/FR/ES;
6. testar links, acessibilidade e visualização em resoluções diferentes;
7. publicar junto com uma release estável.
