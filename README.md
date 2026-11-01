# MPC-CV LAB

Plataforma modular de modulación CV/Gate para **Akai MPC One con Hakai**, desarrollada en C++17. El objetivo es controlar ocho señales desde un solo VST2, con motores virtuales, routing, escenas y controles de performance.

**Estado: laboratorio offline. El plugin genera silencio y valores CV simulados. No contiene backend físico, no está instalado y no se ha probado en MPC.**

## Avance del 1 de noviembre de 2026

**ARM/QEMU ya pasó:** siete pruebas, incluidos el VST cargado dinámicamente y escenas Python/C++, con toolchains GNU y Zig. También se añadieron una FIFO de gates con timestamps y watchdog, aplicación coherente de escenas y ocho interruptores de salida en Overview. El pipeline aún es independiente del VST y no accede a hardware.

Ver [informe actual y evidencia](docs/PROGRESS_2026-11-01.md).

## Qué funciona

- Phase 0 consolidada; investigación histórica y código original preservados.
- Grafo de hasta 16 nodos virtuales, independiente de ocho salidas lógicas. Cuatro salidas habilitadas inicialmente en el VST; el simulador ampliado ejercita las ocho.
- Sine/triangle LFO, gate, secuenciador de ocho pasos, random hold determinista, mezcla, slew y cuantización. Modulación de velocidad entre nodos; rechazo de ciclos y referencias inválidas.
- Escala, offset, inversión/atenuación mediante escala y límites de 0–5 V por salida.
- VST2 propio: identidad `McCv`, parámetros, transporte/start-stop, bucles, estado versionado y recuperación desarmada. Build nativo Mac y cross-build ARMv7 Linux hard-float.
- Skin nativa MPC con OVERVIEW, ENGINES, ROUTING, DEVICES, SCENES y SETTINGS, mapas Q-Link y diagnóstico simulado. Generada y revisada offline; comportamiento táctil y Q-Links pendientes de hardware.
- Backend abstracto, captura de mensajes, recuperación de escrituras parciales y modelo de apagado con ceros. Son modelos probados, no un worker físico en el VST.

No se implementaron todas las familias futuras de motores. ADSR/AD/AR, Euclidean, chaos, clock divider dedicado, MIDI de entrada y scene morphing quedan pendientes. DEVICES y SCENES son páginas iniciales informativas, no navegadores completos.

## Arquitectura

`Host MPC → adaptador VST2 → grafo de señales virtuales → rutas y límites → valores simulados / telemetría`

El simulador offline conecta además el grafo al servicio de captura de CC. La FIFO de flancos gate con timestamps está implementada y probada por separado. Su integración en el VST/coordinator y el backend MPC Private siguen pendientes. No hay dependencia de módulos Eurorack concretos ni del DSP de Glass Spheres.

Engine Preset configura un motor; Device Profile describe entradas de un módulo externo; Performance Scene conserva grafo, parámetros, rutas, máscara y metadatos de sincronización/control. La carga valida todo antes de aplicarlo y desarma la simulación. Cambiar parámetros puede reiniciar estados de slew; no se promete transición continua.

## Compilar y probar

Con clang++, make y Python 3, sin SDK VST comercial ni hardware:

```sh
make check
```

Incluye tests del núcleo/grafo/backend, carga dinámica del VST, procesamiento con control de asignaciones de memoria, contratos UI/perfiles y simulación/captura de ocho salidas. En macOS `build/mpc_cv.so` es Mach-O arm64; el sufijo `.so` no lo convierte en un binario MPC.

```sh
make -B check CXXFLAGS='-std=c++17 -O1 -g -Wall -Wextra -Wpedantic -Werror -Iinclude -pthread -fsanitize=address,undefined'
ZIG=/ruta/absoluta/a/zig-0.15.2/zig tools/build_armv7.sh
```

El segundo comando produce `build/armv7/mpc_cv.so` y siete ejecutables ARM; el script de build local no los ejecuta. La CI Linux sí los ejecuta bajo QEMU con GNU y Zig. Requiere Zig 0.15.2 verificado; `python3 tools/fetch_zig.py` lo descarga con SHA-256 fijo dentro de build/toolchains, sin instalación global. Nada se copia al dispositivo. Ver [build ARM y resultados](docs/VALIDATION.md) y [generación de skin](docs/UI.md).

## Evidencia y límites

El usuario informa que Glass Spheres controla su Eurorack desde MPC One/Hakai. Esto apoya la viabilidad, pero **no valida MPC-CV**, sus ocho canales, voltajes, jitter ni shutdown. Akai documenta cuatro conectores TRS y ocho señales con breakout; el proyecto aún no midió el mapa tip/ring ni calibración.

El protocolo candidato usa canal MIDI 16 y pares CC 0/1 hasta 14/15, 14 bits y rango nominal unipolar 0–5 V. Las pruebas nativas y la compilación ARM no prueban que escribir al puerto compartido sea seguro. Tampoco garantizan precisión de afinación o tiempo real.

## Documentación

- [Estado actual y pruebas ARM/QEMU](docs/PROGRESS_2026-11-01.md)
- [Autoría de escenas y presets JSON](docs/SCENE_FORMAT.md)
- [Validación e historial](docs/VALIDATION.md)
- [Arquitectura actual](docs/ARCHITECTURE.md)
- [Investigación y contraste de fuentes](docs/RESEARCH.md)
- [Auditoría de tiempo real](docs/RT_AUDIT.md)
- [GUI y Q-Links](docs/UI.md)
- [Auditoría de licencias](docs/LICENSE_AUDIT.md)
- [Preparación para una futura prueba física](docs/HARDWARE_READINESS.md)
- [Plan histórico original, sin cambios](docs/RESEARCH_PLAN.md)
- [Phase 0 original](archive/phase0/README.md)

## Roadmap inmediato

1. Integrar la FIFO de eventos y el coordinator de captura sin habilitar salidas físicas.
2. Completar perfiles y navegador de escenas; la autoría de escenas/presets JSON ya funciona offline.
3. Probar descubrimiento, reconexión, reservas multiinstancia y ciclo de vida del futuro worker con fixtures.
4. Revisar licencias de runtime y empaquetado antes de distribuir binarios.
5. Solicitar autorización separada para una prueba supervisada de una salida.

Repositorio oficial: https://github.com/carballocba/MPC-CV-LAB-. No se modifica `main` ni se instala nada en la MPC desde este laboratorio.
