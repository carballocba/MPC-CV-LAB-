# Diagnóstico CV con multímetro

Estado eléctrico **PENDIENTE**. El generador crea escenas MCVS v1 para el host de pruebas actual. Cada escena limita una única ruta lógica a 0–3 V y se restaura desarmada. El VST actual produce una captura en memoria. La prueba con multímetro empieza cuando exista un backend físico validado para el primer ensayo y el propietario confirme que las ocho ramas están libres de módulos y de otros equipos.

```sh
python3 tools/cv_diagnostic.py scene 1 0 build/cv1-0v.mcvs
python3 tools/cv_diagnostic.py scene 1 1 build/cv1-1v.mcvs
python3 tools/cv_diagnostic.py report mediciones.csv
```

1. Desconectar Ornament & Crime, Pamela’s New Workout y cualquier otro destino CV. Usar un breakout TRS de 3,5 mm a dos TS, completamente insertado. Identificar cada rama por conector y lado del breakout. Verificar el mapa con las lecturas, sin deducirlo de los números CC.
2. Poner el multímetro en voltaje DC. Medir entre tip y sleeve de la rama TS. Evitar que las puntas puenteen contactos. Usar solo voltaje DC en salidas energizadas. Evitar conectores a medio insertar.
3. Para el primer canal, registrar el nivel inicial de 0 V y luego 1, 2 y 3 V. En cada nivel registrar las ocho ramas para comprobar independencia. Detener y cerrar desde una consigna no nula y registrar las ocho ramas por separado tras cada acción. Para probar el cierre desde una consigna activa, rearmar y verificar el nivel antes de cerrar. Si aparece señal en la rama equivocada o queda tensión al detener o cerrar, parar y registrar lo ocurrido antes de avanzar a otro canal.
4. Repetir por canal solo tras verificar el anterior. Probar niveles de gate bajo y alto mantenidos. El multímetro no caracteriza pulsos rápidos, jitter ni latencia. Probar los módulos por separado después de la comprobación con el multímetro y de verificar el rango de su entrada específica.

El CSV empieza con `phase,active_channel,setpoint_v,measured_channel,measured_v`. Usar punto decimal y las fases `hold`, `stop`, `close`. En `stop` y `close`, `setpoint_v` registra la consigna previa a la acción. El reporte calcula error firmado en mV respecto al canal activo o a cero en las otras ramas y después de stop/cierre. Filas faltantes, exactitud del instrumento y tolerancia de aceptación se revisan antes de decidir cualquier PASS.

Rango nominal de referencia para programas CV y cableado de breakout documentados por Akai. [Programas CV](https://support.akaipro.com/en/support/solutions/articles/69000804211-akai-pro-mpc-series-understanding-and-loading-programs) · [Manual MPC Standalone](https://cdn.inmusicbrands.com/akai/MPC3-NI/MPC%20Standalone%20OS%20-%20User%20Guide%20-%20v3.4.pdf)
