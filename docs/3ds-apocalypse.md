# Catacomb Apocalypse 1.01 en Nintendo 3DS

La cuarta opción del selector unificado inicia `catapoc101:game`, la entrada original `apocgame_exe_main`. El selector y el registro de instalaciones del backend comprueban APOCGAME.EXE, AUDIO.APC, EGAGRAPH.APC y GAMEMAPS.APC en `sdmc:/3ds/catacomb-3ds/games/catapoc`. Los nombres, tamaños y CRC32 proceden del manifiesto 1.01. Los otros 35 archivos del manifiesto pertenecen a la introducción y al libro de pistas. El selector puede reconocer una distribución completa y también iniciar el juego con los cuatro archivos principales verificados.

El cargador de ReflectionHLE descomprime el ejecutable y comprueba los CRC32 de AUDIODCT.APC, AUDIOHHD.APC, EGADICT.APC, EGAHEAD.APC y MTEMP.TMP. Las ranuras se guardan en `sdmc:/3ds/catacomb-3ds/saves-apoc/catapoc101/`. El encabezado original contiene el nombre DOS de Apocalypse y su versión de guardado. El cuerpo conserva los punteros DOS de cielo, suelo y estados de actores propios de esta edición.

La vista de 320×120 se presenta a 400×180. El HUD de 320×80, con el radar y los indicadores del motor, comparte la pantalla inferior con el automapa. START abre opciones y SELECT pausa. ZR permite seleccionar ayuda, sonido, guardado y carga con la cruceta. Las ranuras usan A para confirmar y B para volver.

El regulador activa dos cámaras paralelas en el mundo. El motor conserva una sola actualización de visibilidad jugable, animación y tiempo por fotograma. La geometría, el recorte y las comprobaciones por ojo están documentados en [Estereoscopía en Nintendo 3DS](3ds-stereo.md).

Las comprobaciones automatizadas utilizan datos sintéticos y los códecs originales. La ronda en consola cubre el arranque con datos DOS, las transiciones entre niveles, los controles en combate, la legibilidad de la ayuda, el audio audible y las operaciones reales en tarjeta SD.
