# Validación ARM en MPC One · 2026-11-04

En la unidad física, Linux armv7l y glibc 2.39. El candidato Zig ARMv7 probado tiene SHA-256 `e0f9a51b2236cefbbb85c409c11dff9b4201a6ecd607d5059153ba51618e5957`.

Se transfirió el candidato y los tres ejecutables de prueba por SSH a un directorio temporal aislado con limpieza automática. Allí pasaron `vst_host_test`, `vst_capture_test` y `vst_capture_demo`. El host sintético en la MPC validó carga dinámica, concurrencia, descarga y ocho recargas, además de 1.446 paquetes capturados. El proceso MPC siguió presente con el mismo PID. La prueba no registró el VST en Akai, cambió settings ni abrió el puerto CV.

## Instalador y backend

Se encontraron dos gestores en el área de Synths, OpenMPC Plugin Manager G2 y poloq Plugin Manager. Ambos tienen manifiesto y biblioteca. `MPC.settings` existe y contiene referencias a Plugin Manager. No contiene coincidencias de MPC-CV o McCv. Hay espacio disponible en las particiones inspeccionadas. La coexistencia por sí sola no demuestra un conflicto. Todavía falta reproducir el fallo y obtener el error del gestor. El instalador de Glass Spheres describe detener y volver a iniciar MPC, por lo que no se ejecutó sobre la sesión activa. [Glass Spheres](https://github.com/padbangers/glass-spheres) · [Open MPC](https://github.com/open-mpc/mpc-vst-plugins) · [Plugin Manager](https://github.com/poloq-instruments/mpc-vst-manager)

El proceso MPC tiene dos descriptores con acceso de escritura hacia el mismo rawmidi. Glass Spheres distingue su candidato con `SNDRV_RAWMIDI_IOCTL_INFO` y busca una salida cuyo subnombre contiene Private. Se necesita validar la identidad exacta, la vida útil y la coordinación de escritura antes de usar un descriptor compartido. Meta4MPCXXL sigue como referencia separada. [Meta4MPCXXL](https://github.com/sunskiefer/Meta4MPCXXL)

Carga en Akai, backend real, ocho salidas físicas, clock, gates, modulación y ceros al cierre eléctrico quedan pendientes. No hay release.

## Identidad de RawMIDI confirmada en la unidad

`tools/probe_rawmidi.c` consultó `SNDRV_RAWMIDI_IOCTL_INFO` sobre referencias temporales al archivo abierto por MPC, mediante pidfd. El probe cerró las referencias y se eliminó del directorio temporal. No abrió un nuevo stream, suspendió el host ni leyó o escribió datos MIDI.

| Descriptor de esta sesión | Tarjeta y dispositivo | Substream de salida | Nombre |
| --- | --- | --- | --- |
| 38 | 0, 0 | 1 | MPC Studio Live Private |
| 39 | 0, 0 | 0 | MPC Studio Live Public |

`fdinfo` daba el mismo inode y los mismos flags para ambas salidas. No usar el orden de los fd para descubrirlas. La identificación por ioctl confirma el endpoint de Glass Spheres, sin confirmar exclusión mutua con las escrituras del host ni el efecto físico de los mensajes. La ruta candidata mediante un puerto MIDI virtual y una pista CV delegaría las escrituras al host. Requiere verificar en Akai si la pista CV de esta versión recibe el puerto y qué controles además de notas/gates expone. El inventario ALSA no demuestra un camino a los DAC por Sequencer.

## Paquete de prueba para carga en Akai

El gestor G2 presente es `0.2.5-hddfix.2`, UID `4f4d4732`; poloq es `1.2.0`, UID `506c4d67`. Están registrados por separado y el arranque de Akai encontró ambos. El journal inspeccionado no contiene una reproducción del fallo de instalación. Se preservaron ambos.

`tools/package_akai_candidate.sh` produce el ZIP local ARMv7 en formato portable Open MPC con UID `4d634376`, metadatos, skin y scripts generados por el framework revisado. Requiere el ELF ARMv7 compilado y Pillow para la skin. El ZIP pasó `unzip -t`. El registro y la eliminación se probaron sobre una copia efímera de la configuración actual: se agregó un solo plugin y el rollback dejó el archivo idéntico byte a byte. El archivo real quedó intacto. El paquete es local y conserva captura en memoria. El reinicio de la aplicación y la carga en un proyecto de prueba requieren que la sesión activa esté guardada.

### Instalación en la unidad

Con el proyecto guardado y el reinicio autorizado, el instalador portable registró el candidato en `/media/acvs-content/Synths`. En esta unidad `/sdcard/Synths` no existe y el destino seleccionado está en el almacenamiento ejecutable. `acvs` volvió a estado activo con un nuevo PID de MPC. El SHA-256 del ELF instalado coincide con el probado, y la entrada del UID aparece una vez. El journal confirma que ambos gestores siguieron presentes en el arranque.

Backup del instalador en la unidad: `/media/az01-internal/Settings/MPC/MPC.settings.bak-mpc_cv-20261104-141131`. Para retirar el candidato, usar el rollback del mismo paquete, apuntando a la misma carpeta Synths y verificando que solo afecta su UID y su directorio. Conservar el backup como recuperación de la transacción. El rollback se probó sobre copia, no sobre la instalación activa.

La instalación y el arranque de Akai están confirmados. Para verificar la carga efectiva, instanciar MPC-CV LAB en una pista Instrument de un proyecto de prueba y comprobar la interfaz y el mapeo de `mpc_cv.so` dentro del PID de MPC. El candidato instalado solo tiene captura en memoria.
