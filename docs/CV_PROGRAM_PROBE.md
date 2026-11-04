# Prueba de CV1 a través de Akai CV Program

El 4 de noviembre se ejecutó el probe ARMv7 en la MPC. ALSA registró `MPC-CV LAB Probe / CV1 diagnostic` y el proceso MPC se suscribió al puerto, sin emisión inicial. Queda por comprobar el ruteo a una pista CV, su respuesta eléctrica y la puesta a cero. El candidato VST instalado conserva captura en memoria. El probe es un paso para validar el transporte antes de integrar la salida en el VST.

## Selección del transporte

Glass Spheres usa el descriptor que ya abrió MPC. Busca un rawmidi de salida con nombre Private mediante `SNDRV_RAWMIDI_IOCTL_INFO`, sin hacer una apertura independiente. Para cada salida indexada desde cero escribe dos CC en canal MIDI 16, con controladores `2j` y `2j+1`, primero los siete bits altos y luego los bajos. Un worker reúne cambios y deja al menos 2 ms entre escrituras, con redescubrimiento limitado a una vez por segundo. El código vuelve a buscar el descriptor tras EBADF. No completa escrituras parciales ni arbitra las escrituras de Akai. Su sincronización cubre instancias propias. [Implementación de Glass Spheres](https://github.com/padbangers/glass-spheres/blob/1fe3b0a17b14dc07d327e5408d7aaf438715ce44/src/cv_out.cc)

El probe usa ALSA Sequencer, con un puerto fuente propio y entrega a sus suscriptores. El CV Program de Akai documenta Mod Wheel como una salida asignable de 0–5 V. Por eso la prueba envía CC1 en el canal 1, con consignas de 0, 25, 51 y 76 sobre 127. Los voltajes son nominales. La resolución MIDI y el mapeo real de Akai determinan la precisión. [Programas CV de Akai](https://support.akaipro.com/en/support/solutions/articles/69000804211-akai-pro-mpc-series-understanding-and-loading-programs)

## Prueba eléctrica

Usar un proyecto de prueba. Seleccionar el puerto del probe como entrada MIDI de la pista CV, canal 1 y monitoreo IN. Asignar solo Mod Wheel a CV1. Dejar Pitch, Gate y Velocity en Off. Confirmar que otros tracks no estén ruteados a ese puerto o salida.

Desconectar los equipos de las salidas CV. Insertar completamente el breakout TRS a doble TS en el conector 1/5. Poner el multímetro en V DC y medir entre tip y sleeve de la rama tip. Registrar primero la consigna de cero y detenerse si hay tensión inesperada. Continuar por 1, 2 y 3 V de forma escalonada. Para el cierre, el probe intenta CC1 cero al recibir EOF, una señal de terminación, una orden de cierre o al pasar 30 s sin entrada tras una consigna no nula. ALSA solo informa aceptación del evento. El retorno a cero requiere una lectura real con la pista todavía monitorizando. No usar esta prueba para afirmar que Akai pone a cero el DAC ante caída del proceso o pérdida de la suscripción.

## Compilación

```sh
ZIG_GLOBAL_CACHE_DIR="$PWD/build/zig-global" ZIG_LOCAL_CACHE_DIR="$PWD/build/zig-cache" \
  "$ZIG" cc -target arm-linux-gnueabihf.2.31 -mcpu=cortex_a9 -O2 \
  -Wall -Wextra -Werror tools/cv_program_probe.c -ldl -o build/armv7/cv_program_probe
```
