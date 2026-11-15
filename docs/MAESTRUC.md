# Navalha 2 e o Maestruc

Registro de 15/11/2026. O Maestruc é o regente da família RASGO
(`RASGO/PROJETOS_AUTORAIS/MAESTRUC` no monorepo privado): abre os
instrumentos, mixa e grava os sons deles e manda comandos pelo protocolo
`maestruc.link/1` (OSC só na máquina local, porta 47900).

## O que o Navalha 2 aceita

Desligado por padrão; liga com `--maestruc` ou `NAVALHA_MAESTRUC=1`. Sem a
opção, nada muda.

| Do Maestruc | No Navalha 2 |
|---|---|
| tocar / parar | o mesmo caminho do PERFORM (`remotePlay`, `remoteStop`) |
| tempo | BPM de 20 a 400 (`remoteSetTempo`) |
| abrir | projeto `.navalha` ou portátil `.zip`, o mesmo caminho do OPEN PROJECT; outro formato é recusado com o motivo |
| estado | tocando ou não e o BPM; o Navalha não tem seed |
| som | a mistura vai ao Maestruc pela ponte de som (memória compartilhada, Linux/Windows/macOS); enquanto ele lê, o Navalha cala a própria saída e volta sozinho em ~0,5 s se ele some |
| mixer | **não**: LEVEL/MUTE/SOLO do Navalha ficam só nas mãos de quem toca |

Os comandos só valem com a senha da sessão que o Maestruc grava num arquivo
só do usuário. Versão 0.2.0 (15/11/2026), já no JUCE 9.0.3 da família.

`--abrir /caminho/projeto.navalha` abre um projeto ao iniciar. **LEVEL, MUTE
e SOLO não entram:** são só de quem toca (decisão do autor). Seed, tom, seção e macros respondem "capacidade ausente".

## Arquivos

- `src/app/MaestrucLink.h` e `src/app/PonteDeSom.h`: cópias do receptor de referência (canal, ponte de som, senha), com a origem e o SHA-256
  anotada. Segue AGPL-3.0-or-later, como o JUCE que o Navalha 2 já combina
  com o próprio código GPL-3.0-or-later (`docs/LICENSE_STATUS.md`).
- `src/app/Main.cpp`: `enableMaestrucLink()` e `openProjectFromOutside()` no
  `MainComponent`; o receptor é desfeito antes de tudo no destrutor.
- `src/app/CMakeLists.txt`: módulo `juce_osc`.
- `run_navalha.sh`: abre o build de `.local-build/juce-app`
  (`NAVALHA_BUILD_DIR` do `build_local.sh`); a pasta antiga
  `juce-app-native` ficou presa ao caminho de antes da mudança de lugar do
  projeto e só serve de reserva.

## Validação (15/11/2026)

`build_local.sh` com `NAVALHA_BUILD_DIR=.local-build/juce-app`: sem avisos,
10/10 testes. Ensaio sem som (saída nula do PipeWire, HOME temporário): o app
se anunciou (`transport,tempo,open`), tocou, aceitou BPM 100 (estado
confirmou), parou, recusou um `.md`, abriu
`validation/project_v1_reference.navalha` (arquivo intacto, MD5 igual) e
recusou seed. Não ensaiado: com som e com o autor tocando.
