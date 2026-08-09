# Navalha 2 public site

Static, dependency-free first publication candidate for the Navalha 2 project.
English is the primary language in this version. PT/FR/ES remain planned, as
defined in `../PLANO_PAGINA_PROJETO.md`.

Serve the repository root locally so that links to sibling documentation work:

```sh
python3 -m http.server 8080
```

Then open `http://127.0.0.1:8080/docs/site/`.

Before public deployment:

- capture clean JUCE single/dual-monitor screenshots without desktop context;
- validate the final repository visibility and both GitHub links;
- run the HTML/link/accessibility checks;
- repeat visual checks at mobile, tablet and wide desktop widths;
- translate the approved English copy to PT/FR/ES.

The page deliberately describes the JUCE application as a migration in
validation and preserves PD/web v0.28.1 as the functional reference.
