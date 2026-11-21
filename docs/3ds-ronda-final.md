# Ronda final de Catacomb en New Nintendo 3DS

Hoja de recogida para la consola. Todas las casillas están pendientes. Anotar modelo de consola, versión del ejecutable, edición DOS verificada por el selector, configuración de sonido y nivel/ranura que permiten repetir cada incidencia.

| Prueba y criterio observable | Catacomb 3-D | Abyss | Armageddon | Apocalypse |
|---|---|---|---|---|
| Arranque en frío desde la opción correspondiente con datos originales. Registrar la edición y la llegada al primer nivel. | □ | □ | □ | □ |
| Caminar, girar, combatir y usar un objeto. Verificar respuesta de controles, indicadores y exploración del automapa. | □ | □ | □ | □ |
| PC speaker y AdLib. En cada modo, repetir un disparo, una recogida y varios efectos seguidos. Registrar cortes, notas retenidas o silencios con nivel y acción. | □ | □ | □ | □ |
| Música durante el juego. En Catacomb 3-D, escuchar el tema del nivel con música habilitada. En las tres aventuras, comprobar los efectos durante el silencio musical previsto. | □ | □ | □ | □ |
| Abrir pausa, opciones y ayuda mientras hay acción. Volver al mismo punto. Registrar respuesta del sonido, legibilidad y recuperación de las dos vistas. | □ | □ | □ | □ |
| Guardar en una ranura libre, cancelar una sobrescritura y cargar. Verificar nivel, inventario, perfil y recuperación de audio y pantalla. | □ | □ | □ | □ |
| Pasar al siguiente nivel y observar el fundido, la primera imagen completa y el reinicio del mapa. Comprobar también muerte y retorno desde el menú. | □ | □ | □ | □ |
| Misma posición y orientación en mono y con regulador bajo, medio y alto. Observar una esquina, un enemigo parcialmente oculto y una pared lejana. Registrar bordes dobles, diafonía, profundidad y comodidad por separado. | □ | □ | □ | □ |
| Registrar FPS y tiempos en los cuatro ajustes anteriores, primero en reposo y luego durante el mismo tramo de combate. Anotar coincidencias entre un pico y un corte audible. | □ | □ | □ | □ |
| Suspensión y reanudación del sistema en reposo y durante un efecto. Registrar sonido al volver, avance del mundo y primera presentación visible. | □ | □ | □ | □ |
| Salida normal y relanzamiento desde Homebrew Launcher. Elegir después otro juego. Verificar silencio tras salir, HUD, perfil y primer efecto del nuevo proceso. | □ | □ | □ | □ |

## Lectura de las métricas

La franja inferior del juego muestra ventanas de aproximadamente un segundo. El cambio de modo, la apertura de un modal y el cambio de nivel descartan la ventana anterior. Los guiones indican que todavía falta una ventana completa. Para comparar, usar la misma ranura, orientación y escena, dejar estabilizar el regulador y anotar varias ventanas consecutivas.

| Campo | Lectura |
|---|---|
| 2D / 3D | Modo efectivamente enviado a la pantalla superior. Una esquina puede reducir la separación. |
| FPS | Frecuencia de envíos de fotogramas superiores a libctru, calculada entre intercambios consecutivos. La lectura mide la aplicación. El barrido físico del LCD requiere observación externa. |
| R | Milisegundos medios del renderizador del mundo, incluido el trabajo de visibilidad y, en estéreo, las vistas y la captura izquierda. Termina tras la mano y antes del fundido, publicación y cálculo de tics. |
| V | Milisegundos medios desde la preparación de paleta y framebuffers hasta terminar la limpieza de cachés y el envío de ambas pantallas. |
| P | Mayor intervalo en milisegundos entre dos envíos superiores en la ventana. |
| T / B | Milisegundos medios de conversión superior RGB565 y de composición y conversión inferior. Pueden solaparse. |
| W | Milisegundos medios que el hilo principal espera al auxiliar después de terminar la pantalla inferior. Una espera inferior a un milisegundo puede aparecer como cero. |
| C2 / C1 / BST / C0 | Núcleo observado en el auxiliar, o conversión en el principal. BST identifica la ruta principal con la petición de aceleración de New 3DS. |

Registrar por muestra **juego, nivel, escena, regulador, modo mostrado, FPS, R, V, P, T, B, W, núcleo y observación audible/visual**. Comparar R en estéreo con R en mono para estimar el coste adicional de producir el par. Comparar también T, V y FPS para incluir la conversión y la presentación. En combate, las diferencias de actividad pueden afectar a la comparación. Los tiempos son enteros en milisegundos. V ya incluye T, B y W con el solapamiento correspondiente.

## Qué se ha comprobado en desarrollo

El selector cierra sus recursos SDL de vídeo antes de inicializar ReflectionHLE para la edición elegida. En los cuatro motores, la simulación y los tics tienen un avance por fotograma. El trazado y la oclusión usan la cámara de cada ojo. El trabajador convierte la pantalla superior mientras el principal compone la inferior. El principal espera su finalización antes de intercambiar los buffers. El diagnóstico utiliza la franja reservada bajo el HUD y se oculta durante los modales.

Catacomb 3-D llama a StartMusic al entrar en juego y selecciona TOOHOT_MUS. En Abyss, Armageddon y Apocalypse el código de música de nivel está excluido y la llamada del bucle de juego está comentada en las fuentes originales. Esas fuentes permiten identificar la decisión de implementación. No establecen su motivo histórico.

En host se ejecutaron los cuatro gestores originales con temporizador y dispositivos de efectos simulados. La prueba del backend usa el mezclador, el generador PC speaker, el emulador OPL y un flujo SDL con el controlador dummy, además de la ruta sin dispositivo. Las pruebas estereoscópicas usan escenas y framebuffers sintéticos. La audición en altavoces, la continuidad durante el combate real, la planificación de hilos en 3DS y el rendimiento en consola quedan para esta matriz.

La prueba del backend se reproduce con `bash tests/run_3ds_audio_lifecycle_host.sh` en un host con SDL3 y su archivo sdl3.pc accesible mediante pkg-config. Los cuatro gestores se prueban con `bash tests/run_3ds_slots_host.sh` y la presentación con `bash tests/run_3ds_stereo_host.sh`.
