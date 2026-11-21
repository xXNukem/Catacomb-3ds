"""Check engine/session ownership against the linked 3DS image."""
import subprocess
import sys

symbols = {}
for line in subprocess.check_output([sys.argv[1], "-nC", sys.argv[2]], text=True).splitlines():
    fields = line.split(maxsplit=2)
    if len(fields) == 3 and fields[0].isalnum():
        symbols[fields[2]] = (int(fields[0], 16), fields[1])

def region(section):
    start = symbols[f"__cat3ds_{section}_begin"][0]
    end = symbols[f"__cat3ds_{section}_end"][0]
    assert end > start
    return range(start, end)

data, bss = region("data"), region("bss")
assert data.stop <= bss.start
for game, entry in zip(("cat3d", "catabyss", "catarm", "catapoc"),
                       ("cat3d", "abysgame", "armgame", "apocgame")):
    assert f"{game}::{entry}_exe_main()" in symbols
    assert symbols[f"{game}::gamestate"][0] in bss
    for name, (address, kind) in symbols.items():
        if name.startswith(game + "::") and kind.lower() in ("d", "b"):
            assert address in (data if kind.lower() == "d" else bss), name
for name in ("g_refKeenCfg", "g_be_emulatedMemSpace", "g_be_selectedGameInstallation"):
    assert symbols[name][0] in bss
for name in ("cat3dsSessionAction", "workerThread", "initPerformance()::exitHookRegistered"):
    address = symbols[name][0]
    assert address not in data and address not in bss
print(f"PASS: four linked engines, session data {len(data)} B, BSS {len(bss)} B")
