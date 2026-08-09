# Configuration minimale — Navalha 2 sous Linux

## Paquet Debian interne

L’artefact `navalha2-deb-ubuntu-22-amd64` est construit sous Ubuntu 22.04 pour
les ordinateurs **amd64 / x86_64**. Il est destiné à Ubuntu 22.04 ou plus
récent, Linux Mint basé sur ces versions et aux systèmes Debian/Ubuntu
compatibles.

Ce n’est pas un paquet ARM, Fedora, Arch ou openSUSE. Utilisez une future
distribution spécifique à la plateforme ou compilez depuis le code source.

## Ordinateur

| Ressource | Minimum pratique | Recommandé pour la musique |
| --- | --- | --- |
| Processeur | 2 cœurs, 2 GHz | processeur moderne à 4 cœurs |
| Mémoire | 4 Go RAM | 8 Go RAM ou plus |
| Stockage | 300 Mo pour app/bibliothèques, plus les WAV | SSD et plusieurs Go pour prises/projets |
| Audio | sortie stéréo ALSA, PipeWire ou PulseAudio | interface USB avec pilote ALSA |
| Session graphique | X11 ou Wayland avec XWayland | session bureau actuelle |

Les WAV longs et grandes bibliothèques utilisent mémoire et stockage selon le
matériel source.

## Écran

| Usage | Résolution |
| --- | --- |
| Fenêtre `PERFORM` | 900 × 560 minimum |
| Interface principale | 1480 × 900 minimum pratique |
| Usage confortable | 1920 × 1080 |
| Performance détachée | second écran 1920 × 1080 facultatif |

Le second écran est facultatif ; il sépare `PERFORM` de l’éditeur principal.

## Avant l'installation

1. Exécutez `uname -m` ; le résultat attendu est `x86_64`.
2. Téléchargez l’artefact `.deb` d’une exécution **Actions** réussie.
3. Suivez les [instructions françaises](INSTALLATION_DEB_INTERNE_FR.md).
