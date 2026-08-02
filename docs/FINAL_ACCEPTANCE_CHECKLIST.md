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

## Heritage Pitch contra Pure Data

1. Usar o mesmo WAV mono ou estéreo nas duas versões.
2. Comparar pitch em 0, -12, +12 e em dois valores intermediários.
3. Testar ataques secos, material sustentado e transientes.
4. Alternar bypass/ativo com ganho percebido alinhado.

Aprovar somente se não houver clique de mudança, instabilidade, diferença
inesperada de altura ou degradação claramente maior no candidato JUCE.

## TRACK MASTER contra WebAudio

1. Usar o WAV de referência do relatório `MASTER_OBJECTIVE_COMPARISON.md`.
2. Renderizar a receita padrão nas duas versões.
3. Alinhar os arquivos por amostra e igualar o ganho para a comparação cega.
4. Ouvir em monitor e fone: transientes, centro, graves, imagem e caudas.
5. Repetir sem igualar ganho para avaliar o resultado de entrega.

Aprovar se a intenção tonal/dinâmica for equivalente e não surgirem pumping,
aspereza, perda de centro, clipping ou alteração estéreo indesejada. Diferença
estética aceitável deve ser registrada; defeito não deve ser aceito como mera
diferença.

## Shell, áudio real e dois monitores

1. Abrir o standalone, selecionar dispositivo e carregar A/B.
2. Exercitar PLAY/STOP, GRID/FREE/JITTER e todos os patterns.
3. Editar slices, BLADE/undo e conferir overlays da waveform.
4. Ativar MEMORY, transformações, FORM, TRACE e Assisted.
5. Abrir PERFORM no segundo monitor e confirmar uma única sessão/motor.
6. Iniciar REC, operar por pelo menos dez minutos e finalizar normalmente.
7. Salvar Project v2, fechar, reabrir e comparar controles e áudio.
8. Abrir um Project v1 disponível, migrar, salvar como v2 e reabrir.
9. Desconectar/reconectar o dispositivo uma vez e confirmar diagnóstico claro.

Aprovar se não houver travamento, xrun perceptível, estado divergente entre
janelas, gravação truncada, perda de projeto ou arquivo temporário abandonado.

As fixtures documentais v1/v2 já passam pelo inspetor automatizado. Este item
permanece humano porque o workspace ainda não contém um projeto real do usuário.

## Registro

Anotar data, sistema, dispositivo, sample rate, buffer, arquivos utilizados e
resultado de cada seção. Falhas devem incluir a sequência mínima de reprodução;
não substituir o runtime v0.28.1 enquanto alguma seção permanecer reprovada.
