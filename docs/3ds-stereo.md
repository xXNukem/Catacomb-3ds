# Estereoscopía en Nintendo 3DS

Catacomb 3-D conserva su vista de 264 × 144 en 400 × 240. Abyss, Armageddon y Apocalypse conservan la focal de su motor y su vista de 320 × 120 en 400 × 180. El arma se dibuja en el plano de pantalla. El HUD y el automapa comparten la vista inferior.

## Geometría y referencia

El port de Blake Stone utiliza cámaras paralelas, un centro óptico fraccional, visibilidad independiente y una pareja de destinos superiores. En su código, cada ojo se desplaza hasta 0,015 unidades y la convergencia se fija a 0,3. Catacomb coloca el origen óptico una casilla detrás del jugador. Trasladar esa convergencia literalmente situaría el plano de pantalla detrás del jugador dentro de la cámara. [Renderizador de Blake Stone](https://github.com/xXNukem/bstone-3ds/blob/master/src/bstone/src/3d_draw.cpp) y [pantallas físicas](https://github.com/xXNukem/bstone-3ds/blob/master/src/bstone/src/bstone_3ds_display.cpp).

Para una coordenada lateral X, profundidad óptica Z, focal f en píxeles y desplazamiento firmado e, la posición horizontal es centro + f(X−e)/Z + fe/1,25. La disparidad derecha menos izquierda se anula a 1,25 casillas. La zona muerta termina en 0,025 y el recorrido restante escala linealmente. Catacomb 3-D utiliza f = 65536/378 y un máximo de 1280/65536 casillas por ojo. En las aventuras se ajusta e según la focal y el escalado horizontal al LCD.

| Profundidad desde el origen óptico | Disparidad anterior de Catacomb 3-D | Disparidad nueva |
|---|---:|---:|
| 1 casilla | −6,16 px | −2,05 px |
| 1,25 casillas | −3,69 px | 0 px |
| 2 casillas | 0 px | +3,08 px |
| 8 casillas | +4,62 px | +6,93 px |
| Infinito | +6,16 px | +8,21 px |

Son valores analíticos en el LCD superior al máximo del regulador, antes de discretizar texeles y píxeles. Cerca de una esquina se reduce simétricamente la separación según la distancia a las celdas sólidas próximas dentro del campo de visión. Cada columna calcula su intersección, cara, coordenada de textura y profundidad. Si el plano cercano atraviesa mampostería, se recupera la cara de entrada desde un origen libre. Desde el interior de un sólido se utiliza su cara de salida.

## Estado por fotograma

La visibilidad central decide la activación y el mapa explorado. Catacomb 3-D obtiene esa máscara con un trazado central sin rasterizar. Las aventuras mantienen el seguimiento original de contornos y sus efectos, incluida la recuperación de desbordamiento mediante Thrust y la animación de paredes de C5/C6. Los ojos proyectan copias transitorias de los actores y usan una profundidad de pared por columna para el recorte interior de los sprites. La página EGA, la publicación y CalcTics avanzan una vez. Con el regulador cerrado se rasteriza una sola vista.

La captura indexada izquierda respeta la página, el desplazamiento horizontal y el ancho de línea EGA. El presentador entrega izquierda y derecha a sus destinos físicos mediante la conversión RGB565 existente. Abrir una ventana modal o cambiar de nivel invalida el par. El fundido se ejecuta en mono. Solo una captura nueva permite reactivar el par.

## Alcance de las pruebas

Desde la raíz, ejecutar `bash tests/run_3ds_stereo_host.sh`. La prueba de fotograma enlaza el código de dibujo de cada motor con reloj y salidas sintéticas. Compara las columnas y las coordenadas de sprites, el estado del actor, la exploración, los tics y la publicación. En las aventuras se enlaza el seguidor de contornos original. La prueba cuenta las llamadas a la animación de paredes C5/C6. Otra prueba ejecuta el escalador de sprites original y verifica los 64 patrones posibles de oclusión para una franja que cruza dos bytes EGA. La prueba del presentador usa dos framebuffers superiores independientes y un framebuffer inferior simulado. Estas pruebas cubren geometría y estado en escenas controladas. La percepción de profundidad, la diafonía del panel y los tiempos en consola se evaluarán con los juegos originales en la ronda final.
