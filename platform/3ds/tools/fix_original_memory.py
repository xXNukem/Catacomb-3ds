#!/usr/bin/env python3
"""3DS-only emulated DOS memory adjustment + targeted OOM report."""
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[3]
backup_root = root / 'platform/3ds/.backup-before-3ds-memory-fix'


def replace_once(src, old, new, rel):
    if new in src:
        return src
    if src.count(old) != 1:
        raise RuntimeError(f'{rel}: unexpected source version; no files modified')
    return src.replace(old, new, 1)


try:
    changes = {}
    rel = 'src/be_cross_mem_internal.h'
    s = (root / rel).read_text()
    s = replace_once(s,
        '       EMULATED_CONVENTIONAL_SIZE = 619 /*640*/ * 1024,',
        '''#ifdef __3DS__
       /* Extra virtual DOS memory on 3DS (no change to native heap).
        * Conventional+EMS remains within the 1 MiB segment-address space. */
       EMULATED_CONVENTIONAL_SIZE = 768 * 1024,
#else
       EMULATED_CONVENTIONAL_SIZE = 619 /*640*/ * 1024,
#endif''', rel)
    changes[rel] = s

    rel = 'src/id91_11/id_mm.c'
    s = (root / rel).read_text()
    s = replace_once(s, '#include "id_heads.h"',
        '#include "id_heads.h"\n#ifdef __3DS__\n#include <stdio.h>\n#endif', rel)
    s = replace_once(s, '\tMM_GetPtr (&bufferseg,BUFFERSIZE);\n}',
        '''\tMM_GetPtr (&bufferseg,BUFFERSIZE);
#ifdef __3DS__
    {
        FILE *f = fopen("sdmc:/3ds/catacomb-3ds/memory-report.txt", "w");
        if (f)
        {
            fprintf(f, "Original Catacomb 3-D / ReflectionHLE 3DS memory report\\n");
            fprintf(f, "MM_Startup: near=%ld far=%ld EMS=%ld free=%ld\\n",
                    (long)mminfo.nearheap, (long)mminfo.farheap,
                    (long)mminfo.EMSmem, (long)MM_TotalFree());
            fclose(f);
        }
    }
#endif
}''', rel)
    s = replace_once(s,
        '\tif (bombonerror)\n\t\t// REFKEEN - Always use macro, but with a game-specific message',
        '''#ifdef __3DS__
    {
        FILE *f = fopen("sdmc:/3ds/catacomb-3ds/memory-report.txt", "a");
        if (f)
        {
            id0_unsigned_t maxGap = 0;
            mmblocktype *block = mmhead;
            while (block && block->next)
            {
                id0_unsigned_t end = block->start + block->length;
                id0_unsigned_t gap = block->next->start - end;
                if (gap > maxGap) maxGap = gap;
                block = block->next;
            }
            fprintf(f, "MM_GetPtr FAILED: requested=%lu free=%ld unused=%ld largest_gap=%lu\\n",
                    (unsigned long)size, (long)MM_TotalFree(),
                    (long)MM_UnusedMemory(), (unsigned long)maxGap * 16UL);
            fprintf(f, "MM totals: near=%ld far=%ld EMS=%ld XMS=%ld\\n",
                    (long)mminfo.nearheap, (long)mminfo.farheap,
                    (long)mminfo.EMSmem, (long)mminfo.XMSmem);
            fclose(f);
        }
    }
#endif
\tif (bombonerror)
\t\t// REFKEEN - Always use macro, but with a game-specific message''', rel)
    changes[rel] = s

    rel = 'src/id91_11/cat_all/cat3d/c3_main.c'
    s = (root / rel).read_text()
    s = replace_once(s, '#include "c3_def.h"',
        '#include "c3_def.h"\n#ifdef __3DS__\n#include <stdio.h>\n#endif', rel)
    s = replace_once(s,
        '\tif (mminfo.nearheap+mminfo.farheap+mminfo.EMSmem+mminfo.XMSmem\n\t\t>= MINMEMORY)\n\t\treturn;\n',
        '''\tif (mminfo.nearheap+mminfo.farheap+mminfo.EMSmem+mminfo.XMSmem
\t\t>= MINMEMORY)
\t\treturn;
#ifdef __3DS__
    {
        FILE *f = fopen("sdmc:/3ds/catacomb-3ds/memory-report.txt", "a");
        if (f)
        {
            fprintf(f, "CheckMemory FAILED: reported=%ld required=%ld\\n",
                    (long)(mminfo.nearheap+mminfo.farheap+mminfo.EMSmem+mminfo.XMSmem),
                    (long)MINMEMORY);
            fclose(f);
        }
    }
#endif
''', rel)
    changes[rel] = s
except (RuntimeError, FileNotFoundError) as exc:
    sys.exit(f'ERROR: {exc}')

for rel, contents in changes.items():
    dest = root / rel
    if dest.read_text() == contents:
        continue
    backup = backup_root / rel
    backup.parent.mkdir(parents=True, exist_ok=True)
    if not backup.exists():
        backup.write_bytes(dest.read_bytes())
    dest.write_text(contents)
    print('updated:', rel)
print('3DS original-engine virtual memory adjustment applied.')
print('If the error persists, retrieve sdmc:/3ds/catacomb-3ds/memory-report.txt')
