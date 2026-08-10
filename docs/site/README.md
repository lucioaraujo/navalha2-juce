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
- run the four language routes through the HTML/link/accessibility checks.

## Localization architecture

`index.html` is the canonical layout and English copy. Each localized route has
a small entry page with localized metadata. `assets/localized-page.js` loads the
canonical markup, applies the selected local dictionary and then initializes
the regular site behavior. Layout and content therefore do not fork into four
manually maintained HTML copies.

The editions are currently `draft/review`: their routes, navigation, assets and
responsive rendering are implemented and tested, while final linguistic review
remains an editorial approval step.

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
