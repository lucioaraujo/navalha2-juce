# Instalación interna — Navalha 2 en Linux

Paquete validado: `navalha2_0.1.0_x86_64.deb`.

Consulte los [requisitos mínimos de Linux](REQUISITOS_MINIMOS_LINUX_ES.md) antes
de instalar.

## Antes de instalar

- use un sistema Linux de 64 bits `amd64` / `x86_64`;
- copie el archivo `.deb` a una carpeta local, como `Descargas`;
- cierre cualquier ventana abierta de Navalha 2.

El paquete interno más compatible se genera con el flujo GitHub **Package
Debian (Ubuntu 22.04)**. Sirve para Ubuntu 22.04 o posterior y sistemas
compatibles basados en Debian/Ubuntu. Si `apt` informa una dependencia no
disponible, no fuerce la instalación: indique la distribución y su versión.

## Instalar

En una terminal dentro de la carpeta que contiene el paquete, ejecute:

```bash
sudo apt install ./navalha2_0.1.0_x86_64.deb
```

Después abra **Navalha 2** desde el menú de aplicaciones o ejecute:

```bash
"Navalha 2"
```

## Verificación rápida

1. Confirme que el nombre y el icono aparecen en el menú.
2. Abra la app y seleccione una salida en `AUDIO`.
3. Cargue un WAV en `SOURCE A`, pulse `PLAY` y después `STOP`.
4. Cierre y vuelva a abrir la app una vez.

Si la aplicación no abre, ejecute `"Navalha 2"` en una terminal y envíe la
salida.

## Desinstalar

```bash
sudo apt remove navalha2
```

Esto elimina la aplicación, no los proyectos, tomas o archivos de audio del
usuario.
