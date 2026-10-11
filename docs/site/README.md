# Navalha 2 public site

Static, dependency-free publication candidate for the Navalha 2 project.
English is the canonical editorial source. Internal Portuguese, French and
Spanish editions reuse that layout through local translation dictionaries at
`assets/site-locales.js`; they do not call an external translation service at
runtime. Language routes are `/pt/`, `/fr/` and `/es/`.

Serve the repository root locally so that links to sibling documentation work:

```sh
python3 -m http.server 8080
```

Then open `http://127.0.0.1:8080/docs/site/`.

Before public deployment:

- enable **Settings → Pages → Source: GitHub Actions** in the repository;
- run the **Deploy public site** workflow once and confirm its GitHub Pages URL;
- capture clean JUCE single/dual-monitor screenshots without desktop context;
- run the HTML/link/accessibility checks and repeat visual checks at mobile,
  tablet and wide desktop widths;
- complete the authorial/editorial review of PT/FR/ES copy before marking those
  editions `approved`;
- expand and editorially review the `TAKES / MASTER` section so recording,
  review and mastering are represented as musical and editorial processes;
- expand the tutorial and contextual `LEARN` copy so learning is represented as
  practice within the instrument, not as a peripheral help feature;
- run the four language routes through the HTML/link/accessibility checks.

## Localization architecture

`index.html` is the canonical layout and English copy; `assets/site-locales.js`
holds the PT/FR/ES dictionaries. Layout and content do not fork into four
manually maintained HTML copies: **`gerar_idiomas.py` generates the body of
`pt/index.html`, `fr/index.html` and `es/index.html`** from those two sources.
Each localized page keeps its own `<head>` (localized title and description,
the `seo.py` block). After editing `index.html` or the dictionaries:

```sh
python3 gerar_idiomas.py && python3 seo.py
python3 gerar_idiomas.py --check   # exit 1 if a localized page is stale
```

Until 30 Sep 2026 the same translation happened in the browser:
`assets/localized-page.js` fetched `index.html` and applied the dictionary at
runtime, so the three localized routes shipped an empty body. Google runs
JavaScript; most AI readers, link previews and no-JS visitors do not, and saw
those editions empty. The generator reproduces the loader's rules exactly
(whole text node or `aria-label`/`title`/`alt` whose normalized value is a
dictionary key; active language link), and was checked against it: rendered
in Chromium, the loader output and the static pages have identical text and
attributes in PT, FR and ES, and pixel-identical layout apart from the SVG
animation frame. The static pages drop `<base href="../">` and prefix relative
URLs with `../` instead, so in-page anchors work natively.
`assets/localized-page.js` is no longer referenced and can be deleted.

The editions are currently `draft/review`: their routes, navigation, assets and
responsive rendering are implemented and tested, while final linguistic review
remains an editorial approval step.

## Fonts

`assets/fonts/` self-hosts Anton (display) and IBM Plex Mono (body/mono),
both SIL Open Font License 1.1, loaded via `@font-face` in `site.css`. Found
live, 26 ago. 2026: the previous `--display` stack (`Impact, Haettenschweiler,
"Arial Narrow Bold", sans-serif`) named only Windows-bundled proprietary
fonts with no embedded fallback, so the header rendered as intended-looking
bold/condensed on Windows but fell through to plain `sans-serif` on Linux/
macOS - a completely different look depending on the visitor's OS. Self-
hosting removes that OS dependency entirely; the site stays otherwise
dependency-free (no external font CDN call at runtime).

## Custom subdomain

After the exact subdomain is created at the DNS provider, add it in
**Settings → Pages → Custom domain** before creating the DNS record. For a
subdomain, create a `CNAME` record pointing directly to
`lucioaraujo.github.io` (without the repository name). Do not use wildcard DNS.
After propagation, enable HTTPS in GitHub Pages. The Actions deployment does
not require a `CNAME` file in this repository; GitHub stores the custom-domain
setting.

The page deliberately describes the JUCE application as a migration in
validation and preserves PD/web v0.28.1 as the functional reference.

## Search and sharing metadata — `seo.py`

Since 29 Sep 2026. Every page carries a block between `<!-- SEO:INICIO -->`
and `<!-- SEO:FIM -->` before `</head>`: canonical, hreflang across the four
languages, Open Graph/Twitter (link previews) and schema.org JSON-LD (what the
site is, for search engines and AI readers). Title and description are read
from the page itself. It also writes `sitemap.xml` and `robots.txt` (this site
is the domain root). **Do not edit the block by hand:** change the
configuration at the top of `seo.py` and run `python3 seo.py` (`--verificar`
only checks). The share image is `assets/og-navalha2.jpg`, 1200×630, derived
from `assets/navalha2-juce-interface.jpg`. The same `seo.py` exists on every
RASGO family site; only the configuration differs. `relatorio-migracao.html`
gained a `<meta name="description">` taken from its own subtitle.
