#!/usr/bin/env python3
"""Metadados de busca e compartilhamento do site do Navalha 2.

Todo endereço gerado aqui é absoluto, então vale igual para a raiz e para
pt/, fr/ e es/. Rodar depois de gerar_idiomas.py.
"""

SITE = {
    "base": "https://navalha2.arquiviagem.net/",
    "nome": "Navalha 2",
    # 1200×630, derivada de assets/navalha2-juce-interface.jpg
    "imagem": "assets/og-navalha2.jpg",
    "imagem_alt": {
        "en": "Navalha 2 interface: A/B sources, signal map and performance controls.",
        "pt": "Interface do Navalha 2: fontes A/B, mapa de sinal e controles de performance.",
        "pt-BR": "Interface do Navalha 2: fontes A/B, mapa de sinal e controles de performance.",
        "fr": "Interface de Navalha 2 : sources A/B, carte de signal et contrôles de performance.",
        "es": "Interfaz de Navalha 2: fuentes A/B, mapa de señal y controles de performance.",
    },
    # O site é a raiz do domínio navalha2.arquiviagem.net: robots.txt vale.
    "raiz_do_dominio": True,
    # Google verifica pelo DNS do domínio arquiviagem.net; o Bing não
    # importa propriedade de domínio, então verifica este site pela tag.
    "bing_verificacao": "CF11609D6DCF239E35F913D512DC3267",
}

VERSAO = "0.1.3"

GRUPOS = [
    [
        {"arquivo": "index.html", "url": "", "lang": "en"},
        {"arquivo": "pt/index.html", "url": "pt/", "lang": "pt"},
        {"arquivo": "fr/index.html", "url": "fr/", "lang": "fr"},
        {"arquivo": "es/index.html", "url": "es/", "lang": "es"},
    ],
    [
        {"arquivo": "relatorio-migracao.html", "url": "relatorio-migracao.html", "lang": "pt-BR"},
    ],
]


def dados_estruturados(pagina, descricao):
    repo = "https://github.com/lucioaraujo/navalha2-juce"
    # Linhagem como o próprio site a declara: Glerm Soares é o autor do
    # instrumento original; Lúcio Araújo dirige o Navalha 2.
    return {
        "@context": "https://schema.org",
        "@type": "SoftwareApplication",
        "name": "Navalha 2",
        "description": descricao,
        "url": SITE["base"],
        "inLanguage": pagina["lang"],
        "image": SITE["base"] + SITE["imagem"],
        "applicationCategory": "MultimediaApplication",
        "applicationSubCategory": "Sampler / live slicing instrument",
        "operatingSystem": "Linux, Windows, macOS",
        "softwareVersion": VERSAO,
        "downloadUrl": repo + "/releases/tag/v" + VERSAO,
        "offers": {"@type": "Offer", "price": "0", "priceCurrency": "EUR"},
        "license": "https://www.gnu.org/licenses/gpl-3.0.html",
        "isAccessibleForFree": True,
        "author": {"@type": "Person", "name": "Glerm Soares"},
        "contributor": {"@type": "Person", "name": "Lúcio Araújo"},
        "isBasedOn": {"@type": "SoftwareApplication",
                      "name": "Navalha (Pure Data)",
                      "author": {"@type": "Person", "name": "Glerm Soares"}},
        "sameAs": [repo],
    }

# ---------------------------------------------------------------------------
# Daqui para baixo o código é o mesmo em todos os sites da família RASGO
# (portal, Rasgo Modular, Antitotem, Navalha 2). Só a configuração acima muda.
#
# O que faz: em cada página, um bloco entre <!-- SEO:INICIO --> e
# <!-- SEO:FIM -->, logo antes de </head>, com canonical, hreflang entre as
# versões de idioma, Open Graph / Twitter (prévia ao compartilhar) e JSON-LD
# (schema.org: o que o site É, para buscadores e IAs). Título e descrição
# são lidos da própria página — não há segunda cópia para divergir. Gera
# também sitemap.xml e, quando o site está na raiz de um domínio, robots.txt.
#
# Uso:  python3 seo.py              aplica
#       python3 seo.py --verificar  só confere; sai com 1 se algo mudaria
# ---------------------------------------------------------------------------

import html
import json
import pathlib
import re
import sys

AQUI = pathlib.Path(__file__).parent
INICIO = "<!-- SEO:INICIO — gerado por seo.py; editar lá, não aqui -->"
FIM = "<!-- SEO:FIM -->"
BLOCO = re.compile(r"[ \t]*<!-- SEO:INICIO.*?<!-- SEO:FIM -->\n", re.S)
OG_LOCALE = {"pt": "pt_BR", "pt-BR": "pt_BR", "en": "en_US", "fr": "fr_FR", "es": "es_ES"}


def url(caminho):
    return SITE["base"] + caminho


def ler_meta(texto, arquivo):
    t = re.search(r"<title>(.*?)</title>", texto, re.S)
    d = re.search(r'<meta name="description" content="(.*?)"', texto, re.S)
    if not t or not d:
        sys.exit("%s: falta <title> ou <meta name=\"description\">" % arquivo)
    limpa = lambda s: " ".join(html.unescape(s).split())
    return limpa(t.group(1)), limpa(d.group(1))


def json_ld(dados):
    # "</" dentro de <script> encerraria o bloco antes da hora.
    texto = json.dumps(dados, ensure_ascii=False, indent=2).replace("</", "<\\/")
    return '<script type="application/ld+json">\n' + texto + "\n</script>"


def bloco(pagina, grupo, titulo, descricao):
    lang = pagina["lang"]
    a = lambda s: html.escape(s, quote=True)
    L = [INICIO, '<link rel="canonical" href="%s">' % url(pagina["url"])]
    if len(grupo) > 1:
        for p in grupo:
            L.append('<link rel="alternate" hreflang="%s" href="%s">' % (p["lang"], url(p["url"])))
        L.append('<link rel="alternate" hreflang="x-default" href="%s">' % url(grupo[0]["url"]))
    L += [
        '<meta property="og:type" content="website">',
        '<meta property="og:site_name" content="%s">' % a(SITE["nome"]),
        '<meta property="og:title" content="%s">' % a(titulo),
        '<meta property="og:description" content="%s">' % a(descricao),
        '<meta property="og:url" content="%s">' % url(pagina["url"]),
        '<meta property="og:image" content="%s">' % url(SITE["imagem"]),
        '<meta property="og:image:width" content="1200">',
        '<meta property="og:image:height" content="630">',
        '<meta property="og:image:alt" content="%s">' % a(SITE["imagem_alt"][lang]),
        '<meta property="og:locale" content="%s">' % OG_LOCALE[lang],
    ]
    for p in grupo:
        if p is not pagina:
            L.append('<meta property="og:locale:alternate" content="%s">' % OG_LOCALE[p["lang"]])
    L.append('<meta name="twitter:card" content="summary_large_image">')
    # Verificação do Google Search Console (propriedade "prefixo do URL"):
    # o Google só procura a tag na página inicial.
    if SITE.get("google_verificacao") and pagina["url"] == "":
        L.append('<meta name="google-site-verification" content="%s">'
                 % a(SITE["google_verificacao"]))
    # Verificação do Bing Webmaster Tools, mesma regra.
    if SITE.get("bing_verificacao") and pagina["url"] == "":
        L.append('<meta name="msvalidate.01" content="%s">' % a(SITE["bing_verificacao"]))
    L.append(json_ld(dados_estruturados(pagina, descricao)))
    L.append(FIM)
    return L


def aplicar(texto, linhas):
    texto = BLOCO.sub("", texto)
    fecha = re.search(r"^([ \t]*)</head>", texto, re.M)
    if not fecha:
        raise ValueError("sem </head>")
    # recuo das linhas do <head>, para o bloco ler como o resto da página
    recuo = re.search(r"^([ \t]*)<title>", texto, re.M).group(1)
    corpo = "".join(recuo + l.replace("\n", "\n" + recuo) + "\n" for l in linhas)
    return texto[: fecha.start()] + corpo + texto[fecha.start():]


def sitemap():
    L = ['<?xml version="1.0" encoding="UTF-8"?>',
         '<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9"',
         '        xmlns:xhtml="http://www.w3.org/1999/xhtml">']
    for grupo in GRUPOS:
        for p in grupo:
            L.append("  <url>")
            L.append("    <loc>%s</loc>" % url(p["url"]))
            if len(grupo) > 1:
                for q in grupo:
                    L.append('    <xhtml:link rel="alternate" hreflang="%s" href="%s"/>'
                             % (q["lang"], url(q["url"])))
                L.append('    <xhtml:link rel="alternate" hreflang="x-default" href="%s"/>'
                         % url(grupo[0]["url"]))
            L.append("  </url>")
    L.append("</urlset>")
    return "\n".join(L) + "\n"


def robots():
    return "User-agent: *\nAllow: /\n\nSitemap: %s\n" % url("sitemap.xml")


def main():
    so_conferir = "--verificar" in sys.argv
    saidas = {}
    for grupo in GRUPOS:
        for p in grupo:
            caminho = AQUI / p["arquivo"]
            texto = caminho.read_text(encoding="utf-8")
            titulo, descricao = ler_meta(texto, p["arquivo"])
            saidas[caminho] = (texto, aplicar(texto, bloco(p, grupo, titulo, descricao)))
    for nome, gerar in (("sitemap.xml", sitemap), ("robots.txt", robots)):
        if nome == "robots.txt" and not SITE["raiz_do_dominio"]:
            continue
        caminho = AQUI / nome
        antes = caminho.read_text(encoding="utf-8") if caminho.exists() else ""
        saidas[caminho] = (antes, gerar())
    if not (AQUI / SITE["imagem"]).exists():
        sys.exit("imagem de compartilhamento ausente: %s" % SITE["imagem"])

    mudaria = [c.name if c.parent == AQUI else str(c.relative_to(AQUI))
               for c, (antes, depois) in saidas.items() if antes != depois]
    if so_conferir:
        if mudaria:
            print("seo.py: desatualizado — rode `python3 seo.py`: " + ", ".join(mudaria))
            sys.exit(1)
        print("seo.py: %d páginas em dia" % sum(len(g) for g in GRUPOS))
        return
    for c, (antes, depois) in saidas.items():
        if antes != depois:
            c.write_text(depois, encoding="utf-8")
    print("seo.py: %d arquivos atualizados%s" % (len(mudaria), (": " + ", ".join(mudaria)) if mudaria else ""))


if __name__ == "__main__":
    main()
