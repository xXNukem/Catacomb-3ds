# Presentación de Catacomb Abyss en 3DS

El selector de `cat3ds-original.3dsx` incluye ambos motores. Abyss entrega al presentador el fotograma EGA compuesto por el backend, que ya resuelve las páginas de vídeo y la división entre mundo y HUD. El mundo ocupa las líneas 0–119 y el HUD las líneas 120–199. La vista superior mide 400×180 y conserva el aspecto DOS de 320×200 en una pantalla 4:3. En modo modal se muestra la composición completa a 320×240. El mapa de exploración se conserva al cerrar una ventana y se inicializa al entrar en un nivel o cargar una partida.

El regulador activa dos cámaras paralelas en el mundo. El motor conserva una sola actualización de visibilidad jugable, animación y tiempo por fotograma. La geometría, el recorte y las comprobaciones por ojo están documentados en [Estereoscopía en Nintendo 3DS](3ds-stereo.md).

La validación en consola queda para la ronda conjunta: proporciones y legibilidad en las dos LCD, salida y retorno de las seis ranuras, fundidos y ritmo de presentación con el procesador auxiliar disponible.
