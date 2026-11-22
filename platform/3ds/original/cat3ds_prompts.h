#ifndef CAT3DS_PROMPTS_H
#define CAT3DS_PROMPTS_H
#include <string.h>

// Match one complete displayed line, including a slice of a multi-line resource.
static inline const char *Cat3DS_ControlPrompt(const char *s, const char *optse)
{
    const size_t len = optse ? (size_t)(optse - s) : strlen(s);
    static const struct { const char *dos, *pad; } prompts[] = {
        {"Press SPACE to continue.", "Press A to continue."},
        {"Press SPACE to try again.", "Press A to try again."},
        {"Press ENTER or ESC to exit.", "Press A or B to close."},
        {"Press any key.", "Press A to continue."},
        {"Press SPACE to continue, ESC to abort.", "A to continue, B to cancel."},
    };
    for (const auto &prompt : prompts) {
        if (strlen(prompt.dos) == len && !memcmp(s, prompt.dos, len)) {
            return prompt.pad;
        }
    }
    return NULL;
}
#endif
