# Installation interne — Navalha 2 sous Linux

Paquet validé : `navalha2_0.1.0_x86_64.deb`.

Consultez les [prérequis Linux](CONFIGURATION_MINIMALE_LINUX_FR.md) avant
l'installation.

## Avant l'installation

- utilisez un système Linux 64 bits `amd64` / `x86_64` ;
- copiez le fichier `.deb` dans un dossier local, par exemple `Téléchargements` ;
- fermez toute fenêtre Navalha 2 ouverte.

Le paquet interne le plus compatible est produit par le workflow GitHub
**Package Debian (Ubuntu 22.04)**. Il convient à Ubuntu 22.04 ou plus récent et
aux systèmes compatibles basés sur Debian/Ubuntu. Si `apt` signale une
dépendance indisponible, ne forcez pas l'installation : indiquez plutôt la
distribution et sa version.

## Installer

Dans un terminal, placé dans le dossier qui contient le paquet :

```bash
sudo apt install ./navalha2_0.1.0_x86_64.deb
```

Ouvrez ensuite **Navalha 2** depuis le menu des applications ou exécutez :

```bash
"Navalha 2"
```

## Vérification rapide

1. Vérifiez que le nom et l’icône apparaissent dans le menu.
2. Ouvrez l’app et choisissez une sortie dans `AUDIO`.
3. Chargez un WAV dans `SOURCE A`, appuyez sur `PLAY`, puis sur `STOP`.
4. Fermez et rouvrez l’app une fois.

Si l’application ne démarre pas, exécutez `"Navalha 2"` dans un terminal et
envoyez la sortie.

## Désinstaller

```bash
sudo apt remove navalha2
```

Cette commande supprime l’application, pas les projets, prises ou fichiers
audio de l’utilisateur.
