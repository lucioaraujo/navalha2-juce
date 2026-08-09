# Requisitos mínimos — Navalha 2 en Linux

## Paquete Debian interno

El artefacto `navalha2-deb-ubuntu-22-amd64` se compila en Ubuntu 22.04 para
ordenadores **amd64 / x86_64**. Está destinado a Ubuntu 22.04 o posterior,
Linux Mint basado en esas versiones y sistemas Debian/Ubuntu compatibles.

No es un paquete para ARM, Fedora, Arch u openSUSE. Use una futura distribución
específica para la plataforma o compile desde el código fuente.

## Ordenador

| Recurso | Mínimo práctico | Recomendado para uso musical |
| --- | --- | --- |
| Procesador | 2 núcleos, 2 GHz | procesador moderno de 4 núcleos |
| Memoria | 4 GB RAM | 8 GB RAM o más |
| Disco | 300 MB para app/bibliotecas, además de los WAV | SSD y varios GB para tomas/proyectos |
| Audio | salida estéreo ALSA, PipeWire o PulseAudio | interfaz USB con controlador ALSA |
| Sesión gráfica | X11 o Wayland con XWayland | sesión de escritorio actual |

Los WAV largos y bibliotecas grandes usan memoria y disco según el propio
material fuente.

## Pantalla

| Uso | Resolución |
| --- | --- |
| Ventana `PERFORM` | 900 × 560 como mínimo |
| Interfaz principal | 1480 × 900 mínimo práctico |
| Uso cómodo | 1920 × 1080 |
| Performance separada | segundo monitor opcional de 1920 × 1080 |

El segundo monitor es opcional; separa `PERFORM` del editor principal.

## Antes de instalar

1. Ejecute `uname -m`; el resultado esperado es `x86_64`.
2. Descargue el artefacto `.deb` de una ejecución exitosa de **Actions**.
3. Siga las [instrucciones en español](INSTALACION_DEB_INTERNA_ES.md).
