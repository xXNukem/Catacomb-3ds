# Catacomb 3DS: comprobacion estructural EGA (investigacion 2026)

Los tres juegos originales proporcionados para comprobar compatibilidad no
coinciden exactamente con los CRC de las ediciones de ReflectionHLE. Aun asi,
se ha comprobado que los ejecutables DOS originales de Softdisk contienen
recursos graficos compatibles con el formato EGA utilizado por el motor.

| Juego | EXE de las pruebas | Imagen LZEXE en memoria | EGAHEAD | EGADICT | Resultado |
|---|---|---:|---:|---:|---|
| Abyss | CATABYSS.EXE | 194592 | 107472 | 154044 | Pic-table, STATUSPIC y varios chunks decodificados |
| Armageddon | CATARM.EXE | 190528 | 114176 | 149064 | Pic-table, STATUSPIC y varios chunks decodificados |
| Apocalypse | CATAPOC.EXE | 191760 | 114800 | 149796 | Pic-table, STATUSPIC y varios chunks decodificados |

Los desplazamientos anteriores pertenecen a **las ediciones examinadas**, no son
constantes en el nuevo cargador. Este las descubre dinamicamente en la imagen
descomprimida con controles estructurales. Se comprobaron archivos de datos
originales, pero **no** se incluyen aqui.

## Criterios de validacion

- EXE MS-DOS comprimido con una variante LZEXE reconocible, con descompresion
  completa y dentro de limites de memoria.
- Cabecera EGA de tres bytes por chunk, con la cantidad de chunks esperada
  por el motor de cada juego, offsets crecientes y ultimo offset igual al
  tamano de EGAGRAPH.
- Diccionario Huffman de 255 nodos valido, sin referencias fuera de rango.
- Descompresion correcta del pictable completo y de STATUSPIC, con dimensiones
  de 1 a 40 bytes EGA y de 1 a 200 lineas.
- Existencia de AUDIO y GAMEMAPS, sin declarar que el contenido sea ya
  utilizable por el motor de juego.

## Que **no** se ha comprobado

- Ejecucion completa del motor original con las versiones de Softdisk.
- Diferencias del codigo, state tables, saved games, scripts de introduccion,
  archivos SHP y demas datos fuera del graficador basico.
- Otras versiones no aportadas ni versiones con formatos no equivalentes.

No se deben presentar ediciones desconocidas como 'FULL EDITION VERIFIED'.
El nuevo indicador del menu significa **EGA ARCHIVE COMPATIBLE**.
El sistema de CRC32 sigue siendo util como diagnostico, pero deja de ser
un bloqueo para mostrar recursos de una edicion compatible.

Fuentes del formato: ReflectionHLE `src/unlzexe/unlzexe.c`,
`src/id91_11/id_ca.c` y `src/id91_11/id_vw_ae.c`.
