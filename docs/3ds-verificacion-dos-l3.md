# Verificación DOS del bloque L3

La comparación usa `e21e1d64`, punta local de `origin/master` anterior a L1 y L2. Las tres instalaciones encontradas en la caché de extracción local llegaron al primer fotograma de juego en los motores ReflectionHLE compilados para macOS. Se identificaron las revisiones por las cadenas de las imágenes DOS descomprimidas. Se inspeccionaron y probaron los recursos fuera del repositorio.

| Edición DOS local | EXE | Bytes del EXE | CRC32 del EXE | Bytes de imagen | EGAGRAPH (bytes / CRC32) | Motor utilizado |
|---|---|---:|---|---:|---|---|
| Abyss registrada 1.23 | CATABYSS.EXE | 82697 | C0790687 | 194592 | 335969 / 8C8D9023 | catabyss113 |
| Armageddon 1.01 | CATARM.EXE | 82465 | ED15EACB | 190528 | 355803 / 5C2EF367 | catarm102 |
| Apocalypse 1.00 | CATAPOC.EXE | 82653 | D6647678 | 191760 | 382076 / 3C0575EA | catapoc101 |

## Rechazo anterior

- **Abyss.** El commit publicado validaba las tablas estructuralmente y seleccionaba la rama 1.13 para CATABYSS.EXE. En L2 la misma copia supera los controles de MZ, asignación de memoria DOS, LZEXE, gráficos, audio y mapas. A continuación `cat3dsAbyssEdition` devuelve cero. EGADICT y EGAHEAD difieren de ambas referencias y la imagen tiene otro tamaño y disposición. El selector convierte ese resultado en `EXE TABLES: FAIL`. Se conserva la rama del motor que utilizaba la instalación previamente funcional, incluida al reconocer el mismo contenido bajo otro nombre DOS.
- **Armageddon.** El commit publicado exigía el manifiesto completo de 1.02. L2 admite cuatro archivos principales exactos o tres archivos de datos exactos y un EXE alternativo. Solo AUDIO y GAMEMAPS coinciden. EGAGRAPH y el EXE difieren. Incluso con los tres archivos exactos, esta imagen difiere del tamaño exigido de 190112 bytes y de ambas firmas EGA.
- **Apocalypse.** El commit publicado exigía los cuatro archivos principales de 1.01. L2 muestra el mismo mensaje que para Armageddon, `Cannot verify game archives or EXE tables.`, al faltar la carpeta, los cuatro requisitos principales o una coincidencia exacta o estructural admisible. La copia local tiene dos coincidencias de cuatro, gráficos distintos y una imagen de 191760 bytes frente a 191360. En ambos episodios la descompresión y la sonda de tablas ya tenían éxito antes de L3.

## Verificación y diagnóstico

Los perfiles medidos comparan tamaño de imagen, tamaño y CRC de los tres archivos y CRC de las cinco tablas. La sonda sigue obteniendo los offsets de la imagen y el backend vuelve a verificar las tablas antes de entrar en el motor. El nombre y el CRC del envoltorio de la tabla anterior sirven para identificar las muestras. MZ sin compresión utiliza el inicio del módulo tras los párrafos de cabecera. LZEXE utiliza los bytes de salida del descompresor. Las direcciones de las tablas son desplazamientos desde el inicio de ese módulo, sin sumar cabecera DOS ni PSP.

Se corrigió además el desplazamiento relativo de búsqueda de MTEMP en Abyss. Las dos referencias originales dan +0x760 desde EGAHEAD. Antes se probaba −0x8a0 y la búsqueda por contenido recuperaba la tabla en la muestra local. El acceso rápido requiere que también coincida GAMEMAPS. El error en ese desplazamiento era independiente del rechazo final de edición.

El selector escanea las rutas absolutas bajo `sdmc:/3ds/catacomb-3ds/games`. El adaptador trabaja tras entrar en la carpeta seleccionada y prepara el descriptor antes de inicializar ReflectionHLE. Un fallo en esa segunda comprobación devuelve al detalle del episodio y permite volver o reescanear. La pantalla distingue edición DOS detectada y referencia del motor. El informe existente `sdmc:/3ds/catacomb-3ds/install-report.txt` incluye ruta, archivos encontrados, tamaños, CRC y detalle de la sonda.

## Pruebas

- **DOS auténtico.** Un ejecutable nativo de prueba enlazó los motores del árbol de trabajo con el escáner y los adaptadores 3DS. Cada ejecución usó almacenamiento temporal y SDL con dispositivos simulados. Se observó el retorno de `InitGame`, `LoadLatchMem` y el primer `ThreeDRefresh` del bucle de juego en los tres episodios. La instrumentación terminaba el proceso tras ese fotograma.
- **Mixtas.** `tests/3ds_dos_releases_host.cpp` usa las tres instalaciones anteriores, un alias DOS y rutas SD temporales. Construye también un MZ de prueba con la imagen auténtica, altera MTEMP y trunca ambas formas de EXE. Se ejecutó con ASan y UBSan. Para repetirla, compilar ese archivo con C++11 y pasar cuatro rutas absolutas: un directorio temporal vacío y las carpetas de Abyss 1.23, Armageddon 1.01 y Apocalypse 1.00, en ese orden.
- **Sintéticas.** Pasaron `tests/run_3ds_menu_host.sh` y `tests/run_3ds_slots_host.sh`. Cubren identificación 1.13/1.24, referencias 1.02/1.01, alias, CRC incorrectos, límites MZ, cargador de tablas y navegación tras rechazo. El control de ELF verificó los cuatro motores y la restauración de los datos de sesión, incluido el puntero a los descriptores.
- **Compilación.** Se generaron `platform/3ds/cat3ds-original.3dsx` y `platform/3ds/3ds.3dsx` con devkitARM.

Queda pendiente ejecutar el 3DSX en New Nintendo 3DS con la SD del usuario, comparar las huellas con la tabla y comprobar el cambio de episodio en hardware. Las revisiones canónicas 1.13, 1.24, 1.02 y 1.01 se comprobaron aquí con definiciones originales y pruebas sintéticas. La prueba con archivos auténticos alcanza el primer fotograma de las tres revisiones locales. No cubre una partida completa ni la interoperabilidad de guardados DOS entre revisiones.

## Archivos del bloque

| Área | Archivos |
|---|---|
| Identificación y selección de recursos | `platform/3ds/source/cat3ds_resource_probe.{cpp,h}`, `cat3ds_game_files.{cpp,h}` y `cat3ds_abyss_compat.cpp` en el mismo directorio |
| Presentación del diagnóstico y reintento | `platform/3ds/original/cat3ds_sdl_launcher.cpp` y `refkeen_original_main.c` |
| Firmas compartidas con el lector original | `src/unlzexe/unlzexe.c` y `src/unlzexe/unlzexe_signature.h` |
| Pruebas | `tests/3ds_installation_host.cpp`, `tests/3ds_selector_host.cpp` y `tests/3ds_dos_releases_host.cpp` |
| Informe | `docs/3ds-verificacion-dos-l3.md` |
