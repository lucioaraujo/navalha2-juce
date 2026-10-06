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

The security warnings below **are expected**: they do not mean anything is
wrong, and they only appear the first time.

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

**v0.1.0 or v0.1.1 and the Start menu shortcut does nothing:** install
v0.1.2 over it. Those versions' shortcuts pointed to a file that does not
exist; the program itself is fine and opens from its folder in
`C:\Program Files`.

### macOS

1. Download `navalha2-<version>-Darwin.dmg`. It works on both Intel and
   Apple Silicon Macs.
2. Open it and **drag Navalha 2 into Applications**.
3. Open it. The first time, macOS **blocks** the app with a message saying
   **Apple could not confirm it is free of malicious software**. In French,
   for example: *« Navalha 2 ne peut pas être ouvert. Apple n'a pas pu confirmer
   que Navalha 2 ne contenait pas de logiciel malveillant. »*
   A similar message may appear instead, such as \"cannot verify the
   developer\" or \"is damaged and can't be opened\". In every case, do the
   same:
   - This is **not a defect or a virus**.
   - Click **"OK"** or **"Done"**, **not** "Move to Trash".
4. Allow the app in one of two ways. You only need to do this once.
   - **In System Settings:** go to **System Settings → Privacy & Security**,
     scroll to the security section, click **"Open Anyway"**, confirm with
     your password or Touch ID, and click **"Open"**. On macOS 14 or
     earlier: right-click the app in Finder → **Open → Open**.
   - **In Terminal**, if the button does not appear or the block remains:
     1. Open **Terminal**. It is in Applications → Utilities, or press
        Cmd + Space and type "Terminal".
     2. Paste this line and press **Enter**:

        ```sh
        xattr -dr com.apple.quarantine "/Applications/Navalha 2.app"
        ```

        If nothing is printed after Enter, it worked.
     3. Close Terminal and open the app normally.

     The command only removes the "quarantine mark" macOS puts on every
     downloaded file. It does not change the app.
5. The first time you record from the microphone or an audio interface,
   macOS asks for permission. Allow it.

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

Os avisos de segurança abaixo **são esperados**: não indicam defeito nem
vírus e só aparecem na primeira vez.

### Windows

1. Baixe `navalha2-<versão>-win64.exe`.
2. Abra o arquivo. O Windows mostra a janela azul **"O Windows protegeu o
   computador"** (ou outra mensagem dizendo que o programa não é
   reconhecido). Clique em **"Mais informações"** e depois em
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

**Se você tem a v0.1.0 ou a v0.1.1 e o atalho do Menu Iniciar não abre
nada:** instale a v0.1.2 por cima. O atalho dessas versões apontava para um
arquivo que não existe; o programa em si está bom e abre pela pasta dele em
`C:\Program Files`.

### macOS

1. Baixe `navalha2-<versão>-Darwin.dmg`. O mesmo arquivo serve para Mac
   Intel e Apple Silicon.
2. Abra o `.dmg` e **arraste o Navalha 2 para a pasta Aplicativos**.
3. Abra o app. Na primeira vez, o macOS **bloqueia** o app e mostra uma
   mensagem dizendo que **a Apple não pôde confirmar que ele está livre de
   software malicioso**. Em francês, por exemplo: *« Navalha 2 ne peut pas être
   ouvert. Apple n'a pas pu confirmer que Navalha 2 ne contenait pas de logiciel
   malveillant. »*
   Também pode aparecer uma mensagem parecida, como \"não é possível
   verificar o desenvolvedor\" ou \"está danificado e não pode ser aberto\". Em
   todos esses casos, faça o mesmo:
   - Isso **não indica defeito nem vírus**.
   - Clique em **"OK"** ou **"Concluído"**. **Não** clique em "Mover para o
     Lixo".
4. Libere o app de um destes dois jeitos. Basta fazer uma vez.
   - **Pelos Ajustes:** abra **Ajustes do Sistema → Privacidade e
     Segurança**, desça até a parte de segurança e clique em **"Abrir Mesmo
     Assim"**. Confirme com sua senha ou Touch ID e clique em **"Abrir"**.
     No macOS 14 ou anterior: clique no app com o botão direito no Finder e
     escolha **Abrir → Abrir**.
   - **Pelo Terminal**, se o botão não aparecer ou o bloqueio continuar:
     1. Abra o **Terminal**. Ele fica em Aplicativos → Utilitários, ou use
        Cmd + Espaço e digite "Terminal".
     2. Cole a linha abaixo e aperte **Enter**:

        ```sh
        xattr -dr com.apple.quarantine "/Applications/Navalha 2.app"
        ```

        Se nada aparecer depois do Enter, deu certo.
     3. Feche o Terminal e abra o app normalmente.

     O comando só retira a "marca de quarentena" que o macOS põe em todo
     arquivo baixado. Ele não altera o app.
5. Na primeira gravação pelo microfone ou por uma interface de áudio, o
   macOS pede permissão. Permita.

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
