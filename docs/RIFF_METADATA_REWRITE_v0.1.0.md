# Escrita posterior de metadados RIFF — v0.1.0

## Objetivo

A TAKE Timeline permite gravar no próprio WAV os campos `TITLE`, `ARTIST`,
`PROJECT / ALBUM`, `YEAR` e `COMMENT` editados no catálogo. A operação é
opcional e separada de `SAVE METADATA / REVIEW`: salvar o catálogo continua
sem alterar o arquivo de áudio.

O comando `WRITE RIFF TAGS + BACKUP` exige confirmação explícita. Ele altera
somente a lista `RIFF/WAVE LIST/INFO`; review, rating, tags editoriais, notas e
recipe permanecem exclusivamente no catálogo privado.

## Contrato de segurança

A operação segue esta ordem:

1. valida que o take é um RIFF/WAVE reconhecível;
2. cria no mesmo diretório um arquivo parcial com nome único;
3. copia todos os chunks que não sejam `LIST/INFO` byte a byte, sem decodificar
   nem recodificar as amostras;
4. remove listas INFO anteriores e grava no máximo uma lista INFO nova;
5. reabre o parcial e compara sample rate, número de frames, canais e bit depth
   com o WAV original;
6. preserva o original como backup com sufixo
   `_before-riff-AAAAMMDD-HHMMSS.wav`;
7. substitui o caminho original somente depois dessas verificações.

Quando o sistema de arquivos permite, o backup começa como hard link: ocupa
praticamente só uma nova entrada de diretório e continua apontando para os
bytes originais depois da troca. Se isso não for possível, Navalha cria uma
cópia integral, recusando a ação quando não há espaço livre com margem de
segurança. O log informa `SMART LINK` ou `FILE COPY` e o nome do backup.

Uma falha antes da substituição remove o parcial e deixa o original intacto.
Uma falha durante a substituição conserva o backup; se o caminho original
desaparecer, o aplicativo tenta restaurá-lo a partir desse backup.

## Preservação e limites

- os bytes de todos os chunks `data` são preservados exatamente;
- chunks desconhecidos, `fmt `, `bext`, `cue `, `smpl` e listas que não sejam
  INFO são preservados, inclusive padding e bytes posteriores ao RIFF;
- campos vazios deixam de ser escritos; se todos estiverem vazios, as listas
  INFO anteriores são removidas sem criar uma nova;
- textos são normalizados pelos mesmos limites do writer WAV;
- o escopo atual é RIFF/WAVE clássico de até 4 GiB; RF64, Wave64, AIFF e outros
  contêineres não são reescritos;
- arquivos sem permissão de escrita ou sem espaço suficiente falham sem uma
  substituição deliberada do original.

## Validação automatizada

Os contratos do núcleo cobrem substituição de INFO preexistente, remoção de
metadados, múltiplas reescritas, rejeição de RIFF truncado e igualdade exata do
payload de áudio e dos bytes posteriores ao RIFF. A aplicação JUCE também
compila com a validação estrutural e o fluxo assíncrono fora da thread de áudio.

Ainda é necessária a aceitação humana com cópias de takes reais em sistemas de
arquivos distintos. Esse ensaio deve confirmar leitura das tags por outros
programas, recuperação manual pelo backup e ausência de diferença auditiva;
ele não é substituído pelos testes binários.
