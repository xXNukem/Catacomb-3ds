#ifndef CAT3DS_MENU_H
#define CAT3DS_MENU_H

enum Cat3DSAction {
    CatResume, CatSave, CatLoad, CatControls, CatHelp, CatOptions,
    CatSwitch, CatExit, CatNew, CatOriginalOptions
};
enum Cat3DSPage { CatRoot, CatGuide, CatSettings, CatConfirm, CatPause, CatNoHelp, CatDiagnostic };
enum Cat3DSButton { CatUp, CatDown, CatLeft, CatRight, CatAccept, CatBack, CatStart, CatSelect };
struct Cat3DSMenu {
    Cat3DSPage page;
    int row, subrow, guide, game, sound, music;
    bool adlib, dead, title;
    int step(Cat3DSButton button) {
        if (page == CatDiagnostic) {
            if (button == CatBack || button == CatStart) page = CatSettings;
            return -1;
        }
        if (page == CatPause)
            return button == CatSelect || button == CatStart || button == CatBack || button == CatAccept ? CatResume : -1;
        if (button == CatStart && !dead) return CatResume;
        if (button == CatBack) {
            if (page == CatRoot) return dead ? -1 : CatResume;
            if (page == CatConfirm && subrow == CatNew) {
                page = CatSettings;
                subrow = game == 0 ? 3 : 1;
            } else page = CatRoot;
            return -1;
        }
        int dir = button == CatUp || button == CatLeft ? -1 : 1;
        if (page == CatConfirm) return button == CatAccept ? (subrow == CatNew ? CatNew : row) : -1;
        if (page == CatNoHelp) { if (button == CatAccept) page = CatRoot; return -1; }
        if (page == CatGuide) {
            if (button == CatAccept) page = CatRoot;
            else if (button != CatSelect) guide ^= 1;
            return -1;
        }
        if (page == CatSettings) {
            const int count = game == 0 ? 5 : 3;
            if (button == CatUp || button == CatDown) subrow = (subrow + count + dir) % count;
            else if (button == CatLeft || button == CatRight || button == CatAccept) {
                if (subrow == 0) sound = (sound + (adlib ? 3 : 2) + dir) % (adlib ? 3 : 2);
                else if (game == 0 && subrow == 1) music = adlib && !music;
                else if (button == CatAccept) {
                    if (subrow == count-1) { page = CatDiagnostic; return -1; }
                    if (game == 0 && subrow == 2) return CatOriginalOptions;
                    page = CatConfirm; subrow = CatNew;
                }
            }
            return -1;
        }
        if (button == CatUp || button == CatDown) {
            do { row = (row + 8 + dir) % 8; } while ((dead && row < CatLoad) || (title && row == CatSave));
        } else if (button == CatAccept) {
            subrow = guide = 0;
            if (title && row == CatResume) return CatNew;
            if (row == CatControls) page = CatGuide;
            else if (row == CatOptions) page = CatSettings;
            else if (row == CatHelp && game == 0) page = CatNoHelp;
            else if (row == CatSwitch || row == CatExit) page = CatConfirm;
            else return row;
        }
        return -1;
    }
};
int Cat3DS_OpenMenu(Cat3DSMenu &menu);
void Cat3DS_WaitForPadRelease();
extern int cat3dsSessionAction;
#endif
