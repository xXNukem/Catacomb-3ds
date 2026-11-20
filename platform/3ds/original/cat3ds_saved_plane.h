// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <stdint.h>
#include <stdio.h>

// Nintendo 3DS is little-endian. Consume exactly the declared compressed
// bytes and fill exactly one plane, without using the live level as scratch.
static bool Cat3DS_ReadSavedPlane(FILE *file, uint16_t *out, size_t cells)
{
    uint16_t bytes;
    if (fread(&bytes, sizeof(bytes), 1, file) != 1 || (bytes & 1)) return false;
    size_t left = bytes / 2;
    while (left) {
        uint16_t value, run = 1;
        if (fread(&value, 2, 1, file) != 1) return false;
        --left;
        if (value == 0xabcd) {
            if (left < 2 || fread(&run, 2, 1, file) != 1 ||
                fread(&value, 2, 1, file) != 1 || !run) return false;
            left -= 2;
        }
        if (run > cells) return false;
        cells -= run;
        while (run--) *out++ = value;
    }
    return cells == 0;
}
