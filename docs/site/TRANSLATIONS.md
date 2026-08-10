# Navalha 2 website localization — activity record

Date: 2026-08-10  
Status: implementation complete; editorial review open

## Objective

Provide internal EN/PT/FR/ES editions without Google Translate, browser overlays
or duplicated layout sources.

## Canonical source and routes

- English canonical source: `index.html`
- Portuguese: `/pt/`
- French: `/fr/`
- Spanish: `/es/`
- Shared localized content: `assets/site-locales.js`
- Shared loader: `assets/localized-page.js`

Technical vocabulary such as SOURCE, BLADE, SLICE, TRACE, FORM, PLAY, STOP and
REC remains stable where it identifies controls or concepts of the instrument.

## Evidence

- JavaScript syntax checks passed for the locale dictionary and loader.
- `git diff --check` passed.
- Chromium rendered EN/PT/FR/ES at 1280 × 900.
- Each route displayed its own localized hero copy and active language link.
- Canonical layout, favicon, CSS, images and interactive script loaded locally.
- No runtime request to Google Translate or another translation service exists.

The translated copy remains marked `draft/review` until authorial and linguistic
proofreading closes the editorial approval gate.
