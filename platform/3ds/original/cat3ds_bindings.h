#ifndef CAT3DS_BINDINGS_H
#define CAT3DS_BINDINGS_H
#include "be_st.h"
#include "be_st_cfg.h"

static inline void Cat3DS_ConfigureBindings(RefKeenConfig &cfg)
{
    // Desktop shortcuts from a saved configuration can otherwise overwrite
    // the physical buttons later in RefKeen_PrepareAltControllerScheme.
    for (auto &bind : cfg.cat3d.binds) bind.pad = -1;
    cfg.cat3d.useLeftStick = true;
    cfg.cat3d.useRightStick = true;
    cfg.cat3d.analogMotion = false;
    cfg.cat3d.binds[BE_ST_CTRL_BIND_CAT3D_FIRE].pad = BE_ST_CTRL_BUT_A;
    cfg.cat3d.binds[BE_ST_CTRL_BIND_CAT3D_STRAFE].pad = BE_ST_CTRL_BUT_B;
    cfg.cat3d.binds[BE_ST_CTRL_BIND_CAT3D_DRINK].pad = BE_ST_CTRL_BUT_X;
    cfg.cat3d.binds[BE_ST_CTRL_BIND_CAT3D_BOLT].pad = BE_ST_CTRL_BUT_Y;
    cfg.cat3d.binds[BE_ST_CTRL_BIND_CAT3D_NUKE].pad = BE_ST_CTRL_EXTRA_BUT_LTRIGGER;
    cfg.cat3d.binds[BE_ST_CTRL_BIND_CAT3D_FASTTURN].pad = BE_ST_CTRL_BUT_RSHOULDER;
    cfg.cat3d.binds[BE_ST_CTRL_BIND_CAT3D_SCROLLS].pad = BE_ST_CTRL_EXTRA_BUT_RTRIGGER;
}
#endif
