// Host substitute for the two newlib output-device slots used by the selector.
#pragma once
struct devoptab_t {};
enum {STD_OUT=1, STD_ERR=2};
extern const devoptab_t *devoptab_list[3];
