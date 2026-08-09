# Requisitos mínimos — Navalha 2 no Linux

## Pacote Debian interno

O artefato `navalha2-deb-ubuntu-22-amd64` é compilado em Ubuntu 22.04 e se
destina a computadores **amd64 / x86_64**. Ele é apropriado para Ubuntu 22.04
ou posterior, Linux Mint baseado nessas versões e outras distribuições
Debian/Ubuntu compatíveis com as dependências informadas pelo `apt`.

Não é um pacote para ARM, Fedora, Arch ou openSUSE. Nessas plataformas, usar
uma futura distribuição específica ou compilar a partir do código-fonte.

## Computador

| Recurso | Mínimo prático | Recomendado para uso musical |
| --- | --- | --- |
| Processador | 2 núcleos, 2 GHz | 4 núcleos modernos |
| Memória | 4 GB RAM | 8 GB RAM ou mais |
| Disco | 300 MB livres para app/bibliotecas, além dos WAVs | SSD e vários GB livres para takes/projetos |
| Áudio | ALSA, PipeWire ou PulseAudio com saída estéreo | interface USB com driver ALSA |
| Sessão gráfica | X11 ou Wayland com XWayland | sessão desktop atualizada |

Os valores são referências de uso; arquivos WAV longos e bibliotecas grandes
consomem memória e disco proporcionalmente ao próprio material.

## Tela

| Uso | Resolução |
| --- | --- |
| Janela `PERFORM` | 900 × 560 no mínimo |
| Interface principal | 1480 × 900 no mínimo prático |
| Uso confortável | 1920 × 1080 |
| Performance destacada | segundo monitor opcional, idealmente 1920 × 1080 |

Em telas menores, a aplicação continua utilizável, mas a densidade de controles
reduz a legibilidade. O segundo monitor não é necessário: apenas separa a
janela `PERFORM` da edição principal.

## Antes de instalar

1. Confirmar arquitetura com `uname -m`; o resultado esperado é `x86_64`.
2. Baixar o artefato `.deb` da execução bem-sucedida em **Actions**.
3. Instalar pelo `apt`, seguindo
   [`INSTALACAO_DEB_INTERNA.md`](INSTALACAO_DEB_INTERNA.md).

Se o `apt` informar dependências incompatíveis, não forçar a instalação:
registrar a distribuição e sua versão para gerar o pacote adequado.
