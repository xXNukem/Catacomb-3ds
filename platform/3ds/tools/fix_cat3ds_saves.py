#!/usr/bin/env python3
"""Catacomb 3DS: safe native save buffers and automatic save-slot labels.

Patches only the original Catacomb 3-D and id91_11 save code. It does not
modify game data, the four-game launcher, renderer, or control configuration.
Runs preflight checks before writing, preserves existing files in a backup,
and is safe to invoke again.
"""
from pathlib import Path
import shutil
import sys

ROOT = Path(__file__).resolve().parents[3]
BACKUPS = ROOT / 'platform/3ds/.backup-before-native-saves'


def replace_one(src, before, after, name):
    if after in src:
        return src
    if src.count(before) != 1:
        raise ValueError(f'{name}: expected one matching code block, found {src.count(before)}')
    return src.replace(before, after, 1)


try:
    changes = {}

    # Keep the game's original save format and the six familiar slots, but no
    # 3DS on-screen keyboard. Game metadata is written as before.
    rel = 'src/id91_11/id_us_2.c'
    text = (ROOT / rel).read_text()
    text = replace_one(text,
        'REFKEEN_NS_B\n\n//      Special imports',
        'REFKEEN_NS_B\n\n#if defined(__3DS__) && defined(REFKEEN_VER_CAT3D)\n'
        'extern id0_int_t Cat3DS_GetSaveLevel(void);\n'
        '#endif\n\n//      Special imports', rel)
    text = replace_one(text,
        '''\tBottomS1 = "Type name";
\tBottomS2 = "Enter accepts";
\tUSL_DrawCtlPanel();''',
        '''#if defined(__3DS__) && defined(REFKEEN_VER_CAT3D)
\tBottomS1 = "Saving game";
\tBottomS2 = "Please wait";
#else
\tBottomS1 = "Type name";
\tBottomS2 = "Enter accepts";
#endif
\tUSL_DrawCtlPanel();''', rel)
    text = replace_one(text,
        '''\tok = US_LineInput(item->x + 2,item->y + 2,
\t\t\t\t\t\tgame->name,game->present? game->name : id0_nil_t,
\t\t\t\t\t\ttrue,MaxGameName,
\t\t\t\t\t\tCtlPanelW - 22);''',
        '''#if defined(__3DS__) && defined(REFKEEN_VER_CAT3D)
\t/* A save-slot selection confirms immediately on New 3DS. The descriptive
\t * label is kept in the original save header (the filename remains the
\t * compatible SAVEGAMn.C3D format). Never invoke the virtual keyboard. */
\t{
\t\ttime_t now = time(NULL);
\t\tstruct tm *when = now == (time_t)-1 ? NULL : localtime(&now);
\t\tif (when)
\t\t\tsnprintf(game->name, sizeof(game->name),
\t\t\t         "Level %02d %04d-%02d-%02d %02d:%02d",
\t\t\t         Cat3DS_GetSaveLevel(), when->tm_year + 1900,
\t\t\t         when->tm_mon + 1, when->tm_mday,
\t\t\t         when->tm_hour, when->tm_min);
\t\telse
\t\t\tsnprintf(game->name, sizeof(game->name),
\t\t\t         "Level %02d", Cat3DS_GetSaveLevel());
\t}
\tok = true;
#else
\tok = US_LineInput(item->x + 2,item->y + 2,
\t\t\t\t\t\tgame->name,game->present? game->name : id0_nil_t,
\t\t\t\t\t\ttrue,MaxGameName,
\t\t\t\t\t\tCtlPanelW - 22);
#endif''', rel)

    changes[rel] = text

    rel = 'src/id91_11/cat_all/cat3d/c3_main.c'
    text = (ROOT / rel).read_text()
    text = replace_one(text,
        'gametype\tgamestate;\nexittype\tplaystate;',
        '''gametype\tgamestate;
exittype\tplaystate;
#ifdef __3DS__
/* Shared with the original save menu; Catacomb levels are zero-based here. */
id0_int_t Cat3DS_GetSaveLevel(void)
{
\treturn gamestate.mapon + 1;
}
#endif''', rel)

    text = replace_one(text, '#define RLETAG\t0xABCD',
        '''#define RLETAG\t0xABCD
#ifdef __3DS__
/* Reject invalid/truncated save-map streams before invoking the historical
 * RLEW decoder (which assumes the input and output are already valid). */
static id0_boolean_t Cat3DS_ValidSavedMap(const id0_unsigned_t *src,
                                         size_t compressedBytes,
                                         size_t expandedBytes)
{
    size_t pos = 0, out = 0;
    const size_t count = compressedBytes / 2;
    const size_t expected = expandedBytes / 2;
    if ((compressedBytes & 1) || (expandedBytes & 1))
        return false;
    while (pos < count)
    {
        id0_unsigned_t value = src[pos++];
        size_t run = 1;
        if (value == RLETAG)
        {
            if (pos + 2 > count)
                return false;
            run = src[pos++];
            ++pos; /* value to repeat */
            if (!run)
                return false;
        }
        if (run > expected - out)
            return false;
        out += run;
    }
    return out == expected;
}
#endif''', rel)

    save_start = text.find('id0_boolean_t\tSaveTheGame(BE_FILE_T file)')
    load_start = text.find('id0_boolean_t\tLoadTheGame(BE_FILE_T file)')
    if save_start < 0 or load_start < 0 or load_start <= save_start:
        raise ValueError('c3_main.c: cannot locate original save/load routines')
    save_region = text[save_start:load_start]
    if 'Cat3DS_SaveBufferRelease' not in save_region:
        # The old buffer is only 'expanded' bytes and can be overrun by the
        # original RLEW encoder (worst-case: three words for every input word).
        save_region = replace_one(save_region,
            '''\tobjtype\t*o;
\tmemptr\tbigbuffer;''',
            '''\tobjtype\t*o;
#ifdef __3DS__
\tid0_byte_t *bigbuffer;
\tsize_t bufferCapacity;
#else
\tmemptr\tbigbuffer;
#endif''', 'c3_main.c SaveTheGame variables')
        save_region = replace_one(save_region,
            '''\texpanded = mapwidth * mapheight * 2;
\tMM_GetPtr (&bigbuffer,expanded);''',
            '''\texpanded = mapwidth * mapheight * 2;
#ifdef __3DS__
\t/* Maximum RLEW expansion is 3 words per input word, plus the header.
\t * Use native memory: an MM_GetPtr allocation can compact movable DOS
\t * blocks while game map pointers are actively being read. */
\tbufferCapacity = (size_t)expanded * 3 + 2;
\tbigbuffer = expanded ? (id0_byte_t *)malloc(bufferCapacity) : NULL;
\tif (!bigbuffer)
\t\treturn false;
#else
\tMM_GetPtr (&bigbuffer,expanded);
#endif''', 'c3_main.c SaveTheGame allocation')
        save_region = replace_one(save_region,
            '''\t\t*(id0_unsigned_t id0_huge *)bigbuffer = compressed;''',
            '''#ifdef __3DS__
\t\tif ((size_t)compressed + 2 > bufferCapacity)
\t\t{
\t\t\tfree(bigbuffer);
\t\t\treturn false;
\t\t}
#endif
\t\t*(id0_unsigned_t id0_huge *)bigbuffer = compressed;''', 'c3_main.c SaveTheGame compression bounds')
        save_region = save_region.replace('''\t\t\tMM_FreePtr (&bigbuffer);
\t\t\treturn(false);''', '''#ifdef __3DS__
\t\t\tfree(bigbuffer);
#else
\t\t\tMM_FreePtr (&bigbuffer);
#endif
\t\t\treturn(false);''')
        # Final cleanup. The resulting function has one trailing cleanup.
        save_region = replace_one(save_region,
            '''\tMM_FreePtr (&bigbuffer);

\treturn(true);''',
            '''#ifdef __3DS__
\tfree(bigbuffer);
#else
\tMM_FreePtr (&bigbuffer);
#endif
\treturn(true);''', 'c3_main.c SaveTheGame final cleanup')
        # Add a stable unique marker for idempotency.
        save_region = save_region.replace('/* Maximum RLEW expansion', '/* Cat3DS_SaveBufferRelease: Maximum RLEW expansion', 1)
        text = text[:save_start] + save_region + text[load_start:]

    # Loading must accept the larger compressed chunks that native save
    # buffers can now create, and must reject corrupt/truncated lengths.
    load_start = text.find('id0_boolean_t\tLoadTheGame(BE_FILE_T file)')
    reset_start = text.find('void ResetGame(void)', load_start)
    if reset_start < 0:
        raise ValueError('c3_main.c: cannot locate end of LoadTheGame')
    load_region = text[load_start:reset_start]
    if 'Cat3DS_LoadBufferCapacity' not in load_region:
        load_region = replace_one(load_region,
            '''\tmemptr\t\tbigbuffer;''',
            '''#ifdef __3DS__
\tid0_byte_t *bigbuffer;
\tsize_t Cat3DS_LoadBufferCapacity;
#else
\tmemptr\t\tbigbuffer;
#endif''', 'c3_main.c LoadTheGame variables')
        load_region = replace_one(load_region,
            '''\texpanded = mapwidth * mapheight * 2;
\tMM_GetPtr (&bigbuffer,expanded);''',
            '''\texpanded = mapwidth * mapheight * 2;
#ifdef __3DS__
\tCat3DS_LoadBufferCapacity = (size_t)expanded * 3 + 2;
\tbigbuffer = expanded ? (id0_byte_t *)malloc(Cat3DS_LoadBufferCapacity) : NULL;
\tif (!bigbuffer)
\t\treturn false;
#else
\tMM_GetPtr (&bigbuffer,expanded);
#endif''', 'c3_main.c LoadTheGame allocation')
        load_region = replace_one(load_region,
            '''\t\tif (BE_Cross_readInt16LEBuffer(file, bigbuffer, compressed) != compressed)''',
            '''#ifdef __3DS__
\t\tif (!compressed || (compressed & 1) ||
\t\t    (size_t)compressed > Cat3DS_LoadBufferCapacity)
\t\t{
\t\t\tfree(bigbuffer);
\t\t\treturn false;
\t\t}
#endif
\t\tif (BE_Cross_readInt16LEBuffer(file, bigbuffer, compressed) != compressed)''', 'c3_main.c LoadTheGame length validation')
        load_region = load_region.replace('''\t\t\tMM_FreePtr (&bigbuffer);
\t\t\treturn(false);''', '''#ifdef __3DS__
\t\t\tfree(bigbuffer);
#else
\t\t\tMM_FreePtr (&bigbuffer);
#endif
\t\t\treturn(false);''')
        load_region = replace_one(load_region,
            '''\t\tCA_RLEWexpand ((id0_unsigned_t id0_huge *)bigbuffer,
\t\t\t(id0_unsigned_t id0_huge *)mapsegs[i],expanded,RLETAG);''',
            '''#ifdef __3DS__
\t\tif (!Cat3DS_ValidSavedMap((const id0_unsigned_t *)bigbuffer,
\t\t                            compressed, expanded))
\t\t{
\t\t\tfree(bigbuffer);
\t\t\treturn false;
\t\t}
#endif
\t\tCA_RLEWexpand ((id0_unsigned_t id0_huge *)bigbuffer,
\t\t\t(id0_unsigned_t id0_huge *)mapsegs[i],expanded,RLETAG);''', 'c3_main.c LoadTheGame RLE validation')
        load_region = replace_one(load_region,
            '''\tMM_FreePtr (&bigbuffer);
//''',
            '''#ifdef __3DS__
\tfree(bigbuffer);
#else
\tMM_FreePtr (&bigbuffer);
#endif
//''', 'c3_main.c LoadTheGame final cleanup')
        text = text[:load_start] + load_region + text[reset_start:]
    changes[rel] = text

    rel = 'src/id91_11/id_ca.c'
    text = (ROOT / rel).read_text()
    text = replace_one(text,
        '''\twhile (*source == value && source<end)''',
        '''#ifdef __3DS__
\t/* Do not dereference one past the end of the source map plane. */
\twhile (source < end && *source == value)
#else
\twhile (*source == value && source<end)
#endif''', rel)
    changes[rel] = text

    # Preflight ends here: no file is written unless every source matches.
    pending = [(ROOT / p, p, data) for p, data in changes.items()
               if (ROOT / p).read_text() != data]
    if not pending:
        print('Catacomb 3DS saves: already patched.')
        sys.exit(0)
    for path, rel, content in pending:
        backup = BACKUPS / rel
        backup.parent.mkdir(parents=True, exist_ok=True)
        if not backup.exists():
            shutil.copy2(path, backup)
    for path, rel, content in pending:
        path.write_text(content)
        print('updated:', rel)
    print('3DS automatic save names and safe save/load buffers installed.')
    print('Backups:', BACKUPS)
except (ValueError, OSError) as exc:
    sys.exit('ERROR: ' + str(exc))
