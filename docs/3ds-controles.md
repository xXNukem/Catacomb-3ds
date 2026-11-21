# Controles de Catacomb 3DS

La asignación usa las letras impresas en la consola. La guía está en **START → Controles**. La cruceta cambia de página y B vuelve al menú. **Ayuda original** abre las instrucciones de cada aventura. Catacomb 3-D carece de una pantalla de ayuda interna.

| Entrada durante la partida | Catacomb 3-D | Abyss | Armageddon | Apocalypse |
| --- | --- | --- | --- | --- |
| Circle Pad, cruceta o C-Stick | Avanzar, retroceder y girar | Igual | Igual | Igual |
| B o L con izquierda/derecha | Desplazamiento lateral | Igual | Igual | Igual |
| R | Correr y girar deprisa | Giro rápido | Giro rápido | Giro rápido |
| A | Disparar | Disparar | Disparar | Disparar |
| X | Beber poción | Beber poción | Beber poción | Beber poción |
| Y | Bolt | Zapper | Zapper | Zapper |
| ZL | Nuke | Xterminator | Xterminator | Xterminator |
| ZR | Selección de pergamino | Selección de pergamino | Atajos de función | Atajos de función |
| START | Abrir o cerrar el menú | Igual | Igual | Igual |
| SELECT | Pausar o reanudar | Igual | Igual | Igual |

Al pulsar ZR en 3-D o Abyss, arriba, derecha, abajo e izquierda seleccionan los pergaminos 1 a 4. Y, B, A y X seleccionan los pergaminos 5 a 8. En Armageddon y Apocalypse, ZR seguido de arriba abre la ayuda, derecha abre las opciones de sonido, abajo abre guardado e izquierda abre carga. ZR vuelve a los controles de partida. START y SELECT siguen disponibles en ambos grupos de atajos.

En los menús, A acepta y B cancela. Arriba y abajo seleccionan la fila. Izquierda y derecha cambian el valor en Opciones. Las seis ranuras se abren desde Guardar partida o Cargar partida. Aceptar una ranura ocupada para guardar abre una segunda confirmación. En el selector, A abre los detalles y una segunda pulsación inicia el juego compatible. Y repite el análisis de archivos.

**Cambiar de juego** pide confirmación antes de abandonar la partida. A devuelve al selector y descarta el progreso sin guardar. B conserva la sesión. **Salir de la aplicación** tiene su propia confirmación.

## Verificación del bloque K

Las pruebas de menú utilizan HID sintético, un mando SDL virtual y los mapas de control de cada motor. La prueba del selector ejecuta su bucle real con el controlador de vídeo dummy de SDL y estados de instalación sintéticos. La prueba del ELF verifica las regiones de memoria del enlace de Nintendo 3DS.

```sh
bash tests/run_3ds_menu_host.sh
python3 tests/3ds_session_elf.py "$DEVKITARM/bin/arm-none-eabi-nm" platform/3ds/build-original/cat3ds-original.elf
```

En la ronda de consola queda pendiente repetir 3-D → Apocalypse → 3-D con recursos originales, probar las cuatro ediciones y repetir una misma edición. Se comprobarán el primer efecto de sonido, el perfil de las seis ranuras, la respuesta al soltar los controles y la recuperación de ambos ojos al cerrar cada modal.
