#!/usr/bin/env python3
"""Catacomb 3DS: read graphics chunks into host-owned, stable buffers.

The original DOS cache uses shared, movable memory. A graphics chunk must not
keep a pointer into that memory while allocating its decompressed destination.
Only the __3DS__ build takes this path; the original other-platform code stays.
"""
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[3]
rel_ca = 'src/id91_11/id_ca.c'
rel_mm = 'src/id91_11/id_mm.c'
backup_root = root / 'platform/3ds/.backup-before-stable-grcache'


def replace_one(source, old, new, path):
    if new in source:
        return source
    if source.count(old) != 1:
        raise RuntimeError(f'{path}: the expected code was not found exactly once (nothing changed)')
    return source.replace(old, new, 1)


try:
    ca = (root / rel_ca).read_text()
    mm = (root / rel_mm).read_text()

    helper = r'''#ifdef __3DS__
/* Only used by this port. The native staging allocation never participates
 * in MM_SortMem, so decompressing cannot invalidate the compressed input.
 * The game still uses ReflectionHLE's original CAL_ExpandGrChunk function. */
int g_cat3ds_active_graphic_chunk = -1;

static void CAL_ReadGrChunkStable3DS(id0_int_t chunk)
{
    id0_int_t next = chunk + 1;
    id0_long_t start, end, length;
    id0_byte_t *source;

    if (chunk < 0 || chunk >= NUMCHUNKS)
        Quit("3DS graphic chunk index out of range");
    start = GRFILEPOS(chunk);
    if (start < 0)
        return; /* Sparse chunk */
    while ((next <= NUMCHUNKS) && (GRFILEPOS(next) == -1))
        ++next;
    if (next > NUMCHUNKS)
        Quit("3DS graphic chunk has no terminating offset");
    end = GRFILEPOS(next);
    length = end - start;
    /* The largest compressed Catacomb 3-D 1.22 chunk is under 19 KiB.
     * Keep a generous upper bound to reject malformed resource offsets. */
    if (length < 4 || length > 131072 || start < 0)
        Quit("3DS graphic chunk has invalid compressed length");

    source = (id0_byte_t *)malloc((size_t)length);
    if (!source)
        Quit("3DS native graphics staging allocation failed");
    if ((BE_Cross_seek(grhandle, start, SEEK_SET) != 0) ||
        !CA_FarRead(grhandle, source, length))
    {
        free(source);
        Quit("3DS graphic chunk read failed");
    }
    /* EGA tiles don't have an explicit length header. Other chunks do. */
    if (chunk < STARTTILE8 || chunk >= STARTEXTERNS)
    {
        uint32_t expanded = (uint32_t)source[0] |
            ((uint32_t)source[1] << 8) |
            ((uint32_t)source[2] << 16) |
            ((uint32_t)source[3] << 24);
        if (expanded == 0 || expanded > 65536)
        {
            FILE *report = fopen("sdmc:/3ds/catacomb-3ds/memory-report.txt", "a");
            if (report)
            {
                fprintf(report, "INVALID EGA RESOURCE: chunk=%d offset=%ld compressed=%ld expanded=%lu\n",
                    (int)chunk, (long)start, (long)length, (unsigned long)expanded);
                fclose(report);
            }
            free(source);
            Quit("3DS graphics chunk has invalid expanded length");
        }
    }
    g_cat3ds_active_graphic_chunk = chunk;
    CAL_ExpandGrChunk(chunk, source);
    g_cat3ds_active_graphic_chunk = -1;
    free(source);
}
#endif /* __3DS__ */
'''
    # Insert after CAL_ExpandGrChunk (which is needed by this helper) and
    # before CAL_ReadGrChunk, avoiding forward declarations.
    ca = replace_one(ca,
        '/*\n======================\n=\n= CAL_ReadGrChunk\n=',
        helper + '\n/*\n======================\n=\n= CAL_ReadGrChunk\n=', rel_ca)

    ca = replace_one(ca,
        'void CAL_ReadGrChunk (id0_int_t chunk)\n{\n',
        'void CAL_ReadGrChunk (id0_int_t chunk)\n{\n#ifdef __3DS__\n    CAL_ReadGrChunkStable3DS(chunk);\n    return;\n#endif\n', rel_ca)

    old = ('\tgrneeded[chunk] |= ca_levelbit;\t\t// make sure it doesn\'t get removed\n'
           '\tif (grsegs[chunk])\n'
           '\t{\n'
           '\t\tMM_SetPurge (&grsegs[chunk],0);\n'
           '\t\treturn;\t\t\t\t\t\t\t// allready in memory\n'
           '\t}\n')
    # Anchor to the precise entry of CA_CacheGrChunk, since similar code
    # appears elsewhere in id_ca.c for other games.
    begin = ca.find('void CA_CacheGrChunk (id0_int_t chunk)\n{')
    if begin < 0:
        raise RuntimeError(f'{rel_ca}: CA_CacheGrChunk definition missing')
    part = ca[begin:]
    insert = '\n#ifdef __3DS__\n    CAL_ReadGrChunkStable3DS(chunk);\n    return;\n#endif\n'
    if insert not in part[:2000]:
        if old not in part[:2000]:
            raise RuntimeError(f'{rel_ca}: unexpected CA_CacheGrChunk implementation')
        part = part.replace(old, old + insert, 1)
        ca = ca[:begin] + part

    marks = ca.find('void CA_CacheMarks (const id0_char_t *title)\n{')
    if marks < 0:
        raise RuntimeError(f'{rel_ca}: CA_CacheMarks definition missing')
    part = ca[marks:]
    old_marks = ('\tfor (i=0;i<NUMCHUNKS;i++)\n'
                 '\t\tif ( (grneeded[i]&ca_levelbit) && !grsegs[i])\n'
                 '\t\t{\n'
                 '//\n// update thermometer\n//\n')
    new_marks = ('\tfor (i=0;i<NUMCHUNKS;i++)\n'
                 '\t\tif ( (grneeded[i]&ca_levelbit) && !grsegs[i])\n'
                 '\t\t{\n'
                 '#ifdef __3DS__\n'
                 '            if (dialog && updatecachebox) updatecachebox();\n'
                 '            CAL_ReadGrChunkStable3DS(i);\n'
                 '            if (mmerror) return;\n'
                 '            continue;\n'
                 '#endif\n'
                 '//\n// update thermometer\n//\n')
    if new_marks not in part[:5000]:
        if old_marks not in part[:5000]:
            raise RuntimeError(f'{rel_ca}: unexpected CA_CacheMarks implementation')
        part = part.replace(old_marks, new_marks, 1)
        ca = ca[:marks] + part

    # Avoid DOS paragraph-count wraparound. This was previously able to turn
    # a nonsensical 1.33-GiB request into a much smaller 16-bit allocation.
    safety = r'''#ifdef __3DS__
    if (size > (1024UL * 1024UL))
    {
        FILE *f = fopen("sdmc:/3ds/catacomb-3ds/memory-report.txt", "a");
        if (f)
        {
            fprintf(f, "INVALID MM REQUEST: bytes=%lu caller=%p active EGA chunk=%d\n",
                (unsigned long)size, __builtin_return_address(0), g_cat3ds_active_graphic_chunk);
            fclose(f);
        }
        Quit("MM_GetPtr: invalid oversized allocation (3DS)");
    }
#endif
'''
    mm = replace_one(mm, 'void MM_GetPtr (memptr *baseptr,id0_unsigned_long_t size)\n{\n',
                     'void MM_GetPtr (memptr *baseptr,id0_unsigned_long_t size)\n{\n' + safety,
                     rel_mm)

    # Extra failure diagnostics: a useful caller PC for other allocation
    # failures and the active EGA chunk, if within CAL_ExpandGrChunk.
    report_anchor = '            fprintf(f, "MM totals: near=%ld far=%ld EMS=%ld XMS=%ld\\n",'
    report_extra = '''            fprintf(f, "MM caller=%p, active EGA chunk=%d\\n",
                    __builtin_return_address(0), g_cat3ds_active_graphic_chunk);
'''
    if report_extra not in mm:
        if report_anchor not in mm:
            raise RuntimeError(f'{rel_mm}: previous memory-report instrumentation was not found')
        mm = mm.replace(report_anchor, report_extra + report_anchor, 1)
        # Add the declaration after the includes (not inside another namespace).
        mm = replace_one(mm, '#include "id_heads.h"',
                            '#include "id_heads.h"\n#ifdef __3DS__\nextern int g_cat3ds_active_graphic_chunk;\n#endif', rel_mm)

    changes = {rel_ca: ca, rel_mm: mm}
except (OSError, RuntimeError) as error:
    sys.exit(f'ERROR: {error}')

for rel, text in changes.items():
    destination = root / rel
    previous = destination.read_text()
    if previous == text:
        print('unchanged:', rel)
        continue
    backup = backup_root / rel
    backup.parent.mkdir(parents=True, exist_ok=True)
    if not backup.exists():
        backup.write_bytes(destination.read_bytes())
    destination.write_text(text)
    print('updated:', rel)
print('3DS graphics streaming: compressed chunks now use stable native staging memory.')
print('Menu, renderer and game data have not been modified.')
