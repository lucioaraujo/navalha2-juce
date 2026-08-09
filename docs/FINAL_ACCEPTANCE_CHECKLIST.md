# Checklist final de aceitação humana

Este roteiro cobre somente os pontos que não podem ser aprovados honestamente
por testes headless. A automação, os WAVs dourados, o stress combinado e a
comparação objetiva do MASTER já são executados separadamente.

## Preparação segura

- fechar aplicações de áudio não necessárias;
- selecionar o dispositivo e começar com volume físico baixo;
- usar buffer de 512 frames no primeiro passe e 128/256 no segundo;
- carregar WAVs curtos conhecidos em A e B;
- confirmar pelo menos 1 GiB livre antes de gravar;
- manter a v0.28.1 e o candidato JUCE na mesma sample rate.

## Bloqueio: engenharia da saída

Antes de classificar a saída como adequada a palco ou master final, resolver e
reexecutar `AUDITORIA_ENGENHARIA_SAIDA_AUDIO.md`. A paridade v0.28.1 não substitui
este bloqueio.

1. Testar Sources A/B idênticas em fase, todos os players/virtual voices e
   Preview simultâneos no máximo permitido.
2. Confirmar que Preview, medidor, dispositivo e gravação `post-safety` recebem
   exatamente a mesma soma.
3. Verificar ceiling por true peak, não apenas sample peak.
4. Confirmar clip latch, peak hold, RMS e gain reduction na interface.
5. Testar impulso, DC, subgrave, picos intersample, `NaN`/`Inf`, mudança de
   sample rate e reconnect de dispositivo.
6. Validar BS.1770 com fixtures apropriadas e analisar novamente o WAV final
   depois da codificação; manter os contratos de dither TPDF PCM16/24 aprovados.

Estado automatizado em 2026-08-09: itens 1–3 possuem cobertura de core e
validação externa FFmpeg; `NaN`/`Inf`, DC, impulso em 44,1–192 kHz e reinício do
DSP em nova taxa do item 5 também possuem contratos. Output trim, mute rampado,
suspensão e fade-in de reconnect foram automatizados no P0.4. A checagem física,
desconexão/reconnect real, subgrave ampliado, BS.1770 completo e análise do WAV
pós-codificação continuam pendentes e não devem ser aprovados por inferência. O
dither TPDF determinístico em PCM16/24 passou a ter contratos próprios no P1.1;
float32 permanece corretamente sem dither.

Aprovar somente se nenhum cenário válido ultrapassar o ceiling configurado e
se não houver clipping oculto, clique de automação/reconexão ou divergência
entre medição, gravação e saída física.

## Heritage Pitch contra Pure Data

1. Usar o mesmo WAV mono ou estéreo nas duas versões.
2. Comparar pitch em 0, -12, +12 e em dois valores intermediários.
3. Testar ataques secos, material sustentado e transientes.
4. Alternar bypass/ativo com ganho percebido alinhado.

Aprovar somente se não houver clique de mudança, instabilidade, diferença
inesperada de altura ou degradação claramente maior no candidato JUCE.

## TRACK MASTER contra WebAudio

1. Usar o WAV de referência do relatório `MASTER_OBJECTIVE_COMPARISON.md`.
2. No JUCE, carregar a faixa, ajustar a receita e usar `PREPARE A/B`.
3. Com `MATCH LOUDNESS` ativo, alternar `PLAY ORIGINAL` e `PLAY MASTER`; conferir
   no status que somente o lado mais alto recebe atenuação.
4. Desativar MATCH e repetir sem compensação para avaliar o ganho de entrega.
5. Confirmar que iniciar A/B interrompe a performance e que A/B é recusado
   enquanto REC estiver ativo.
6. Renderizar a receita padrão nas duas versões.
7. Alinhar os arquivos por amostra e igualar o ganho para a comparação cega.
8. Ouvir em monitor e fone: transientes, centro, graves, imagem e caudas.

No ALBUM PROJECT, escolher um alvo conhecido, usar `MATCH RELATIVE LEVELS` e
confirmar que a lista mostra LUFS estimado/trim por faixa, nenhum trim excede
±6 dB e a ordem/análises sobrevivem ao fechamento e à reabertura. Reordenar uma
cópia do projeto durante uma análise longa deve impedir a aplicação a faixas
erradas.

Aprovar se a intenção tonal/dinâmica for equivalente e não surgirem pumping,
aspereza, perda de centro, clipping ou alteração estéreo indesejada. Diferença
estética aceitável deve ser registrada; defeito não deve ser aceito como mera
diferença.

Esta comparação comprova paridade histórica, não ceiling true peak, BS.1770,
neutralidade de bypass ou qualidade do novo modo de master proposto na auditoria
de saída.

## Escrita RIFF posterior e recuperação

Usar somente uma cópia descartável de um take conhecido no primeiro ensaio.

1. Importar o WAV na TAKE Timeline e editar TITLE, ARTIST, PROJECT/ALBUM, YEAR
   e COMMENT.
2. Confirmar que `SAVE METADATA / REVIEW` não modifica o WAV.
3. Acionar `WRITE RIFF TAGS + BACKUP`, ler a confirmação e concluir a ação.
4. Verificar no log o tipo e o nome do backup, e confirmar que esse arquivo
   existe ao lado do take.
5. Fechar/reabrir o take no Navalha e em pelo menos outro leitor de tags.
6. Ouvir original preservado e WAV reescrito em comparação A/B, sem ajustar
   ganho, e confirmar duração, canais, sample rate e bit depth.
7. Restaurar manualmente o backup em uma cópia de teste e confirmar que ele é
   utilizável.
8. Repetir uma vez em outro sistema de arquivos quando houver mídia adequada,
   cobrindo tanto `SMART LINK` quanto `FILE COPY` se possível.

Aprovar somente se as tags forem interoperáveis, o backup for recuperável e
não houver mudança audível nem arquivo `.riff.partial` abandonado. Os contratos
automatizados já comprovam igualdade exata dos chunks `data`, mas não substituem
este teste de permissões, ferramentas externas e recuperação operacional.

## Shell, áudio real e dois monitores

1. Abrir o standalone, selecionar dispositivo e carregar A/B.
2. Exercitar PLAY/STOP, GRID/FREE/JITTER e todos os patterns.
3. Editar slices, BLADE/undo e conferir overlays da waveform.
4. Ativar MEMORY, transformações, FORM, TRACE e Assisted.
5. Abrir PERFORM no segundo monitor e confirmar uma única sessão/motor.
6. Iniciar REC, finalizar normalmente e confirmar que o take entra na timeline.
7. Fazer um ensaio controlado que ultrapasse cinco minutos e confirmar o
   `RECORDING AUTO-STOP | 5 MIN LIMIT`, sem arquivo parcial ou gigante.
8. Salvar Project v2, fechar, reabrir e comparar controles e áudio.
9. Abrir um Project v1 disponível, migrar, salvar como v2 e reabrir.
10. Desconectar/reconectar o dispositivo uma vez e confirmar diagnóstico claro.

Aprovar se não houver travamento, xrun perceptível, estado divergente entre
janelas, gravação truncada, perda de projeto ou arquivo temporário abandonado.

As fixtures documentais v1/v2 já passam pelo inspetor automatizado. Este item
permanece humano porque o workspace ainda não contém um projeto real do usuário.

## Registro

Anotar data, sistema, dispositivo, sample rate, buffer, arquivos utilizados e
resultado de cada seção. Falhas devem incluir a sequência mínima de reprodução;
não substituir o runtime v0.28.1 enquanto alguma seção permanecer reprovada.

## Traduções e revisão textual

- revisar rótulos, tooltips, mensagens de status e erros em EN/PT/FR/ES;
- manter o inglês como texto principal e conferir acentos, concordância e
  terminologia nas demais línguas;
- revisar Tutorial, LEARN, ABOUT, diálogos e documentação;
- uniformizar termos como SOURCE, MASTER, TAKE, PERFORM e COMPOSE;
- confirmar que nenhum texto fica cortado nos modos single e dual monitor.

## Pré-empacotamento e publicação

- resolver o teste de Portable Project com um ZIP v2 produzido pelo JUCE;
- repetir a validação com áudio efetivamente audível no MASTER;
- gerar um `.deb` para validação interna;
- somente após a aceitação humana, preparar builds Windows/macOS/Linux e
  documentação de instalação.
