# FORM Advanced — contrato JUCE/C++

Atualizado em 9 de agosto de 2026.

O editor nativo cobre o contrato funcional da v0.25 Web sem criar um segundo
estado musical. A interface e o `AudioEngine` mantêm diretores FORM espelhados
pela fila SPSC; o callback nunca lê componentes JUCE.

O nome é metadado editorial: permanece no diretor da interface e segue para
Project/Portable/recipe. Um checkpoint compacto, sem transportar texto pela
fila realtime, mantém o histórico estrutural do motor alinhado para undo/redo.

## Cenas

- uma a 16 cenas, com nome de até 36 caracteres;
- bars, energy, variation, transition e seis macros composicionais;
- perfis A/B `WORKING`, `LONG`, `MEDIUM`, `SHORT`, `MICRO`, `MANUAL` e
  `REGION`;
- lock, add, copy, delete e deslocamento para a esquerda/direita;
- undo/redo estrutural limitado aos 64 estados mais recentes.

O histórico registra operações estruturais, lock, nome e mudanças discretas
de transição/perfil. Movimentos contínuos dos sliders não geram dezenas de
entradas enquanto o usuário arrasta. O armazenamento do histórico é reservado
na construção do diretor; undo, redo e edição não alocam memória no callback.

## CAPTURE A/B

Cada fonte possui um conjunto próprio de bancos nomeados. `CAPTURE A` ou
`CAPTURE B` copia o banco de slices de trabalho para o perfil selecionado pela
cena. Se o perfil for `WORKING`, a captura usa `MANUAL`, como na v0.28.1.

Ao entrar numa cena, o perfil nomeado é recuperado. LONG/MEDIUM/SHORT/MICRO só
são gerados automaticamente na primeira utilização; uma captura posterior
substitui de fato esse conteúdo. MANUAL e REGION preservam a fotografia
explícita dos slices.

## Persistência

Project v2 e Portable Project continuam com `version: 2`. A extensão opcional
fica dentro de `sources.A/B.slicing`:

```json
{
  "activeSliceBank": "MANUAL",
  "sliceBanks": {
    "LONG": [[0.0, 0.25], [0.25, 0.5]],
    "MANUAL": [[0.1, 0.24], [0.4, 0.72]]
  }
}
```

Leitores antigos podem ignorar os campos. O banco operacional continua em
`slicing.slices`, preservando compatibilidade reversa. Nome de cena, estrutura
FORM e bancos nomeados também entram na recipe de TAKE.

## Verificação automatizada

Os testes cobrem normalização de nome, undo/redo, comandos realtime de nome e
undo, captura WORKING→MANUAL, substituição/recall de banco e round-trip JSON
dos bancos nomeados no Project v2.
