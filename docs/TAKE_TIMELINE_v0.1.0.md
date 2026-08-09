# TAKE Timeline — JUCE v0.1.0

## Escopo implementado

A migração JUCE passa a registrar automaticamente cada WAV finalizado com
sucesso em um catálogo local persistente. O áudio gravado permanece imutável.

Cada entrada conserva:

- identificador estável do take;
- caminho e nome do WAV;
- data/hora, duração, frames, sample rate e formato PCM16/PCM24/FLOAT32;
- título, artista, projeto/álbum, ano e comentário;
- status `EXPERIMENT`, `CANDIDATE`, `SELECTED`, `APPROVED`, `REJECTED` ou
  `MASTER`;
- rating de zero a cinco, tags e notas de revisão;
- recipe JSON capturada imediatamente antes do início da gravação.

O catálogo usa o formato `navalha-take-catalog` v1, tem limite de 512 entradas
e é guardado nas configurações privadas da aplicação. Campos textuais, tamanho
do catálogo, quantidade de entradas e recipe possuem limites explícitos.

## Workspace nativo de produção

O comando `TAKES / MASTER` na navegação abre uma única janela de produção com
uma única sessão/motor. Em telas largas, TAKE TIMELINE e MASTERING permanecem
visíveis lado a lado, com fundos semitransparentes distintos. Em larguras
menores, a mesma janela alterna as duas páginas para evitar controles cortados.
A lista mostra primeiro os takes mais recentes e sinaliza quando o WAV
referenciado não está mais no caminho registrado.

O editor permite:

- atualizar metadados de catálogo e revisão;
- exportar a recipe de um take como JSON;
- usar o WAV como SOURCE A ou SOURCE B;
- enviar o take diretamente ao TRACK MASTER, que o carrega e analisa sem
  modificar o arquivo original.

`USE AS SOURCE A/B` decodifica o WAV em memória pelo mesmo caminho seguro da
Audio Library. Não move, renomeia nem reescreve o take original.

## Recipe e limite de reprodutibilidade

A recipe `navalha-take-recipe` v1 contém um snapshot Project v2 formado pelo
estado de interface imediatamente anterior ao REC: fontes, slices, patterns,
mixer, vozes, FORM, TRACE, Assisted, motifs e locks.

O cursor interno do RNG Assisted ainda não possui telemetria segura para a UI;
por isso a recipe declara explicitamente `assistedCursorAvailable: false` e não
promete reprodução exata desse cursor. Seed e configurações visíveis são
preservados. A ampliação dessa telemetria permanece uma tarefa futura.

## Limitações conhecidas

- gravações JUCE anteriores a este catálogo não são descobertas
  automaticamente;
- editar os campos da janela altera o catálogo privado, não os chunks RIFF INFO
  do WAV;
- ainda não há preset padrão de metadados para novas gravações;
- o ALBUM PROJECT builder ainda não foi ligado ao catálogo de takes.

## Versionamento visual

O aplicativo JUCE passa a se identificar como `v0.1.0`. O rodapé mantém
`PD reference v0.28.1` somente para indicar a versão funcional usada na
auditoria de paridade.

O mascote e o letreiro aprovados foram deslocados juntos 28 pixels à direita
para liberar a aba vertical, sem alteração artística dos assets.

A janela principal volta a exibir uma barra de rolagem vertical estreita. A
roda central do mouse percorre livremente o documento, enquanto as abas no topo
continuam disponíveis como atalhos para as áreas funcionais.
