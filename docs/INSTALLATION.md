# Navalha 2 — installation / instalação

Packages for every system are on the
[releases page](https://github.com/lucioaraujo/navalha2-juce/releases) (the
newest one is at the top). Os pacotes de cada sistema estão na
[página de releases](https://github.com/lucioaraujo/navalha2-juce/releases),
com a mais nova no topo.

- [English — Installing, step by step](#installing-step-by-step)
- [Português — Instalar, passo a passo](#instalar-passo-a-passo)
- Detailed Debian package guides / guias detalhados do pacote Debian:
  [Português](INSTALACAO_DEB_INTERNA.md) · [English](INSTALLATION_DEB_INTERNAL_EN.md) ·
  [Français](INSTALLATION_DEB_INTERNE_FR.md) · [Español](INSTALACION_DEB_INTERNA_ES.md)
- Minimum Linux requirements / requisitos mínimos no Linux:
  [Português](REQUISITOS_MINIMOS_LINUX.md) · [English](MINIMUM_REQUIREMENTS_LINUX_EN.md) ·
  [Français](CONFIGURATION_MINIMALE_LINUX_FR.md) · [Español](REQUISITOS_MINIMOS_LINUX_ES.md)

---

## Installing, step by step

The security warnings below **are expected**: Navalha 2 is free software
published without a paid code-signing certificate. They do not mean anything
is wrong, and they only appear the first time.

### Windows

1. Download `navalha2-<version>-win64.exe`.
2. Open it. Windows shows a blue **"Windows protected your PC"** box. Click
   **"More info"**, then **"Run anyway"**.
3. If Windows asks for administrator permission, click **"Yes"**.
4. Click **"Next"** to the end. From v0.1.1 on, the last page has **"Run
   Navalha 2"** already ticked. Navalha 2 is then in the **Start menu** and
   on the desktop.

**Without installing** (from v0.1.1): unzip `navalha2-<version>-win64.zip`
and open `Navalha 2.exe`.

**v0.1.0 and nothing happens**, or an error about `VCRUNTIME140.dll` or
`MSVCP140.dll`: that version silently required the "Microsoft Visual C++
Redistributable". Install it from
<https://aka.ms/vs/17/release/vc_redist.x64.exe> and open Navalha 2 again.
From v0.1.1 on, everything is inside the `.exe`.

### macOS

1. Download `navalha2-<version>-Darwin.dmg`. It works on both Intel and
   Apple Silicon Macs.
2. Open it and **drag Navalha 2 into Applications**.
3. Open it. The first time, macOS says it **cannot verify the developer**,
   because the app is not notarised by Apple, which needs a paid account.
   Click **"OK"** or **"Done"**, **not** "Move to Trash".
4. Go to **System Settings → Privacy & Security**, scroll to the security
   section, and click **"Open Anyway"** next to "Navalha 2 was blocked…".
   Confirm with your password or Touch ID, then click **"Open"**. This is
   needed only the first time.
   - On **macOS 14 or earlier**, there is a shortcut: right-click the app
     in Finder, then **Open → Open**.
5. The first time you record from the microphone or an audio interface,
   macOS asks for permission. Allow it.

**"Navalha 2 is damaged and can't be opened"** (v0.1.0, Apple Silicon): the
file is not damaged. That version was not fully signed. Run this in
Terminal, then open the app normally:

```sh
xattr -dr com.apple.quarantine "/Applications/Navalha 2.app"
```

### Linux

- **Ubuntu 22.04+, Debian 12+, Mint 21+:** double-click the `.deb`, or run
  `sudo apt install ./navalha2_<version>_x86_64.deb`. apt installs the
  dependencies.
- **Fedora, Arch, openSUSE and other distributions** (from v0.1.1): use
  one of these:
  - the **AppImage**: make it executable (Properties → allow executing, or
    `chmod +x`), then open it;
  - the **`.tar.gz`**: unpack it and run `./install.sh`, which installs for
    your user only, without root (`./install.sh --remove` undoes it).

### Minimum requirements to run

| | |
|---|---|
| **Windows** | Windows 10 or 11, 64-bit |
| **macOS** | macOS 10.15 or later (Intel); macOS 11 or later (Apple Silicon) — one Universal 2 package |
| **Linux** | x86-64 with glibc 2.35 or newer (2022+ distributions). `.deb` for the Debian family; AppImage or `.tar.gz` for the rest (from v0.1.1: launch-tested on Debian 12, Ubuntu 24.04, Fedora and Arch) |
| **Processor** | 2 cores at 2 GHz in practice; 4 modern cores recommended for musical use |
| **Memory** | 4 GB; 8 GB or more recommended. Long WAV files use memory in proportion to their size |
| **Disk** | 300 MB for the app, plus your recordings |
| **Screen** | 1480 × 900 for the main interface; the PERFORM window alone needs 900 × 560 |
| **Audio** | any stereo output; a USB interface is recommended for recording |

These are **reference values** from the project's requirement guides
(linked above). They were **not measured** on a reference machine, unlike
Rasgo Modular and Antitotem.

---

## Instalar, passo a passo

Os avisos de segurança abaixo **são esperados**. O Navalha 2 é software
livre, publicado sem certificado pago de assinatura digital. Os avisos não
indicam defeito e só aparecem na primeira vez.

### Windows

1. Baixe `navalha2-<versão>-win64.exe`.
2. Abra o arquivo. O Windows mostra a janela azul **"O Windows protegeu o
   computador"**. Clique em **"Mais informações"** e depois em
   **"Executar assim mesmo"**.
3. Se o Windows pedir permissão de administrador, clique em **"Sim"**.
4. Clique em **"Avançar"** até o fim. A partir da v0.1.1, a última tela já
   traz **"Executar o Navalha 2"** marcado. Depois disso, o Navalha 2 fica
   no **Menu Iniciar** e na área de trabalho.

**Sem instalar** (a partir da v0.1.1): descompacte
`navalha2-<versão>-win64.zip` e abra `Navalha 2.exe`.

**Se você tem a v0.1.0 e nada acontece ao abrir**, ou aparece um erro sobre
`VCRUNTIME140.dll` ou `MSVCP140.dll`: essa versão exigia, sem avisar, o
"Microsoft Visual C++ Redistributable". Instale-o pelo link da Microsoft
(<https://aka.ms/vs/17/release/vc_redist.x64.exe>) e abra o Navalha 2 de
novo. A partir da v0.1.1 isso não é mais necessário, porque tudo vai dentro
do `.exe`.

### macOS

1. Baixe `navalha2-<versão>-Darwin.dmg`. O mesmo arquivo serve para Mac
   Intel e Apple Silicon.
2. Abra o `.dmg` e **arraste o Navalha 2 para a pasta Aplicativos**.
3. Abra o app. Na primeira vez, o macOS avisa que **não pode verificar o
   desenvolvedor**. Isso acontece porque o app não passou pela notarização
   da Apple, que exige uma conta paga. Clique em **"OK"** ou
   **"Concluído"**. **Não** clique em "Mover para o Lixo".
4. Abra **Ajustes do Sistema → Privacidade e Segurança** e desça até a parte
   de segurança. Clique em **"Abrir Mesmo Assim"**, ao lado de "Navalha 2
   foi bloqueado…". Confirme com a senha ou o Touch ID e clique em
   **"Abrir"**. Isso só é preciso na primeira vez.
   - No **macOS 14 ou anterior** há um atalho: no Finder, clique no app com
     o botão direito e escolha **Abrir → Abrir**.
5. Na primeira gravação pelo microfone ou por uma interface de áudio, o
   macOS pede permissão. Permita.

**"Navalha 2 está danificado e não pode ser aberto"** (v0.1.0, Apple
Silicon): o arquivo não está danificado. Essa versão não tinha o pacote
assinado por inteiro. Rode o comando abaixo no Terminal e depois abra o app
normalmente:

```sh
xattr -dr com.apple.quarantine "/Applications/Navalha 2.app"
```

### Linux

- **Ubuntu 22.04+, Debian 12+ e Mint 21+:** dê dois cliques no `.deb` ou
  rode `sudo apt install ./navalha2_<versão>_x86_64.deb`. O apt instala as
  dependências.
- **Fedora, Arch, openSUSE e outras distribuições** (a partir da v0.1.1):
  use um destes:
  - o **AppImage**: marque o arquivo como executável (Propriedades →
    permitir executar, ou `chmod +x`) e abra;
  - o **`.tar.gz`**: descompacte e rode `./install.sh`, que instala só no
    seu usuário, sem root (`./install.sh --remove` desfaz).

### Requisitos mínimos para usar

| | |
|---|---|
| **Windows** | Windows 10 ou 11, 64 bits |
| **macOS** | macOS 10.15 ou posterior (Intel); macOS 11 ou posterior (Apple Silicon) — um só pacote Universal 2 |
| **Linux** | x86-64 com glibc 2.35 ou mais nova (distribuições de 2022 em diante). `.deb` para a família Debian; AppImage ou `.tar.gz` para as demais (a partir da v0.1.1, testados na abertura em Debian 12, Ubuntu 24.04, Fedora e Arch) |
| **Processador** | na prática, 2 núcleos a 2 GHz; 4 núcleos modernos recomendados para uso musical |
| **Memória** | 4 GB; 8 GB ou mais recomendados. WAVs longos ocupam memória na proporção do tamanho |
| **Disco** | 300 MB para o app, mais as suas gravações |
| **Tela** | 1480 × 900 para a interface principal; a janela PERFORM sozinha precisa de 900 × 560 |
| **Áudio** | qualquer saída estéreo; uma interface USB é recomendada para gravar |

Estes são **valores de referência**, tirados dos guias de requisitos do
projeto (links acima). Eles **não foram medidos** numa máquina de
referência, ao contrário do que foi feito com o Rasgo Modular e com o
Antitotem.
