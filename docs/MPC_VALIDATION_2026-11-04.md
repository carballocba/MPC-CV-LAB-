# Validación ARM en MPC One · 2026-11-04

En la unidad física, Linux armv7l y glibc 2.39. El candidato Zig ARMv7 probado tiene SHA-256 `e0f9a51b2236cefbbb85c409c11dff9b4201a6ecd607d5059153ba51618e5957`.

Se transfirió el candidato y los tres ejecutables de prueba por SSH a un directorio temporal aislado con limpieza automática. Allí pasaron `vst_host_test`, `vst_capture_test` y `vst_capture_demo`. El host sintético en la MPC validó carga dinámica, concurrencia, descarga y ocho recargas, además de 1.446 paquetes capturados. El proceso MPC siguió presente con el mismo PID. La prueba no registró el VST en Akai, cambió settings ni abrió el puerto CV.

## Instalador y backend

Se encontraron dos gestores en el área de Synths, OpenMPC Plugin Manager G2 y poloq Plugin Manager. Ambos tienen manifiesto y biblioteca. `MPC.settings` existe y contiene referencias a Plugin Manager. No contiene coincidencias de MPC-CV o McCv. Hay espacio disponible en las particiones inspeccionadas. La coexistencia por sí sola no demuestra un conflicto. Todavía falta reproducir el fallo y obtener el error del gestor. El instalador de Glass Spheres describe detener y volver a iniciar MPC, por lo que no se ejecutó sobre la sesión activa. [Glass Spheres](https://github.com/padbangers/glass-spheres) · [Open MPC](https://github.com/open-mpc/mpc-vst-plugins) · [Plugin Manager](https://github.com/poloq-instruments/mpc-vst-manager)

El proceso MPC tiene dos descriptores con acceso de escritura hacia el mismo rawmidi. Glass Spheres distingue su candidato con `SNDRV_RAWMIDI_IOCTL_INFO` y busca una salida cuyo subnombre contiene Private. Se necesita validar la identidad exacta, la vida útil y la coordinación de escritura antes de usar un descriptor compartido. Meta4MPCXXL sigue como referencia separada. [Meta4MPCXXL](https://github.com/sunskiefer/Meta4MPCXXL)

Carga en Akai, backend real, ocho salidas físicas, clock, gates, modulación y ceros al cierre eléctrico quedan pendientes. No hay release.
