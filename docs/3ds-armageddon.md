# Catacomb Armageddon 1.02 en Nintendo 3DS

El selector comprueba los 40 nombres, tamaños y CRC32 de la edición de ReflectionHLE en `sdmc:/3ds/catacomb-3ds/games/catarm`. La tercera opción selecciona `catarm102:game`. El backend descomprime ARMGAME.EXE, valida los CRC32 de sus cinco tablas embebidas y entrega los datos al motor original. El perfil de escritura es `sdmc:/3ds/catacomb-3ds/saves-arm/catarm102`.

La pantalla superior muestra el mundo de 320×120 a 400×180. La inferior conserva las 80 líneas del HUD, incluido el radar de gemas, y muestra el automapa explorado. START abre las opciones y SELECT pausa. ZR da acceso a las teclas de función mediante la cruceta: arriba abre la ayuda, derecha el sonido, abajo el guardado e izquierda la carga. El selector de seis ranuras lee A y B físicos para confirmar y cancelar. El título de cada ranura muestra el número de nivel y la fecha de modificación del archivo.

El regulador activa dos cámaras paralelas en el mundo. El motor conserva una sola actualización de visibilidad jugable, animación y tiempo por fotograma. La geometría, el recorte y las comprobaciones por ojo están documentados en [Estereoscopía en Nintendo 3DS](3ds-stereo.md).

Los ensayos de host usan archivos, tablas y efectos de sonido sintéticos. La ronda final en consola cubre el arranque con los originales, los cambios de nivel, los controles durante combate, la legibilidad de la ayuda de 640×200, las operaciones de la tarjeta SD y la salida audible.
