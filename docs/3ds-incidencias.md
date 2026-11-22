# Incidencias confirmadas en New Nintendo 3DS

| Nº | Incidencia observada y criterio de aceptación | Estado |
|---|---|---|
| 1 | Cambiar de juego desde el menú común bloquea la aplicación o provoca un crasheo. A veces reaparece el selector pero falla el arranque siguiente. Debe poderse confirmar el cambio, jugar otro nivel o reiniciar el mismo título y repetir el ciclo dentro del proceso. Cancelar debe devolver al punto de partida. | Abierta. Corregidas y probadas en host la espera con el Circle Pad mantenido y la restauración de la consola de texto. Cierre de audio comprobado con DSP simulado. Pendiente de repetir en consola. |
| 2 | El menú común tenía las cadenas en español codificadas directamente. Se han pasado a inglés las páginas, opciones y confirmaciones. El selector y las ranuras usan inglés. | Corregida en el código. Navegación A/B comprobada en host. |
| 3 | La detección y el arranque aplicaban reglas distintas sobre versiones, nombres y archivos auxiliares. Ahora se pasa al motor la instalación seleccionada y se comprueban archivos y tablas de juego. | La prueba posterior a L3 confirma el arranque de los cuatro juegos en New Nintendo 3DS. Las revisiones y huellas comprobadas se detallan en el informe L3. |
| 4 | El compositor reservaba 180 de las 240 filas superiores a la vista de las aventuras. Se ha recalculado la proyección y la región visible para llenar 400 × 240. | Geometría y composición comprobadas en host en los tres motores. Pendiente de inspección visual en consola. |
| 5 | El código original y algunos recursos describen el teclado DOS. Se adaptaron confirmaciones, pergaminos, avisos de continuación, reintentos y la ayuda compilada de los episodios posteriores. | Parcial. Las ayudas y gráficos de las distribuciones originales requieren inspección con sus datos. |
| 6 | La separación dependía de una focal de referencia y el cálculo anterior no incorporaba el recorte final de las aventuras. La proyección ahora parte de la disparidad en el LCD. | Abierta. El usuario sigue observando incomodidad y poca profundidad en New Nintendo 3DS. Banco independiente disponible desde L4. |
| 7 | Crash al morir. Se corrigieron dos riesgos durante la transición gráfica, la entrega de un framebuffer RGB565 a SDL y el recorrido de bloques EGA que dependía de un centinela. | Abierta. Daño letal y rutas de presentación probados con servicios simulados. Falta reproducir el fallo original e identificar su causa en hardware. |
| 8 | El origen óptico giraba detrás del jugador. Además, una recuperación del trazador DOS podía llamar a `Thrust` y cambiar el ángulo mientras dibujaba. El trazador 3DS parte del jugador y recorre el mapa sin esa recuperación. | Corregida en movimiento y renderizado probados en host. Pendiente de contraste visual y colisiones con mapas originales. |
| 9 | La entrada digitalizada giraba normalmente un grado por tic. Se han ajustado los cuatro motores a dos grados por tic o cuatro con giro rápido. | Integración por tics probada en host. Pendiente de valoración con los controles físicos. |
| 10 | Solo Catacomb 3-D reproduce música de nivel. | Comportamiento original explicado por título en L4. |
| 11 | Microcortes, distorsión e interrupciones de audio en consola. | Abierta. Corrección de continuidad y reducción de coste en el mezclador. Pendiente de escucha y contadores físicos. |
| 12 | Ventana de texto y demora al cambiar de juego. | Corregidas dos rutas de texto y el escaneo síncrono antes del retorno al selector. Pendiente de repetir el recorrido en consola. |
| 13 | Un mensaje ordinario recupera la composición DOS y desaparece el HUD inferior de las aventuras. | Corregida la clasificación de ventanas y probada la composición en host. Pendiente de inspección física. |

## Bloque L4

### Audio

Los cuatro gestores originales envían los efectos al altavoz PC o a AdLib. El secuenciador de Catacomb 3-D también escribe registros OPL. El callback de demanda de SDL genera el PCM, incluida la síntesis Nuked OPL a 49 716 Hz y el remuestreo al formato del mezclador. Con la configuración habitual de 48 kHz y 1024 muestras por fragmento, cada fragmento dura 21,3 ms. El controlador N3DS de SDL 3.4.18 dispone de tres fragmentos lineales, limpia la caché antes de encolar y despierta al hilo de reproducción cuando NDSP termina un buffer. La ruta de baja latencia opcional pide aproximadamente 700 servicios por segundo y reduce mucho ese margen. El tamaño efectivo se registra al cerrar la sesión.

El controlador N3DS eleva un escalón la prioridad heredada del hilo de reproducción, limitada al intervalo 0x19–0x2F. SDL intenta situarlo en el núcleo 1 de New 3DS con cuota del 30 %. El presentador usa normalmente el núcleo 2, con alternativa en el núcleo 1 y espera de finalización del trabajo gráfico antes de publicar las vistas. Esta competencia y el coste de OPL pueden influir en consola. Falta medir su contribución a los cortes percibidos. El temporizador PIT, las esperas de escritura AdLib, el número de buffers NDSP y el drenaje de cierre siguen sus rutas anteriores. [Controlador NDSP de SDL](https://github.com/libsdl-org/SDL/blob/release-3.4.18/src/audio/n3ds/SDL_n3dsaudio.c) · [Hilos N3DS de SDL](https://github.com/libsdl-org/SDL/blob/release-3.4.18/src/thread/n3ds/SDL_systhread.c)

El remuestreador retenía la última salida cuando ya había consumido la última entrada de una llamada. Una lectura posterior con cero entradas tampoco podía obtenerla. Ahora entrega todas las repeticiones pendientes hasta llenar la salida. En la mezcla final, dos fuentes a 48 kHz suponían 96 000 adquisiciones de mutex por segundo, además de sus liberaciones. Ahora se bloquea cada fuente por grupos de hasta 64 muestras. La regresión comprueba una rampa mono y estéreo con lecturas de 1, 63, 64, 65 y 257 muestras y el drenaje del remuestreador.

En consola, reproducir una misma situación con música y efectos, primero en mono y luego con el regulador a mitad y al máximo. Cambiar de juego para volcar la sesión en `sdmc:/3ds/catacomb-3ds/audio-l4.log`. Comparar `max_mix_us` con el periodo del fragmento, `frames/device_hz` segundos. `dsp_dropped` refleja el incremento del contador de frames perdidos del servicio DSP. `channel_stops` cuenta transiciones de reproducción a parada observadas al muestrear el canal 0 en callbacks sucesivos. Un hueco entre dos observaciones puede pasar inadvertido. La SD se escribe después de destruir el stream.

### Música y recursos DOS

| Título | Recurso | Comportamiento de nivel |
|---|---|---|
| Catacomb 3-D | Índice 90, `TOOHOT_MUS` | `PlayLoop` llama a `StartMusic`. Se carga la secuencia y se entrega a `SD_StartMusic` para AdLib. Confirmado de oído por el usuario. |
| Abyss | Índice 102 de AUDIO.ABS, 1805 bytes comprimidos en 1.23 | El arranque de música está comentado y las rutinas están excluidas. La fuente DOS publicada ya lo hacía. |
| Armageddon | Índice 105 de AUDIO.ARM, 1804 bytes comprimidos en 1.01 | El nivel inicia los controles y entra en simulación con la llamada a música comentada. |
| Apocalypse | Índice 111 de AUDIO.APC, 1807 bytes comprimidos en 1.00 | Misma decisión en su bucle original de juego. |

Se descomprimió por separado el último chunk de cada aventura con AUDIOHED y AUDIODICT de su EXE local. Los tres producen 2742 bytes con CRC32 `067DF309`. El ejecutable externo de introducción reconstruido en ReflectionHLE tiene llamadas al altavoz PC. No se encontró allí una orden para reproducir esta secuencia durante un nivel. Se conserva la selección musical original de cada título. [Código DOS de Catacomb 3-D](https://github.com/CatacombGames/Catacomb3D) · [Bucle DOS de Abyss](https://raw.githubusercontent.com/CatacombGames/CatacombAbyss/master/C4_PLAY.C) · [Código de Armageddon](https://github.com/CatacombGames/CatacombArmageddon) · [Motores e introducciones en ReflectionHLE](https://github.com/ReflectionHLE/ReflectionHLE/tree/master/src/id91_11/cat_all)

### Cambio de juego y presentación modal

El cierre original pasaba por `ShutdownId → VW_Shutdown → TEXTGR`, que prepara una pantalla de texto DOS con cursor. Al volver, el selector instalaba además una `PrintConsole` de libctru en el LCD inferior. Su inicialización intercambia buffers y espera un VBlank. Después se recorrían las cuatro instalaciones, con CRC, descompresión de EXE, identificación de tablas y escritura del informe antes del primer dibujo del selector. Esas dos superficies de texto están identificadas en código. Sin captura de la ventana observada no se puede atribuir su contenido a una de ellas con certeza. [Inicialización de consola de libctru](https://github.com/devkitPro/libctru/blob/master/libctru/source/console.c)

Al confirmar el cambio se omite la restauración de la pantalla DOS de salida. El selector compone su texto y sus errores directamente en un framebuffer inferior y usa la última lista de instalaciones de la sesión. Abrir el detalle vuelve a comprobar el episodio seleccionado. Y solicita una inspección completa. Se conserva la liberación del trabajador gráfico, el cierre del audio y de SDL y la restauración del estado de los motores antes del siguiente arranque.

Las ventanas centradas de juego se marcan como superposiciones mono. En las aventuras el área legible incluye las 320 columnas del mensaje y las 120 filas de vista. La pantalla inferior conserva los píxeles indexados y la paleta del último HUD y mapa. La exploración permanece hasta el cambio de nivel. La presentación distingue la transición explícita y el modo EGA de 640 columnas para respetar páginas especiales. Una captura izquierda anterior queda invalidada al entrar en un modal. La presentación vuelve a estéreo cuando recibe otro par del mundo. Comprobar preguntas anchas, pergaminos, ayuda, muerte y cambio de nivel en la consola.

### Uso del banco estéreo

Abrir START → Options → Technical: stereo test. El banco dibuja tres cuadrados a profundidades 2, 4 y 10 en unidades de la escena. El tamaño proyectado disminuye con la distancia. Los ojos usan cámaras paralelas con focal equivalente de 200 píxeles y la misma altura. La disparidad derecha menos izquierda crece con `1/Zpantalla − 1/Zobjeto`. El desplazamiento se multiplica por el regulador y se redondea al píxel del framebuffer, con un límite de dos píxeles por ojo. A igual separación física, una focal mayor aumenta la disparidad. El banco emplea proyección por software para ejercitar el mismo tipo de entrega a LCD que el port. El ejemplo de devkitPro y la proyección asimétrica de Citro3D sirven de referencia. [Ejemplo de devkitPro](https://github.com/devkitPro/3ds-examples/blob/master/graphics/gpu/fragment_light/source/main.c) · [Mtx_PerspStereoTilt](https://github.com/devkitPro/citro3d/blob/master/source/maths/mtx_perspstereotilt.c) · [Buenas prácticas para videojuegos](https://developer.download.nvidia.com/assets/gamedev/files/sdk/11/3DV%20-%20BestPracticesGuide.pdf)

1. Empezar con SELECT y regulador a cero. Debe indicar MONO. Subir el regulador manteniendo la posición de observación.
2. Tapar un ojo y luego el otro. En orientación normal corresponde L al izquierdo y R al derecho. A intercambia las vistas para comparar.
3. Con los valores iniciales, el cuadrado central y la marca móvil están en el plano del LCD. El naranja aparece delante y el lejano detrás. Verificar que la marca móvil coincide en altura y fase con ambos ojos. Cada par utiliza un mismo contador de fotograma y un único intercambio superior. [API de framebuffers libctru](https://github.com/devkitPro/libctru/blob/master/libctru/include/3ds/gfx.h)
4. Izquierda y derecha ajustan la separación entre 0 y 4. Arriba y abajo cambian la distancia de convergencia entre 3 y 5. SELECT restablece separación 2, convergencia 4 y orientación normal. B vuelve a Options. Los ajustes viven en la invocación del menú. Comparar después el banco y el mundo del juego con el mismo regulador.

### Validación y archivos de L4

Se generaron `platform/3ds/cat3ds-original.3dsx` y `platform/3ds/3ds.3dsx`. Se ejercitaron en host las suites de audio, menú, ranuras y estéreo, las revisiones DOS L3, las regiones de sesión del ELF, el escalador, la composición LCD, el trazador, las texturas y el recorrido experimental. Audio, menú y estéreo se verificaron con ASan/UBSan. El controlador NDSP real abre, encola PCM y cierra contra un DSP simulado. La reapertura verifica el silencio inicial en los tres buffers. Siguen pendientes la escucha, el tiempo físico de retorno, la inspección de los mensajes y la percepción de profundidad en New Nintendo 3DS con estos binarios.

| Área | Archivos de L4 |
|---|---|
| Sonido | `src/backend/audio/{be_audio_mixer.c,be_audio_resampling.c,be_audio_sdl.c}` |
| Selector y cierre | `platform/3ds/original/cat3ds_sdl_launcher.cpp`, `platform/3ds/source/cat3ds_game_files.{cpp,h}`, `src/id91_11/id_vw.c` |
| Presentación y menú | `platform/3ds/original/{cat3ds_dualscreen.c,cat3ds_menu.cpp,cat3ds_menu.h}` |
| Ventanas de los cuatro motores | `src/id91_11/cat_all/{cat3d/c3_play.c,catabyss/c4_play.c,catarm/c5_play.c,catapoc/c6_play.c}` |
| Regresiones | `tests/{3ds_audio_lifecycle_host.cpp,3ds_menu_host.cpp,3ds_selector_host.cpp,3ds_stereo_present_host.cpp,3ds_ndsp_close_host.cpp,run_3ds_audio_lifecycle_host.sh}` |
| Informe | `docs/3ds-incidencias.md` |

## Comprobación de la incidencia 1 en consola

Con ediciones que ya pasen la detección, entrar en un nivel, provocar un sonido y abrir el menú con START. Cancelar una vez y comprobar la posición, el audio y la entrada. Repetir y confirmar el aviso de pérdida de progreso. En cada paso comprobar el texto inferior del selector, entrar en el nivel del juego elegido, caminar, producir un efecto y comprobar las ranuras de su perfil.

- Catacomb 3-D → Abyss → Armageddon → Apocalypse
- Catacomb 3-D → Apocalypse → Catacomb 3-D
- Abyss → Abyss
- Armageddon → Armageddon
- Apocalypse → Catacomb 3-D

Repetir una confirmación manteniendo inclinado el Circle Pad al soltar A. Repetir las secuencias en el mismo proceso y comprobar la salida explícita al Homebrew Launcher al final. Registrar el último paso alcanzado si falla, especialmente si se llega al selector y el siguiente nivel no arranca.

## Muerte y transición de vídeo

Catacomb 3-D limita el daño a la salud restante, establece `ex_died`, dibuja `DEADPIC`, espera confirmación y vuelve a la pantalla normal para comprobar las puntuaciones. En las aventuras la muerte establece `FL_DEAD`. El bucle muestra el mensaje, reproduce el sonido y carga el rostro de salud cero. Abyss y Armageddon añaden un aviso si quedan pociones. El menú restringido permite cargar, empezar de nuevo o salir. Las implementaciones de `Died` desactivadas en las fuentes de las aventuras no intervienen.

El controlador de ventana de SDL3 escribe cuatro bytes por píxel en el modo RGBA8888. El compositor y el menú propios utilizan dos en RGB565. La entrega de pantalla comprueba ahora quién la posee antes de cada presentación de SDL y recupera su formato. La imagen de muerte de Catacomb 3-D invalida la captura estéreo anterior. En EGA, la búsqueda de un bloque sucio podía continuar fuera de la matriz si faltaba la marca de fin. La búsqueda y la agrupación terminan ahora en el último bloque válido. También se rechaza la primera fila fuera de rango antes de consultar la tabla de filas.

En host se ejercitaron el daño letal real de los cuatro motores, el dibujo de muerte y la entrada de puntuación de Catacomb 3-D, la selección del rostro en las tres aventuras, el menú de muerte y la transición de formato. El audio, la entrada y las imágenes de estas pruebas son sintéticos. Queda por comprobar la secuencia continua con enemigos y recursos DOS, con y sin pociones, al cargar una partida y al regresar al selector.

## Posición, giro y pantalla superior

La prueba de movimiento ejecuta la rutina original de cada motor y observa las llamadas a `Thrust`. Una vuelta con giro puro, también con disparo mantenido, conserva las coordenadas y recupera el ángulo inicial. El modificador de desplazamiento lateral sigue solicitando traslación. Otra prueba renderiza las 360 orientaciones en un mapa abierto y junto a una esquina, en mono y estéreo. La posición central es la del jugador y la separación de los ojos es perpendicular a su orientación. Se eliminaron del recorrido 3DS el desplazamiento longitudinal de la cámara y la recuperación física del trazador de contornos de las aventuras. El código de colisión durante la marcha conserva su comportamiento original.

El Circle Pad, el C-Stick y la cruceta llegan como direcciones a través de la configuración existente. Los motores integran 2° por tic en giro normal y 4° por tic con R. Con la base nominal de 70 Hz equivalen a 140°/s y 280°/s. Una vuelta normal requiere 180 tics y la rápida 90. Los límites originales de tics por actualización siguen aplicándose bajo carga. El paso de cada ojo comparte el mismo reloj de simulación.

En Abyss, Armageddon y Apocalypse se muestrean las 240 columnas centrales de la vista EGA de 320 × 120. La focal de paredes y sprites se reduce aproximadamente a tres cuartos para conservar el campo horizontal y ampliar el vertical. El factor de presentación es 400/240 en horizontal y 240/120 en vertical, coherente con el píxel DOS de relación 5:6. Las 120 filas proceden de la nueva proyección. El HUD y el mapa siguen abajo. La composición de pantallas de ayuda mantiene su diseño propio.

## Identificación de ediciones

| Juego | Referencias de ReflectionHLE | Criterio y límite |
|---|---|---|
| Catacomb 3-D | 1.00 y 1.22 | Cuatro archivos principales por tamaño y CRC. El selector pasa la revisión identificada y admite un nombre DOS alternativo del EXE. |
| Abyss | 1.13 shareware y 1.24 registrada | El adaptador comprueba la estructura de los recursos. El diccionario o la cabecera EGA, o la disposición conocida de las cinco tablas e imagen, identifican la familia de guardados. Una identificación ambigua se rechaza. |
| Armageddon | 1.02 | EXE original o alias verificado y los tres archivos principales. Para otro envoltorio se requieren los CRC de los tres archivos y de las cinco tablas localizadas, además del tamaño de imagen de la revisión. |
| Apocalypse | 1.01 | Se aplica el mismo criterio con las firmas propias de Apocalypse. El libro de pistas y el programa externo de introducción son independientes del motor de juego. |

La ruta estructural de las aventuras reconoce el formato LZEXE existente y una imagen MZ sin comprimir con cabecera y límites válidos. El cargador vuelve a comprobar los archivos y las tablas al abrirlos. Los alias idénticos usan el perfil de la edición. Los envoltorios estructurales de Armageddon y Apocalypse reciben un perfil con el CRC del ejecutable. El backend conserva el límite de nombre de archivo DOS. Un EXE de introducción no sustituye al del juego.

Los manifiestos locales contienen seis referencias. CatacombGL documenta por separado la distribución de GOG y Abyss shareware. Eso identifica distribuciones relevantes, pero la equivalencia de una reedición concreta, incluida una vendida con otro título, requiere sus archivos. Para recursos diferentes hacen falta los tamaños, CRC, tablas EGA/audio/mapas y evidencia de correspondencia de los estados de actores y guardados. No se ha arrancado una edición adicional con archivos comerciales en este bloque. [ReflectionHLE](https://github.com/ReflectionHLE/ReflectionHLE) · [Distribuciones documentadas por CatacombGL](https://github.com/ArnoAnsems/CatacombGL#how-to-install)

## Estereoscopía y calibración

El ejemplo de devkitPro lee el regulador, configura dos destinos físicos y dibuja una escena con desplazamientos de signo contrario. Citro3D incorpora una distancia al plano de proyección estereoscópico. En Portal3DS la actualización del jugador y del mundo precede al dibujo del fotograma, y el arma se dibuja aparte. Para el trazador por columnas se trasladan la cámara paralela, el centro de proyección asimétrico y la separación entre simulación y vistas. Las matrices de la GPU y sus unidades no se transfieren al motor EGA. [Ejemplo de devkitPro](https://github.com/devkitPro/3ds-examples/blob/master/graphics/gpu/fragment_light/source/main.c) · [Proyección de Citro3D](https://github.com/devkitPro/citro3d/blob/master/source/maths/mtx_perspstereotilt.c) · [Portal3DS](https://github.com/smealum/portal3DS/blob/master/source/main.c)

Para una profundidad perpendicular `z`, una focal en píxeles físicos `f`, una semiseparación en casillas `b` y un plano de pantalla `Z`, la disparidad horizontal derecha menos izquierda es `2fb(1/Z − 1/z)`. Los dos ojos comparten altura y dirección. El eje horizontal de cada proyección pasa por el mismo punto a `Z = 1,5` casillas. A regulador máximo se elige `b` para llegar a cuatro píxeles de disparidad al infinito. El regulador progresa linealmente tras una zona muerta de 2,5 %. La proximidad de paredes o sprites puede reducir la separación para limitar la disparidad cruzada calculada a 2,1 píxeles antes del redondeo. Son valores iniciales del port. La literatura relaciona la comodidad con la distancia de observación y el signo de la disparidad. [Shibata et al.](https://pubmed.ncbi.nlm.nih.gov/21778252/)

Cada ojo obtiene su trazado de paredes y su prueba de oclusión de sprites. Una vista central determina la visibilidad que consume el juego. El compositor entrega la captura izquierda a GFX_LEFT y la vista derecha a GFX_RIGHT y espera al trabajador de vídeo antes de intercambiar los buffers. Los mensajes y las transiciones descartan la captura anterior. Los límites se expresan en píxeles del LCD después de considerar cuántas columnas EGA aparecen en pantalla. [API gráfica de libctru](https://github.com/devkitPro/libctru/blob/master/libctru/include/3ds/gfx.h)

Para validar en New 3DS, comparar el regulador a cero, a mitad y al máximo frente a una esquina y un enemigo próximo. Observar también una pared del fondo, los bordes del arma y un sprite parcialmente oculto. Repetir un giro completo, abrir y cerrar el menú y morir. Registrar el episodio, la edición, la posición del regulador y los tiempos de renderizado y presentación que indica la pantalla inferior. Siguen pendientes la comodidad, la estabilidad perceptiva, la profundidad aparente y el coste real del segundo ojo.

## Archivos del bloque L2

| Área | Archivos modificados o añadidos |
|---|---|
| Pantallas, menú y proyección | `platform/3ds/original/{cat3ds_adventure_eye.inc,cat3ds_dualscreen.c,cat3ds_dualscreen.h,cat3ds_layout.h,cat3ds_menu.cpp,cat3ds_prompts.h,cat3ds_sdl_launcher.cpp,cat3ds_stereo.h,refkeen_original_main.c}` |
| Identificación DOS | `platform/3ds/source/{cat3ds_abyss_compat.cpp,cat3ds_game_files.cpp,cat3ds_game_files.h,cat3ds_resource_probe.cpp,cat3ds_resource_probe.h}`, `src/backend/filesystem/be_filesystem_gameinst.c` y `src/backend/startup/be_startup.c` |
| Presentación y texto compartidos | `src/backend/video/be_video_sdl.c`, `src/id91_11/{id_us_1.c,id_us_2.c,id_vw.c,id_vw_ae.c}` |
| Motores en `src/id91_11/cat_all/` | `cat3d/{c3_draw.c,c3_game.c,c3_wiz.c}`, `catabyss/{c4_draw.c,c4_wiz.c}`, `cat_late/{c5_draw.c,gelib.c}`, `catarm/{c5_main.c,c5_wiz.c}`, `catapoc/{c6_main.c,c6_wiz.c}` |
| Validación en `tests/` | `3ds_apocalypse_controls_host.cpp`, `3ds_arm_resources_host.cpp`, `3ds_death_host.cpp`, `3ds_installation_host.cpp`, `3ds_layout_host.c`, `3ds_menu_host.cpp`, `3ds_mz_host.cpp`, `3ds_selector_host.cpp`, `3ds_stereo_frame_host.cpp`, `3ds_stereo_host.c`, `3ds_stereo_present_host.cpp`, `3ds_turn_host.cpp`, `3ds_update_blocks_host.cpp` y `run_3ds_menu_host.sh` |

Se compilaron `platform/3ds/cat3ds-original.3dsx` y `platform/3ds/3ds.3dsx`. El primero enlaza los cuatro motores. Se ejecutaron en host las suites de menú, ciclo de audio, ranuras y estereoscopía, la comprobación del ELF, y las pruebas de composición LCD, escalador de paredes, trazador, texturas y recorrido del ejecutable experimental. Las suites de menú, audio y estéreo emplearon ASan/UBSan. Las pruebas de ranuras ejercitan serialización original. Los escenarios de juego, recursos, entradas y dispositivos fueron sintéticos. No se encontró una instalación DOS original completa en las ubicaciones locales examinadas.
