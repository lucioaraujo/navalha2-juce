# Navalha 2 public site

Static, dependency-free first publication candidate for the Navalha 2 project.
English is the primary language in this version. The header includes PT/FR/ES
full-page translation links as an interim reading aid. They are intentionally
kept separate from the original English source; editorially reviewed static
PT/FR/ES pages remain a future documentation task.

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
- translate the approved English copy to PT/FR/ES.

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
