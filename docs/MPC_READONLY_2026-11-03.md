# MPC One · diagnóstico de solo lectura · 2026-11-03

SSH con la clave de host ya conocida. Linux `6.18.44-az01-2026-08-10-rt6` en `armv7l`. El sistema expone `/lib/ld-linux-armhf.so.3`. El proceso activo es `/usr/bin/MPC` y su cliente ALSA figura como `MPC 3`. La versión exacta de Hakai y la versión de glibc quedan por confirmar.

| Fuente | Observación |
| --- | --- |
| ALSA raw MIDI | Tarjeta 0 `MPC One MIDI`, dispositivo 0, `/dev/snd/midiC0D0` |
| ALSA sequencer | Cliente 16, puerto 1 `MPC Studio Live Private` |
| Estado raw MIDI | `Output 1` y `Input 1` ya tienen como propietario el PID de `/usr/bin/MPC` |
| Otros dispositivos | Tarjeta 1 `ACVA`, playback y capture de audio |

Los índices son los de esta sesión. La coincidencia del puerto Private y el substream 1 es una pista para el backend futuro. El propietario existente hace necesario resolver el acceso compartido con el host antes de intentar abrir un segundo writer. El inventario no prueba la codificación CV, el acceso desde el VST ni la tensión de salida.

Se leyó metadata de ALSA sin abrir dispositivos MIDI. La instalación sigue en pausa mientras se resuelve Community Installer. El VST se ejecutó en el host de pruebas local mediante `make check`. La captura simulada validó 1446 paquetes, 8 salidas, 64 flancos y ceros finales.
