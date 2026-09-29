#!/usr/bin/env python3
"""Generate the static PT/FR/ES editions of the Navalha 2 site.

Canonical sources stay exactly as before: `index.html` (layout + English
copy) and `assets/site-locales.js` (the PT/FR/ES dictionaries). This script
applies the dictionaries the same way `assets/localized-page.js` did in the
browser, but ahead of time, and writes the result into the body of
`pt/index.html`, `fr/index.html` and `es/index.html`.

Why: the runtime loader shipped each localized route as an empty body
("Loading…") that JavaScript filled in later. Google runs JavaScript; most
AI readers, link previews and no-JS visitors only read the raw HTML, so for
them the three editions were empty. (29-30 Sep 2026.)

Rules reproduced from localized-page.js:
- each text node whose whitespace-normalized value is a dictionary key is
  replaced by the translation (whole node, as `node.nodeValue = ...`);
- `aria-label`, `title` and `alt` attributes likewise;
- the language-nav link of the current locale gets `class="active"` and
  `aria-current="page"`, the others lose them.

What changes on purpose: the localized pages no longer use
`<base href="../">`. Relative URLs are rewritten with a `../` prefix instead,
so same-page anchors (`#workflow`) work natively — the loader had to
intercept clicks to undo the side effect of <base>.

Each localized page keeps its own <head> (localized title/description, the
seo.py block); only the loader scripts are swapped for `site.js`, which the
English page loads from its head. Run after editing index.html or the
dictionaries, then `python3 seo.py`:

    python3 gerar_idiomas.py            write the three pages
    python3 gerar_idiomas.py --check    exit 1 if any page is stale
"""

import html
import json
import pathlib
import re
import sys
from html.parser import HTMLParser

HERE = pathlib.Path(__file__).parent
LOCALES = ("pt", "fr", "es")
TRANSLATABLE_ATTRS = ("aria-label", "title", "alt")
URL_ATTRS = ("href", "src")
RAW_TEXT = {"script", "style"}
VOID = {"area", "base", "br", "col", "embed", "hr", "img", "input", "link",
        "meta", "source", "track", "wbr"}


def load_dictionaries():
    """Read site-locales.js: the main object plus every later
    Object.assign(window.NavalhaSiteLocales.xx, {...}) extension."""
    src = (HERE / "assets" / "site-locales.js").read_text(encoding="utf-8")

    def to_json(obj):
        obj = re.sub(r"^(\s*)(pt|fr|es)\s*:", r'\1"\2":', obj, flags=re.M)
        return json.loads(re.sub(r",(\s*[}\]])", r"\1", obj))

    def balanced(start):
        depth, i, in_str = 0, start, False
        while True:
            c = src[i]
            if in_str:
                if c == "\\":
                    i += 1
                elif c == '"':
                    in_str = False
            elif c == '"':
                in_str = True
            elif c == "{":
                depth += 1
            elif c == "}":
                depth -= 1
                if depth == 0:
                    return src[start:i + 1]
            i += 1

    head = src.index("window.NavalhaSiteLocales = {")
    dicts = to_json(balanced(src.index("{", head)))
    for m in re.finditer(r"Object\.assign\(window\.NavalhaSiteLocales\.(\w+),\s*\{", src):
        dicts[m.group(1)].update(to_json(balanced(m.end() - 1)))
    return dicts


def normalize(value):
    return " ".join(value.split())


def rewrite_url(url):
    """The page moved one directory down and lost <base href="../">."""
    if not url or url.startswith(("#", "/", "http:", "https:", "mailto:", "tel:", "data:", "javascript:")):
        return url
    return "../" if url in ("./", ".") else "../" + url


class Localizer(HTMLParser):
    """Re-emits a document, translating text/attributes and rewriting URLs.
    Markup that needs no change is copied verbatim."""

    def __init__(self, table, locale):
        super().__init__(convert_charrefs=True)
        self.table, self.locale = table, locale
        self.out, self.stack = [], []
        self.nav_depth = None  # set by NavTracker inside <nav class="language-nav">

    def translate(self, text):
        key = normalize(text)
        return self.table.get(key) if key else None

    def handle_starttag(self, tag, attrs):
        self.stack.append(tag)
        self.emit_tag(tag, attrs, self.get_starttag_text())
        if tag in VOID:
            self.stack.pop()

    def handle_startendtag(self, tag, attrs):
        self.emit_tag(tag, attrs, self.get_starttag_text())

    def emit_tag(self, tag, attrs, raw):
        attrs = [list(a) for a in attrs]
        changed = False
        for a in attrs:
            name, value = a
            if value is None:
                continue
            if name in TRANSLATABLE_ATTRS and self.translate(value):
                a[1], changed = self.translate(value), True
            elif name in URL_ATTRS and rewrite_url(value) != value:
                a[1], changed = rewrite_url(value), True
            elif name == "srcset":
                new = ", ".join(rewrite_url(p.strip().split(" ")[0]) +
                                p.strip()[len(p.strip().split(" ")[0]):]
                                for p in value.split(","))
                if new != value:
                    a[1], changed = new, True
        if tag == "a" and self.in_language_nav():
            changed |= self.mark_language(attrs)
        if not changed:
            self.out.append(raw)
            return
        parts = [tag] + [n if v is None else '%s="%s"' % (n, html.escape(v, quote=True))
                         for n, v in attrs]
        self.out.append("<%s%s>" % (" ".join(parts), " /" if raw.rstrip().endswith("/>") else ""))

    def in_language_nav(self):
        return self.nav_depth is not None

    def mark_language(self, attrs):
        lang = dict(attrs).get("lang")
        classes = (dict(attrs).get("class") or "").split()
        before = [a[:] for a in attrs]
        active = lang == self.locale
        if active and "active" not in classes:
            classes.append("active")
        if not active and "active" in classes:
            classes.remove("active")
        rest = [a for a in attrs if a[0] not in ("class", "aria-current")]
        new = rest + ([["class", " ".join(classes)]] if classes else [])
        if active:
            new.append(["aria-current", "page"])
        attrs[:] = new
        return attrs != before

    def handle_endtag(self, tag):
        if self.stack and self.stack[-1] == tag:
            self.stack.pop()
        self.out.append("</%s>" % tag)

    def handle_data(self, data):
        if self.stack and self.stack[-1] in RAW_TEXT:
            self.out.append(data)
            return
        new = self.translate(data)
        self.out.append(html.escape(new if new else data, quote=False))

    def handle_comment(self, data):
        self.out.append("<!--%s-->" % data)

    def handle_decl(self, decl):
        self.out.append("<!%s>" % decl)


class NavTracker(Localizer):
    """Adds tracking of <nav class="language-nav"> to the Localizer."""

    def handle_starttag(self, tag, attrs):
        if tag == "nav" and "language-nav" in (dict(attrs).get("class") or "").split():
            self.nav_depth = len(self.stack)
        super().handle_starttag(tag, attrs)

    def handle_endtag(self, tag):
        super().handle_endtag(tag)
        if tag == "nav" and self.nav_depth is not None and len(self.stack) <= self.nav_depth:
            self.nav_depth = None


def localized_body(canonical, table, locale):
    body = canonical[canonical.index("<body"):canonical.rindex("</body>") + len("</body>")]
    p = NavTracker(table, locale)
    p.feed(body)
    p.close()
    return "".join(p.out)


def localized_head(page, canonical):
    head = page[:page.index("<body")]
    head = re.sub(r'[ \t]*<base href="\.\./">\n', "", head)
    # the loader and its dictionary are no longer needed at runtime
    head = re.sub(r'[ \t]*<script src="assets/(site-locales|localized-page)\.js[^"]*" defer></script>\n', "", head)
    site_js = re.search(r'<script src="(assets/site\.js[^"]*)" defer></script>', canonical).group(1)
    # remaining relative URLs in the head (stylesheet, favicon)
    head = re.sub(r'(<link [^>]*href=")(?!https?:|/|\.\./)([^"]+)"', r'\1../\2"', head)
    # site.js, which the English page loads from its own head; kept right
    # after the stylesheet so the seo.py block stays last
    head = re.sub(r'[ \t]*<script src="\.\./assets/site\.js[^"]*" defer></script>\n', "", head)
    head = re.sub(r'(\n([ \t]*)<link rel="stylesheet"[^\n]*\n)',
                  lambda m: m.group(1) + '%s<script src="../%s" defer></script>\n' % (m.group(2), site_js),
                  head, count=1)
    return head


def main():
    check = "--check" in sys.argv
    tables = load_dictionaries()
    canonical = (HERE / "index.html").read_text(encoding="utf-8")
    stale = []
    for locale in LOCALES:
        path = HERE / locale / "index.html"
        page = path.read_text(encoding="utf-8")
        new = localized_head(page, canonical) + localized_body(canonical, tables[locale], locale) + "\n</html>\n"
        if new != page:
            stale.append("%s/index.html" % locale)
            if not check:
                path.write_text(new, encoding="utf-8")
    if check:
        if stale:
            print("gerar_idiomas.py: stale — run `python3 gerar_idiomas.py`: " + ", ".join(stale))
            sys.exit(1)
        print("gerar_idiomas.py: PT/FR/ES up to date")
    else:
        print("gerar_idiomas.py: %d page(s) written%s" % (len(stale), (": " + ", ".join(stale)) if stale else ""))


if __name__ == "__main__":
    main()
