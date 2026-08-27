(() => {
  "use strict";

  const locale = document.documentElement.lang;
  const translations = window.NavalhaSiteLocales?.[locale];

  const normalize = (value) => value.replace(/\s+/g, " ").trim();

  const applyTranslations = (root) => {
    const walker = document.createTreeWalker(root, NodeFilter.SHOW_TEXT);
    const nodes = [];
    while (walker.nextNode()) nodes.push(walker.currentNode);
    nodes.forEach((node) => {
      const key = normalize(node.nodeValue || "");
      if (key && translations[key]) node.nodeValue = translations[key];
    });

    root.querySelectorAll("[aria-label], [title], [alt]").forEach((element) => {
      ["aria-label", "title", "alt"].forEach((attribute) => {
        const key = normalize(element.getAttribute(attribute) || "");
        if (key && translations[key]) element.setAttribute(attribute, translations[key]);
      });
    });
  };

  const load = async () => {
    if (!translations) throw new Error(`Unsupported locale: ${locale}`);
    const response = await fetch("index.html", { cache: "no-cache" });
    if (!response.ok) throw new Error(`Canonical page: HTTP ${response.status}`);
    const source = new DOMParser().parseFromString(await response.text(), "text/html");
    const body = document.importNode(source.body, true);

    body.querySelectorAll(".language-nav a").forEach((link) => {
      const active = link.getAttribute("lang") === locale;
      link.classList.toggle("active", active);
      if (active) link.setAttribute("aria-current", "page");
      else link.removeAttribute("aria-current");
    });

    applyTranslations(body);

    // <base href="../"> (needed above so bare "assets/..." src/href attributes
    // cloned from the canonical body still resolve from this subdirectory)
    // has a side effect: an in-page anchor like href="#workflow" no longer
    // targets this document, it targets ../#workflow - the canonical English
    // page - so clicking it navigated away from the localized edition instead
    // of scrolling. Intercept same-page hash links and scroll manually
    // instead of letting the browser resolve/navigate them against the base.
    body.addEventListener("click", (event) => {
      const link = event.target.closest('a[href^="#"]');
      if (!link || link.hasAttribute("data-contact-trigger")) return;
      const id = link.getAttribute("href").slice(1);
      event.preventDefault();
      if (!id) { window.scrollTo({ top: 0, behavior: "smooth" }); return; }
      document.getElementById(id)?.scrollIntoView({ behavior: "smooth", block: "start" });
    });

    document.body.replaceWith(body);

    const script = document.createElement("script");
    script.src = "assets/site.js?v=20260810-5";
    document.body.append(script);
  };

  load().catch((error) => {
    const message = {
      pt: "Não foi possível carregar esta edição.",
      fr: "Impossible de charger cette édition.",
      es: "No se pudo cargar esta edición."
    }[locale] || "Unable to load this edition.";
    document.body.innerHTML = `<main class="localized-error"><h1>Navalha 2</h1><p>${message}</p><a href="./">EN</a></main>`;
    console.error(error);
  });
})();
