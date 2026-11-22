# Música de nivel en Catacomb 3DS

| Juego | Música de nivel en DOS | Efectos en DOS | Comportamiento del port |
|---|---|---|---|
| Catacomb 3-D | Sí, con música AdLib habilitada | Sí | Inicia TOOHOT_MUS al entrar al bucle de nivel |
| Catacomb Abyss | No | Sí | Reproduce TOOHOT_MUS con AdLib y los efectos |
| Catacomb Armageddon | No | Sí | Reproduce TOOHOT_MUS con AdLib y los efectos |
| Catacomb Apocalypse | No | Sí | Reproduce TOOHOT_MUS con AdLib y los efectos |

En el código publicado de Catacomb 3-D, PlayLoop llama a StartMusic. En las tres aventuras la llamada está comentada y las rutinas musicales de nivel quedan excluidas por el preprocesador. ReflectionHLE conserva esa decisión de los juegos originales. [Catacomb 3-D](https://github.com/CatacombGames/Catacomb3D/blob/master/C3_PLAY.C), [Abyss](https://github.com/CatacombGames/CatacombAbyss/blob/master/C4_PLAY.C), [Armageddon](https://github.com/CatacombGames/CatacombArmageddon/blob/master/C5_PLAY.C), [Apocalypse](https://github.com/CatacombGames/CatacombApocalypse/blob/master/C6_PLAY.C).

La prueba de instalaciones DOS leyó las tablas AUDIOHHD y AUDIODCT de los ejecutables y descomprimió el recurso musical de AUDIO.ABS, AUDIO.ARM y AUDIO.APC. En las copias locales de Abyss 1.23, Armageddon 1.01 y Apocalypse 1.00, los índices 102, 105 y 111 contienen la misma secuencia de 2742 bytes con CRC32 067df309, incluida su cabecera de longitud. Esto explica por qué las tablas enumeran una pista aunque DOS reproduzca únicamente efectos durante el nivel. En Nintendo 3DS se activa el secuenciador AdLib al entrar en cada nivel y se detiene al salir.
