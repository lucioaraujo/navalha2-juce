# Navalha 2 JUCE — ALBUM PROJECT v0.1.0

## Escopo

O `ALBUM PROJECT` organiza takes do catálogo privado em uma sequência editorial
sem copiar, mover ou reescrever seus WAVs. Ele é distinto do `ALBUM MASTER`:
o projeto registra a curadoria; o master executa o processamento offline.

## Fluxo integrado

1. selecionar um take na TAKE Timeline;
2. usar `ADD TO ALBUM`;
3. editar título, artista e notas no modo `ALBUM MASTER`;
4. selecionar faixas e usar `MOVE UP`, `MOVE DOWN` ou `REMOVE`;
5. escolher `TARGET LUFS EST.` e usar `MATCH RELATIVE LEVELS` para analisar os
   takes e sugerir trims limitados a ±6 dB;
6. usar `EXPORT PROJECT` para publicar o manifesto editorial ou
   `RENDER PROJECT` para masterizar diretamente os WAVs ainda registrados no
   catálogo.

O mesmo take entra somente uma vez. O primeiro take pode sugerir o título do
álbum e o artista a partir dos próprios metadados. A soma exibida corresponde à
duração original das faixas, sem incluir os gaps do master.

## Persistência e formato

O rascunho persiste nas preferências privadas do aplicativo. A exportação usa
`navalha-album-project` v1, compatível conceitualmente com a referência Web, e
preserva:

- ordem e identidade do take;
- título, nome de arquivo, duração e status;
- review, rating, tags e notas;
- recipe JSON quando disponível;
- parâmetros de trim, gap e fades preparados para o fluxo de master;
- análise interna opcional e trim resultante do matching relativo.

O manifesto é limitado a 99 faixas e 8 MiB. Textos e recipes passam pelos
mesmos limites defensivos do catálogo. Versões desconhecidas, números não
finitos e estruturas incompletas são rejeitados.

## Relação com ALBUM MASTER

`RENDER PROJECT` resolve cada faixa pelo identificador do take e exige que o
WAV ainda exista no caminho catalogado. O renderizador faz preflight de todas
as fontes e saídas antes do batch PCM24. O projeto e os WAVs permanecem
inalterados.

`LOAD MANIFEST` e `RENDER MANIFEST` continuam aceitando o formato independente
`navalha-album-master` v1 com referências relativas. Assim, o fluxo histórico
por manifesto não depende do catálogo privado, enquanto o novo fluxo integrado
não exige relocalizar gravações já existentes.

## Limitações conhecidas

- o rascunho privado ainda não integra o arquivo artístico Project v2;
- não há importação de um `navalha-album-project` exportado pela interface;
- trims, gaps e fades usam os valores preservados pelo modelo, mas ainda não
  possuem edição manual por faixa no builder JUCE; o trim pode ser calculado
  coletivamente pelo matching relativo;
- a aprovação auditiva de um álbum real permanece obrigatoriamente humana.
