#!/usr/bin/env python3
"""Tutorial, Learn and Installation pages of the Navalha 2 site (EN at the
root, PT/FR/ES in their folders), and the page links in the main menu.

    python3 gerar_idiomas.py && python3 paginas_extra.py && python3 seo.py

RASGO family site leveling (author, 19 Nov 2026: "nivele pelos mais
completos"; method in RASGO_DOCUMENTATION/design/WEBSITES.md §8):

- Tutorial: the ten chapters of the app's TUTORIAL (src/app/UiHelp.h,
  `tutorialChapters`), read from the code so the site and the window say
  the same thing;
- Learn: every LEARN entry of the app (`learnEntries`), the explanation of
  each control;
- Installation: the steps of docs/INSTALLATION.md (EN and PT), here in the
  four languages, with the download links of the current release;
- the main menu keeps its anchors and gains Tutorial, Learn (the page, in
  place of the #learn anchor) and Installation.

Each new page reuses the shell (head, header, footer) of the home page of
its language; seo.py writes the SEO block afterwards.
"""

import html
import pathlib
import re

AQUI = pathlib.Path(__file__).parent
FONTE = AQUI.parents[1] / "src" / "app" / "UiHelp.h"
VERSAO = "0.2.2"
REL = "https://github.com/lucioaraujo/navalha2-juce/releases/download/v%s/" % VERSAO
PACOTES = [("Windows · .exe", "navalha2-%s-win64.exe"), ("Windows · .zip", "navalha2-%s-win64.zip"),
           ("macOS · .dmg", "navalha2-%s-Darwin.dmg"), ("Linux · .deb", "navalha2_%s_x86_64.deb"),
           ("Linux · AppImage", "navalha2-%s-linux-x86_64.AppImage"), ("Linux · .tar.gz", "navalha2-%s-linux-x86_64.tar.gz")]
LANGS = ("en", "pt", "fr", "es")
ORDEM = {"en": 0, "pt": 1, "fr": 2, "es": 3}
PASTA = {"en": "", "pt": "pt/", "fr": "fr/", "es": "es/"}

ROTULOS = {  # menu labels of the new pages
    "en": {"tutorial": "Tutorial", "aprender": "Learn", "instalacao": "Installation"},
    "pt": {"tutorial": "Tutorial", "aprender": "Aprender", "instalacao": "Instalação"},
    "fr": {"tutorial": "Tutoriel", "aprender": "Apprendre", "instalacao": "Installation"},
    "es": {"tutorial": "Tutorial", "aprender": "Aprender", "instalacao": "Instalación"},
}

PAGINAS = {
    "tutorial": {
        "en": ("Tutorial — Navalha 2", "Tutorial", "The ten chapters of the tutorial Navalha 2 carries under its TUTORIAL button, from the first source to recording and help.", "Navalha 2 tutorial: sources, library, region, slices, pattern, sound, the assisted performer, projects, recording and help."),
        "pt": ("Tutorial — Navalha 2", "Tutorial", "Os dez capítulos do tutorial que o Navalha 2 traz no botão TUTORIAL, da primeira fonte à gravação e à ajuda.", "Tutorial do Navalha 2: fontes, biblioteca, região, slices, pattern, som, o performer assistido, projetos, gravação e ajuda."),
        "fr": ("Tutoriel — Navalha 2", "Tutoriel", "Les dix chapitres du tutoriel que Navalha 2 porte sous son bouton TUTORIAL, de la première source à l'enregistrement et à l'aide.", "Tutoriel de Navalha 2 : sources, bibliothèque, région, slices, pattern, son, le performer assisté, projets, enregistrement et aide."),
        "es": ("Tutorial — Navalha 2", "Tutorial", "Los diez capítulos del tutorial que Navalha 2 lleva en su botón TUTORIAL, de la primera fuente a la grabación y la ayuda.", "Tutorial de Navalha 2: fuentes, biblioteca, región, slices, patrón, sonido, el performer asistido, proyectos, grabación y ayuda."),
    },
    "aprender": {
        "en": ("Learn — Navalha 2", "Learn", "Every control of Navalha 2 and what it does: the LEARN text the app shows in its Activity Log when you point at a control.", "Learn Navalha 2: every control explained, from sources and slices to the mixer, the performer, recording and projects."),
        "pt": ("Aprender — Navalha 2", "Aprender", "Cada controle do Navalha 2 e o que ele faz: o texto do LEARN que o app mostra no Activity Log quando você aponta um controle.", "Aprender o Navalha 2: cada controle explicado, das fontes e slices ao mixer, ao performer, à gravação e aos projetos."),
        "fr": ("Apprendre — Navalha 2", "Apprendre", "Chaque contrôle de Navalha 2 et ce qu'il fait : le texte LEARN que l'app affiche dans son Activity Log quand vous pointez un contrôle.", "Apprendre Navalha 2 : chaque contrôle expliqué, des sources et slices au mixeur, au performer, à l'enregistrement et aux projets."),
        "es": ("Aprender — Navalha 2", "Aprender", "Cada control de Navalha 2 y lo que hace: el texto LEARN que la app muestra en su Activity Log cuando apunta a un control.", "Aprender Navalha 2: cada control explicado, de las fuentes y slices al mezclador, al performer, la grabación y los proyectos."),
    },
    "instalacao": {
        "en": ("Installation — Navalha 2", "Installation", "How to install Navalha 2 on Windows, macOS and Linux, and what it needs from the computer.", "Install Navalha 2 on Windows, macOS and Linux, step by step, with the security warnings and the requirements."),
        "pt": ("Instalação — Navalha 2", "Instalação", "Como instalar o Navalha 2 no Windows, no macOS e no Linux, e o que ele pede do computador.", "Instalar o Navalha 2 no Windows, macOS e Linux, passo a passo, com os avisos de segurança e os requisitos."),
        "fr": ("Installation — Navalha 2", "Installation", "Comment installer Navalha 2 sous Windows, macOS et Linux, et ce qu'il demande à l'ordinateur.", "Installer Navalha 2 sous Windows, macOS et Linux, pas à pas, avec les avertissements de sécurité et la configuration requise."),
        "es": ("Instalación — Navalha 2", "Instalación", "Cómo instalar Navalha 2 en Windows, macOS y Linux, y qué pide del ordenador.", "Instalar Navalha 2 en Windows, macOS y Linux, paso a paso, con los avisos de seguridad y los requisitos."),
    },
}

INST = {
    "en": [
        ("Download", [("p", "Version %s, the current one. The Windows and macOS security warnings are expected and appear only the first time." % VERSAO), ("dl", None)]),
        ("Windows", [("ol", ["Download `navalha2-<version>-win64.exe` and open it.",
                             "Windows shows **\"Windows protected your PC\"**: click **\"More info\"**, then **\"Run anyway\"**.",
                             "If Windows asks for administrator permission, click **\"Yes\"**.",
                             "Click **\"Next\"** to the end. Navalha 2 is then in the Start menu and on the desktop; each new version replaces the previous one."]),
                     ("p", "**Without installing:** unzip `navalha2-<version>-win64.zip` and open `Navalha 2.exe`.")]),
        ("macOS", [("ol", ["Download `navalha2-<version>-Darwin.dmg`. It works on Intel and Apple Silicon Macs.",
                           "Open it and drag Navalha 2 into **Applications**.",
                           "Open the app. The first time, macOS blocks it, because it is signed ad hoc, without a paid Apple Developer ID. Click **\"OK\"**.",
                           "Allow the app once: in **System Settings → Privacy & Security**, click **\"Open Anyway\"** and confirm. On macOS 14 or earlier: right-click the app in Finder → **Open → Open**.",
                           "The first time you record from the microphone or an audio interface, macOS asks for permission: allow it."]),
                   ("p", "If the button does not appear, use Terminal. The command removes the quarantine mark macOS puts on every downloaded file:"),
                   ("code", 'xattr -dr com.apple.quarantine "/Applications/Navalha 2.app"')]),
        ("Linux", [("ul", ["**Ubuntu 22.04+, Debian 12+, Mint 21+:** double-click the `.deb`, or `sudo apt install ./navalha2_<version>_x86_64.deb`.",
                           "**Other distributions, AppImage:** make it executable (Properties → allow executing, or `chmod +x`) and open it.",
                           "**Other distributions, `.tar.gz`:** unpack it and run `./install.sh`, which installs for your user, without root; `./install.sh --remove` undoes it."])]),
        ("Requirements", [("tab", [("**Windows**", "Windows 10 or 11, 64-bit"), ("**macOS**", "10.15 or later (Intel), 11 or later (Apple Silicon); one Universal 2 package"),
                                   ("**Linux**", "x86-64 with glibc 2.35 or newer; launch-tested on Debian 12, Ubuntu 24.04, Fedora and Arch"),
                                   ("**Processor**", "2 cores at 2 GHz; 4 modern cores recommended for musical use"),
                                   ("**Memory**", "4 GB; 8 GB or more recommended (long WAV files use memory in proportion to their size)"),
                                   ("**Disk**", "300 MB for the app, plus your recordings"), ("**Screen**", "1480 × 900 for the main interface"),
                                   ("**Audio**", "any stereo output; a USB interface is recommended for recording")]),
                          ("p", "These are reference values from the project's requirement guides, not measured on a reference machine. For anything else, write to **rasgo.instruments@gmail.com**.")]),
    ],
    "pt": [
        ("Baixar", [("p", "Versão %s, a atual. Os avisos de segurança do Windows e do macOS são esperados e aparecem só na primeira vez." % VERSAO), ("dl", None)]),
        ("Windows", [("ol", ["Baixe `navalha2-<versão>-win64.exe` e abra.",
                             "O Windows mostra **\"O Windows protegeu o computador\"**: clique em **\"Mais informações\"** e depois em **\"Executar assim mesmo\"**.",
                             "Se pedir permissão de administrador, clique em **\"Sim\"**.",
                             "Clique em **\"Avançar\"** até o fim. O Navalha 2 fica no Menu Iniciar e na área de trabalho; cada versão nova substitui a anterior."]),
                     ("p", "**Sem instalar:** descompacte `navalha2-<versão>-win64.zip` e abra `Navalha 2.exe`.")]),
        ("macOS", [("ol", ["Baixe `navalha2-<versão>-Darwin.dmg`. Ele serve para Mac Intel e Apple Silicon.",
                           "Abra e arraste o Navalha 2 para **Aplicativos**.",
                           "Abra o app. Na primeira vez, o macOS o bloqueia, porque ele tem assinatura ad hoc, sem o Developer ID pago da Apple. Clique em **\"OK\"**.",
                           "Libere o app uma vez: em **Ajustes do Sistema → Privacidade e Segurança**, clique em **\"Abrir Mesmo Assim\"** e confirme. No macOS 14 ou anterior: botão direito no app, no Finder → **Abrir → Abrir**.",
                           "Na primeira gravação pelo microfone ou por uma interface de áudio, o macOS pede permissão: permita."]),
                   ("p", "Se o botão não aparecer, use o Terminal. O comando tira a marca de quarentena que o macOS põe em todo arquivo baixado:"),
                   ("code", 'xattr -dr com.apple.quarantine "/Applications/Navalha 2.app"')]),
        ("Linux", [("ul", ["**Ubuntu 22.04+, Debian 12+, Mint 21+:** dois cliques no `.deb`, ou `sudo apt install ./navalha2_<versão>_x86_64.deb`.",
                           "**Outras distribuições, AppImage:** torne-o executável (Propriedades → permitir executar, ou `chmod +x`) e abra.",
                           "**Outras distribuições, `.tar.gz`:** descompacte e rode `./install.sh`, que instala para o seu usuário, sem root; `./install.sh --remove` desfaz."])]),
        ("Requisitos", [("tab", [("**Windows**", "Windows 10 ou 11, 64 bits"), ("**macOS**", "10.15 ou mais novo (Intel), 11 ou mais novo (Apple Silicon); um pacote Universal 2"),
                                 ("**Linux**", "x86-64 com glibc 2.35 ou mais nova; testado na abertura em Debian 12, Ubuntu 24.04, Fedora e Arch"),
                                 ("**Processador**", "2 núcleos a 2 GHz; 4 núcleos modernos recomendados para uso musical"),
                                 ("**Memória**", "4 GB; 8 GB ou mais recomendados (WAVs longos usam memória proporcional ao tamanho)"),
                                 ("**Disco**", "300 MB para o app, mais as suas gravações"), ("**Tela**", "1480 × 900 para a interface principal"),
                                 ("**Áudio**", "qualquer saída estéreo; uma interface USB é recomendada para gravar")]),
                        ("p", "São valores de referência dos guias de requisitos do projeto, não medidos numa máquina de referência. Para outra coisa, escreva para **rasgo.instruments@gmail.com**.")]),
    ],
    "fr": [
        ("Télécharger", [("p", "Version %s, l'actuelle. Les avertissements de sécurité de Windows et de macOS sont attendus et n'apparaissent que la première fois." % VERSAO), ("dl", None)]),
        ("Windows", [("ol", ["Téléchargez `navalha2-<version>-win64.exe` et ouvrez-le.",
                             "Windows affiche **« Windows a protégé votre ordinateur »** : cliquez sur **« Informations complémentaires »**, puis sur **« Exécuter quand même »**.",
                             "Si Windows demande l'autorisation d'administrateur, cliquez sur **« Oui »**.",
                             "Cliquez sur **« Suivant »** jusqu'à la fin. Navalha 2 est alors dans le menu Démarrer et sur le bureau ; chaque nouvelle version remplace la précédente."]),
                     ("p", "**Sans installer :** décompressez `navalha2-<version>-win64.zip` et ouvrez `Navalha 2.exe`.")]),
        ("macOS", [("ol", ["Téléchargez `navalha2-<version>-Darwin.dmg`. Il fonctionne sur les Mac Intel et Apple Silicon.",
                           "Ouvrez-le et glissez Navalha 2 dans **Applications**.",
                           "Ouvrez l'app. La première fois, macOS la bloque, car elle a une signature ad hoc, sans Developer ID Apple payant. Cliquez sur **« OK »**.",
                           "Autorisez l'app une fois : dans **Réglages Système → Confidentialité et sécurité**, cliquez sur **« Ouvrir quand même »** et confirmez. Sous macOS 14 ou antérieur : clic droit sur l'app dans le Finder → **Ouvrir → Ouvrir**.",
                           "Au premier enregistrement par le microphone ou une interface audio, macOS demande l'autorisation : acceptez."]),
                   ("p", "Si le bouton n'apparaît pas, utilisez le Terminal. La commande retire la marque de quarantaine que macOS pose sur chaque fichier téléchargé :"),
                   ("code", 'xattr -dr com.apple.quarantine "/Applications/Navalha 2.app"')]),
        ("Linux", [("ul", ["**Ubuntu 22.04+, Debian 12+, Mint 21+ :** double-cliquez sur le `.deb`, ou `sudo apt install ./navalha2_<version>_x86_64.deb`.",
                           "**Autres distributions, AppImage :** rendez-le exécutable (Propriétés → autoriser l'exécution, ou `chmod +x`) et ouvrez-le.",
                           "**Autres distributions, `.tar.gz` :** décompressez et lancez `./install.sh`, qui installe pour votre utilisateur, sans root ; `./install.sh --remove` annule."])]),
        ("Configuration requise", [("tab", [("**Windows**", "Windows 10 ou 11, 64 bits"), ("**macOS**", "10.15 ou ultérieur (Intel), 11 ou ultérieur (Apple Silicon) ; un paquet Universal 2"),
                                            ("**Linux**", "x86-64 avec glibc 2.35 ou plus récente ; testé au lancement sous Debian 12, Ubuntu 24.04, Fedora et Arch"),
                                            ("**Processeur**", "2 cœurs à 2 GHz ; 4 cœurs modernes recommandés pour l'usage musical"),
                                            ("**Mémoire**", "4 Go ; 8 Go ou plus recommandés (les WAV longs occupent une mémoire proportionnelle à leur taille)"),
                                            ("**Disque**", "300 Mo pour l'app, plus vos enregistrements"), ("**Écran**", "1480 × 900 pour l'interface principale"),
                                            ("**Audio**", "toute sortie stéréo ; une interface USB est recommandée pour enregistrer")]),
                                   ("p", "Ce sont des valeurs de référence des guides de configuration du projet, non mesurées sur une machine de référence. Pour autre chose, écrivez à **rasgo.instruments@gmail.com**.")]),
    ],
    "es": [
        ("Descargar", [("p", "Versión %s, la actual. Los avisos de seguridad de Windows y macOS son esperables y aparecen solo la primera vez." % VERSAO), ("dl", None)]),
        ("Windows", [("ol", ["Descarga `navalha2-<versión>-win64.exe` y ábrelo.",
                             "Windows muestra **\"Windows protegió su PC\"**: haz clic en **\"Más información\"** y luego en **\"Ejecutar de todas formas\"**.",
                             "Si Windows pide permiso de administrador, haz clic en **\"Sí\"**.",
                             "Haz clic en **\"Siguiente\"** hasta el final. Navalha 2 queda en el menú Inicio y en el escritorio; cada versión nueva reemplaza a la anterior."]),
                     ("p", "**Sin instalar:** descomprime `navalha2-<versión>-win64.zip` y abre `Navalha 2.exe`.")]),
        ("macOS", [("ol", ["Descarga `navalha2-<versión>-Darwin.dmg`. Sirve para Mac Intel y Apple Silicon.",
                           "Ábrelo y arrastra Navalha 2 a **Aplicaciones**.",
                           "Abre la app. La primera vez, macOS la bloquea, porque tiene firma ad hoc, sin el Developer ID de pago de Apple. Haz clic en **\"OK\"**.",
                           "Autoriza la app una vez: en **Ajustes del Sistema → Privacidad y seguridad**, haz clic en **\"Abrir igualmente\"** y confirma. En macOS 14 o anterior: clic derecho en la app en el Finder → **Abrir → Abrir**.",
                           "En la primera grabación por el micrófono o una interfaz de audio, macOS pide permiso: autorízalo."]),
                   ("p", "Si el botón no aparece, usa la Terminal. El comando quita la marca de cuarentena que macOS pone a todo archivo descargado:"),
                   ("code", 'xattr -dr com.apple.quarantine "/Applications/Navalha 2.app"')]),
        ("Linux", [("ul", ["**Ubuntu 22.04+, Debian 12+, Mint 21+:** doble clic en el `.deb`, o `sudo apt install ./navalha2_<versión>_x86_64.deb`.",
                           "**Otras distribuciones, AppImage:** hazlo ejecutable (Propiedades → permitir ejecutar, o `chmod +x`) y ábrelo.",
                           "**Otras distribuciones, `.tar.gz`:** descomprímelo y ejecuta `./install.sh`, que instala para tu usuario, sin root; `./install.sh --remove` lo deshace."])]),
        ("Requisitos", [("tab", [("**Windows**", "Windows 10 u 11, 64 bits"), ("**macOS**", "10.15 o posterior (Intel), 11 o posterior (Apple Silicon); un paquete Universal 2"),
                                 ("**Linux**", "x86-64 con glibc 2.35 o más reciente; probado al abrir en Debian 12, Ubuntu 24.04, Fedora y Arch"),
                                 ("**Procesador**", "2 núcleos a 2 GHz; 4 núcleos modernos recomendados para uso musical"),
                                 ("**Memoria**", "4 GB; 8 GB o más recomendados (los WAV largos usan memoria proporcional a su tamaño)"),
                                 ("**Disco**", "300 MB para la app, más tus grabaciones"), ("**Pantalla**", "1480 × 900 para la interfaz principal"),
                                 ("**Audio**", "cualquier salida estéreo; se recomienda una interfaz USB para grabar")]),
                        ("p", "Son valores de referencia de las guías de requisitos del proyecto, no medidos en una máquina de referencia. Para otra cosa, escribe a **rasgo.instruments@gmail.com**.")]),
    ],
}


def literais(trecho):
    """Strings of a {...} initializer, adjacent literals joined, commas separating."""
    partes, anterior = [], False
    for literal, virgula in re.findall(r'"((?:[^"\\]|\\.)*)"|(,)', trecho):
        if virgula:
            anterior = False
            continue
        texto = bytes(literal, "utf-8").decode("unicode_escape").encode("latin-1").decode("utf-8")
        if anterior:
            partes[-1] += texto
        else:
            partes.append(texto)
        anterior = True
    return partes


def ler(nome, tipo):
    fonte = FONTE.read_text(encoding="utf-8")
    inicio = fonte.index("inline constexpr std::array %s {" % nome)
    fim = fonte.index("\n};", inicio)
    itens = []
    for bloco in re.split(r"\n\s*%s \{" % tipo, fonte[inicio:fim])[1:]:
        p = literais(bloco)
        itens.append(p)
    return itens


def inline(texto):
    e = html.escape(texto, quote=False)
    e = re.sub(r"\*\*(.+?)\*\*", r"<strong>\1</strong>", e)
    return re.sub(r"`(.+?)`", r"<code>\1</code>", e)


def blocos(lista):
    out = []
    for tipo, c in lista:
        if tipo == "p":
            out.append("<p>%s</p>" % inline(c))
        elif tipo in ("ul", "ol"):
            out.append("<%s>%s</%s>" % (tipo, "".join("<li>%s</li>" % inline(i) for i in c), tipo))
        elif tipo == "tab":
            out.append('<table class="page-table">%s</table>' % "".join(
                '<tr><th scope="row">%s</th><td>%s</td></tr>' % (inline(a), inline(b)) for a, b in c))
        elif tipo == "code":
            out.append("<pre><code>%s</code></pre>" % html.escape(c, quote=False))
        elif tipo == "dl":
            out.append('<p class="page-downloads">%s</p>' % " ".join(
                '<a class="button" href="%s%s">%s</a>' % (REL, f % VERSAO, html.escape(r, quote=False)) for r, f in PACOTES))
    return "\n".join(out)


NAV_RE = re.compile(r'(<nav id="main-nav"[^>]*>)(.*?)(</nav>)', re.S)


def menu(lang, atual, casa):
    """The home's anchors, plus Tutorial, Learn (page) and Installation."""
    def troca(m):
        abre, corpo, fecha = m.groups()
        ancoras = re.findall(r'<a href="([^"]*#[a-z-]+)"[^>]*>([^<]*)</a>', corpo)
        r, itens = ROTULOS[lang], []
        for href, texto in ancoras:
            ancora = href[href.index("#"):]
            if ancora == "#learn":
                continue
            itens.append((("" if casa else "./") + ancora if casa else "./" + ancora, texto, False))
            if ancora == "#workflow":
                for tipo in ("tutorial", "aprender", "instalacao"):
                    itens.append((tipo + ".html", r[tipo], tipo == atual))
        links = "".join('\n      <a href="%s"%s>%s</a>' % (h, ' aria-current="page"' if cur else "", t) for h, t, cur in itens)
        return abre + links + "\n    " + fecha
    return troca


def pagina(lang, tipo, corpo):
    casa = AQUI / PASTA[lang] / "index.html"
    texto = casa.read_text(encoding="utf-8")
    titulo, h1, lead, desc = PAGINAS[tipo][lang]
    cabeca, resto = texto.split('<main id="main">', 1)
    rodape = resto.split("</main>", 1)[1]
    cabeca = re.sub(r"<title>.*?</title>", lambda _: "<title>%s</title>" % html.escape(titulo, quote=False), cabeca, count=1, flags=re.S)
    cabeca = re.sub(r'<meta name="description" content="[^"]*"', lambda _: '<meta name="description" content="%s"' % html.escape(desc, quote=True), cabeca, count=1)
    cabeca = NAV_RE.sub(menu(lang, tipo, False), cabeca, count=1)
    # the language links lead to the same page in the other language
    cabeca = re.sub(r'(<nav class="language-nav".*?</nav>)', lambda m: re.sub(
        r'href="([^"]*)"', lambda h: 'href="%s"' % (h.group(1) + ("" if h.group(1).endswith(".html") else "") + tipo + ".html"
                                                     if not h.group(1).endswith(".html") else h.group(1)), m.group(1)), cabeca, count=1, flags=re.S)
    return (cabeca + '<main id="main" class="page-main">\n<section class="page-hero">\n  <h1>%s</h1>\n  <p class="page-lead">%s</p>\n</section>\n%s\n</main>' % (
        html.escape(h1, quote=False), html.escape(lead, quote=False), corpo) + rodape)


def main():
    capitulos = ler("tutorialChapters", "TutorialChapter")
    learn = ler("learnEntries", "LearnEntry")
    if len(capitulos) != 10:
        raise ValueError("UiHelp.h: expected 10 tutorial chapters, found %d" % len(capitulos))
    for lang in LANGS:
        i = ORDEM[lang]
        # Tutorial: each chapter = [title x4, body x4]
        corpo = "\n".join('<section class="page-section" id="chapter-%d"><h2>%s</h2>%s</section>' % (
            n + 1, html.escape(c[i], quote=False), "".join("<p>%s</p>" % html.escape(p, quote=False) for p in c[4 + i].split("\n\n")))
            for n, c in enumerate(capitulos))
        (AQUI / PASTA[lang] / "tutorial.html").write_text(pagina(lang, "tutorial", corpo), encoding="utf-8")
        # Learn: each entry = [key, title x4, body x4] (the key is a string_view literal too)
        itens = []
        for e in learn:
            if len(e) < 9:
                continue
            itens.append('<div class="page-learn"><h3>%s</h3><p>%s</p></div>' % (html.escape(e[1 + i], quote=False), html.escape(e[5 + i], quote=False)))
        corpo = '<section class="page-section"><div class="page-learn-grid">%s</div></section>' % "\n".join(itens)
        (AQUI / PASTA[lang] / "aprender.html").write_text(pagina(lang, "aprender", corpo), encoding="utf-8")
        corpo = "\n".join('<section class="page-section"%s><h2>%s</h2>%s</section>' % (
            ' id="download"' if n == 0 else "", html.escape(h2, quote=False), blocos(lista)) for n, (h2, lista) in enumerate(INST[lang]))
        (AQUI / PASTA[lang] / "instalacao.html").write_text(pagina(lang, "instalacao", corpo), encoding="utf-8")
        casa = AQUI / PASTA[lang] / "index.html"
        casa.write_text(NAV_RE.sub(menu(lang, "", True), casa.read_text(encoding="utf-8"), count=1), encoding="utf-8")
    print("paginas_extra.py: 12 pages (tutorial, aprender, instalacao × 4), menu in 4 homes; %d LEARN entries" % len(learn))


if __name__ == "__main__":
    main()
