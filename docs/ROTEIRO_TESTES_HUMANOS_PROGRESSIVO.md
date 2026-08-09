# Roteiro de testes humanos progressivos

Este roteiro transforma a aceitação humana do Navalha 2 JUCE em acompanhamento
contínuo. Ele **não bloqueia o avanço do desenvolvimento**: a automação de core,
render, stress, gravação e true peak já passou em 2026-08-09. Cada item abaixo
serve para registrar uso real e encontrar regressões concretas quando houver
tempo, não para impedir novos incrementos por falta de uma sessão única longa.

Estado inicial: uso prévio do usuário não revelou incômodo audível.

## Como registrar uma ocorrência

Para cada sessão, anotar somente o que for relevante:

- data, sistema, dispositivo, sample rate e buffer;
- WAV/projeto usado;
- funções exercitadas;
- resultado (`ok`) ou falha, com a menor sequência que a reproduz;
- se a falha é audível, visual, de fluxo ou de arquivo.

Uma falha concreta tem prioridade sobre a expansão de recursos. A ausência de
um item marcado não significa reprovação nem pausa do projeto.

## Passe curto — uso comum

Fazer ocasionalmente, ao abrir o app para trabalhar de verdade.

- [ ] Abrir o standalone e selecionar o dispositivo de áudio.
- [ ] Carregar WAV em `SOURCE A` e em `SOURCE B`.
- [ ] Tocar A, B e A+B; usar `PLAY` e `STOP`.
- [ ] Alterar level, pan, width, balance, mute e solo durante a reprodução.
- [ ] Observar se há estalo, corte, travamento, distorção, clipping visível ou
      medidor que não corresponda ao áudio.
- [ ] Usar GRID, FREE e JITTER; editar alguns patterns.
- [ ] Fazer uma edição simples de slices, `BLADE` e `UNDO`.

## Performance e criação

Fazer em sessões criativas normais, sem pressão de cobrir tudo.

- [ ] Usar MEMORY, MUTATION, EROSION, DECONSTRUCT, STUTTER, BURST, MICRO e
      REVERSE em combinações naturais.
- [ ] Exercitar as duas Virtual Voices (source, divisão, foco, pattern, pitch,
      envelope, level e pan).
- [ ] Usar FORM, TRACE e Assisted; conferir que o estado continua coerente.
- [ ] Abrir `PERFORM`, inclusive em segundo monitor quando disponível, e
      confirmar que as duas janelas compartilham o mesmo transporte/estado.
- [ ] Comparar Heritage Pitch com a referência Pure Data usando 0, -12, +12 e
      dois valores intermediários, em material percussivo e sustentado.

## Gravação, takes e metadados

Executar quando uma gravação real for necessária.

- [ ] Gravar um take curto em PCM16, PCM24 ou float32; finalizar e conferir
      entrada na TAKE Timeline.
- [ ] Reabrir o take por `TAKE → SOURCE A/B` e confirmar que toca corretamente.
- [ ] Editar título, artista, projeto/álbum, ano e comentário.
- [ ] Usar `SAVE METADATA / REVIEW` e confirmar que ele não reescreve o WAV.
- [ ] Em uma cópia descartável, usar `WRITE RIFF TAGS + BACKUP`; confirmar o
      backup, reabrir o arquivo e testar a tag em outro leitor.
- [ ] Fazer pelo menos uma gravação longa (>5 min) e conferir o auto-stop, a
      ausência de arquivo parcial e a integridade do WAV final.

## Projetos e portabilidade

Executar naturalmente ao trocar de sessão ou máquina.

- [ ] Salvar Project v2, fechar o app, reabrir e conferir fontes, controles e
      áudio.
- [ ] Quando houver um projeto v1 real, migrá-lo, salvar como v2 e reabrir.
- [ ] Criar `SAVE PORTABLE`, abrir o ZIP localmente e, quando conveniente, em
      outro computador ou sistema de arquivos.
- [ ] No ALBUM PROJECT, adicionar/reordenar takes, salvar/reabrir e conferir
      que análise, trims e ordem permanecem corretos.

## Compatibilidade legada

Fazer apenas quando houver um par histórico `.nvl`/`.ptn` que possa ser usado
como referência.

- [ ] Em `LEGACY I/O → IMPORT .NVL / .PTN`, abrir os dois arquivos juntos e
      confirmar slices e patterns; carregar manualmente o áudio referido em
      `SOURCE A`, pois o formato histórico só guarda o nome do sample.
- [ ] Exportar `.nvl` e `.ptn`, reimportá-los e conferir a contagem de slices e
      as 10 linhas × 8 passos dos patterns.
- [ ] Quando possível, abrir o export na v0.28.1/Pure Data e registrar qualquer
      diferença concreta de leitura, limites ou referência de arquivo.

## Master e saída

Fazer quando houver monitores/fones adequados e material conhecido.

- [ ] No TRACK MASTER, comparar `ORIGINAL` e `MASTER` com e sem `MATCH
      LOUDNESS`; ouvir transientes, graves, centro, imagem estéreo e caudas.
- [ ] Confirmar que `MATCH LOUDNESS` só atenua o lado mais alto e que PREPARE
      A/B interrompe a performance e é bloqueado durante REC.
- [ ] Em ALBUM PROJECT, testar `MATCH RELATIVE LEVELS`, fechar/reabrir e
      confirmar que trims e ordem continuam associados às faixas certas.
- [ ] Em uso real, observar clip latch, peak hold, RMS e gain reduction.
- [ ] Uma vez quando possível: testar troca de sample rate e desconectar/
      reconectar o dispositivo, procurando diagnóstico claro, silêncio seguro e
      retorno sem clique.

## Interface, ajuda e idiomas

Sem urgência; marcar aos poucos durante o uso.

- [ ] Percorrer PT, EN, FR e ES; registrar texto cortado, tradução ausente,
      acento, termo inconsistente ou tooltip confuso.
- [ ] Consultar TUTORIAL, LEARN e ABOUT em uso real.
- [ ] Conferir o layout em uma tela menor e, se disponível, em dois monitores.

## Resultado da sessão

| Data | Contexto | Itens usados | Resultado / falha |
| --- | --- | --- | --- |
| 2026-08-09 | uso prévio do usuário | audição geral | nenhum incômodo percebido |

## Limites conhecidos

Algumas lacunas de produto continuam registradas na matriz de paridade, mas não
são defeitos que precisem interromper o uso: tradução global ainda parcial,
validação humana do intercâmbio histórico `.nvl`/`.ptn` pendente e pop-out de
módulos arbitrários parcial. Ver
`AUDITORIA_PARIDADE_PD_V0281_JUCE.md` para a matriz completa.
