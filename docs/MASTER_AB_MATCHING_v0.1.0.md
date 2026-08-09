# MASTER A/B e matching relativo — JUCE v0.1.0

## TRACK MASTER: comparação preparada

O painel TRACK MASTER incorpora o fluxo da referência Web v0.28.1:

1. carregar uma faixa;
2. ajustar a cadeia;
3. usar `PREPARE A/B`;
4. alternar `PLAY ORIGINAL` e `PLAY MASTER`;
5. interromper com `STOP A/B`.

`PREPARE A/B` renderiza fora da thread de áudio e cria duas cópias temporárias
float32: uma do buffer original já decodificado pelo Navalha e outra da cadeia
processada. Ambas têm o mesmo sample rate, número de frames e alinhamento. A
cadeia C++ atual não declara latência diferencial; o estágio live-safe comum é
aplicado igualmente às duas durante a audição.

Qualquer mudança de parâmetro invalida o MASTER preparado, interrompe sua
audição e exige novo `PREPARE A/B`. Os temporários são removidos ao preparar
novamente ou fechar o workspace. A renderização PCM24 final continua uma ação
separada e nunca sobrescreve o mix original.

## MATCH LOUDNESS

Com `MATCH LOUDNESS` ativo, a comparação preserva a regra histórica:

```text
atenuação do item escolhido = min(0 dB, LUFS estimado do outro − LUFS estimado do escolhido)
```

Portanto somente o lado mais alto é atenuado; o lado mais baixo nunca recebe
ganho para “vencer” a comparação. O ganho base da pré-escuta continua em 70%.
ORIGINAL e MASTER começam do início a cada acionamento, como na referência
Web, e o status mostra a atenuação aplicada.

A medição continua sendo a estimativa interna histórica, não ITU-R BS.1770/EBU
R128 certificada. O resultado deve ser conferido por medição externa e escuta.

## Segurança da audição

- iniciar A/B envia STOP ao motor de performance para não somar o instrumento;
- A/B é recusado durante REC, impedindo que a pré-escuta entre na gravação;
- a saída percorre o mesmo estágio live-safe, trim, mute, medidores e dispositivo;
- o barramento de preview identifica seu proprietário: uma nova pré-escuta da
  Library pode substituir A/B sem que a limpeza do MASTER interrompa depois a
  fonte errada;
- nenhuma preparação ou análise roda no callback de áudio.

## ALBUM MASTER: matching relativo

No modo ALBUM MASTER, `TARGET LUFS EST.` aceita −24 a −6 LUFS em passos de
0,5 dB. `MATCH RELATIVE LEVELS`:

1. resolve cada take na ordem atual do ALBUM PROJECT;
2. decodifica e analisa as faixas sequencialmente numa worker thread;
3. cancela a aplicação se a ordem do álbum mudar durante a análise;
4. calcula `target − LUFS estimado`, limitado a ±6 dB;
5. persiste análise e trim no `navalha-album-project` v1;
6. propaga esses dados para export e render ALBUM MASTER.

A lista mostra `TRIM` e `LUFS EST.` por faixa. A análise é opcional e
retrocompatível: projetos v1 anteriores, sem o novo objeto `analysis`, continuam
válidos.

## Validação pendente

Os contratos automatizados verificam a regra “somente atenua”, o limite de
±6 dB, associação estrita entre análise e ordem das faixas e round-trip dos
dados no projeto. Build e testes não substituem o ensaio humano de ORIGINAL ×
MASTER, transições de álbum, dispositivo real e ferramenta externa de loudness.
