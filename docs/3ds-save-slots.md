# Ranuras de los motores originales en 3DS

El selector físico de seis ranuras usa el directorio reescribible de la edición seleccionada. Los índices de archivo van de 0 a 5. Catacomb 3-D conserva SAVEGAMn.C3D y su cabecera DOS. Abyss conserva SAVEGAMn.SAV y permite prescindir de SAVEGAMn.INF. Un INF antiguo sin vínculo verificable con el SAV se sustituye visualmente por el número de nivel leído del SAV y la fecha del archivo. La inicialización no migra las ranuras.

El temporal .new solo se publica tras releer la partida, vaciar los búferes, sincronizar y cerrar el archivo. Si la publicación se interrumpe después del primer renombrado, el lector puede abrir .bak. La etiqueta de Abyss se publica después de la partida y lleva CRC del SAV y de los propios metadatos. Un fallo en esa segunda escritura se comunica por separado.

## Incorporación posterior de Armageddon y Apocalypse

Los motores comparten GE_SaveGame y GE_LoadGame en src/id91_11/cat_all/cat_late/gelib.c. Conservan sus propias cabeceras de juego y versión y registran los callbacks con US_SetLoadSaveHooks en catarm/c5_main.c y catapoc/c6_main.c. Antes de habilitar el selector para cada motor:

1. Enlazar su motor y comprobar que el directorio seleccionado corresponde a catarm102 o catapoc101.
2. Conectar los menús al selector común y añadir la cabecera DOS específica. Conservar los nombres y extensiones que el motor acepta.
3. Adaptar el lector en dos fases al orden de campos de cada edición, incluidos colores de suelo y cielo, punteros DOS y actores. Contrastar con catarm_savegame_compat.txt.
4. Probar el serializador de cada motor y partidas de referencia propias antes de habilitar sus ranuras. Los dos perfiles siguen reservados para esta integración.

Las pruebas de escritorio se ejecutan con bash tests/run_3ds_slots_host.sh. Usan directorios temporales y datos sintéticos.
