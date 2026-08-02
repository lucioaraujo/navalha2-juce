# License status — Navalha 2 JUCE

**Status atual: código próprio GPL-3.0-or-later; JUCE 8.0.13 usado sob
AGPL-3.0-only. A combinação é permitida pela Seção 13 da GPLv3, sem licença
comercial do JUCE.**

## Por que duas licenças aparecem aqui

- O código-fonte do Navalha 2 em si (o que está em `src/`, os patches
  herdados, a direção de interface) segue sob **GPL-3.0-or-later**, a mesma
  licença de toda a linhagem Navalha/Navalha 2 -- ver `AUTHORS.md`,
  `CONTRIBUTORS.md` e a autorização de Glerm Soares registrada em julho de
  2026 (ver `../../NAVALHA2_PD/docs/LICENSE_STATUS.md` para o registro completo
  dessa autorização).
- Este diretório também compila e liga contra o **JUCE 8.0.13**. A opção
  open-source do JUCE é a **GNU Affero General Public License v3.0
  (AGPL-3.0-only)**, alternativa à licença comercial do fornecedor e não
  relacionada à autoria do Navalha 2.

## Por que isso é permitido sem pagar nada

A GPLv3 -- na Seção 13, "Use with the GNU Affero General Public License" --
concede permissão explícita para combinar código coberto pela GPLv3 com código
coberto pela AGPLv3 e distribuir o resultado. A permissão já existe na própria
GPL versão 3; ela não depende do sufixo "or later" adotado pelo Navalha 2.

Cada parte mantém a sua licença: a GPLv3 continua aplicável à parte Navalha 2,
a AGPLv3 continua aplicável à parte JUCE, e a exigência especial da Seção 13
da AGPLv3 para interação por rede aplica-se à combinação como um todo. Portanto
esta documentação não apresenta o código próprio do Navalha 2 como
relicenciado integralmente sob AGPL.

## Efeito prático da AGPLv3 neste projeto

A obrigação adicional mais relevante da AGPLv3 é a cláusula de interação por
rede (Seção 13): se uma versão modificada oferecer interação remota por uma
rede, deve oferecer a esses usuários acesso ao código-fonte correspondente.

O Navalha 2 JUCE é um aplicativo desktop local, sem componente de
servidor/rede. No estado atual, essa obrigação específica não é acionada pelo
uso cotidiano local do instrumento. Ela continuará documentada para que uma
futura função de rede não seja adicionada sem a correspondente oferta de
código-fonte.

## O que isso significa pra quem for distribuir/publicar

- Publicar o código próprio deste repositório mantém esse código sob
  GPL-3.0-or-later. O JUCE não é incorporado ao repositório e deve ser obtido
  separadamente sob uma das licenças oferecidas pelo fornecedor.
- Distribuir um **binário compilado** que combine Navalha 2 e JUCE exige
  cumprir simultaneamente a GPLv3 para a parte Navalha 2 e a AGPLv3 para a
  parte JUCE, incluindo a disponibilização do código-fonte correspondente.
- Nenhuma licença comercial do JUCE é necessária enquanto o JUCE for usado
  sob AGPLv3 e todas as obrigações dessa licença forem cumpridas.
- O texto integral da AGPLv3 está em `LICENSE-AGPLv3.txt`.

SPDX identifiers: `GPL-3.0-or-later` (código próprio do Navalha 2) e
`AGPL-3.0-only` (JUCE 8.0.13). Para descrever um pacote que contenha partes
sob ambas as licenças: `GPL-3.0-or-later AND AGPL-3.0-only`.
