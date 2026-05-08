#ifdef __ANDROID__

#include "android_widescreen.h"

#include <SDL.h>

#include "common/grafdata.h"
#include "harness/trace.h"
#include "pd/sys.h"

static int s_base_width = 0;

static int round_up_4(int v) {
    return (v + 3) & ~3;
}

int DRAndroid_GetBaseWidth(void) {
    return s_base_width;
}

void DRAndroid_WidenForDeviceAspect(int *width, int *height) {
    const int base_width = *width;
    const int base_height = *height;

    LOG_INFO("widescreen: requested base spec %dx%d", base_width, base_height);

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        LOG_PANIC("SDL_INIT_VIDEO error: %s", SDL_GetError());
    }

    SDL_DisplayMode mode;
    if (SDL_GetCurrentDisplayMode(0, &mode) != 0) {
        LOG_WARN("SDL_GetCurrentDisplayMode failed: %s", SDL_GetError());
        s_base_width = base_width;
        return;
    }

    /* The display reports portrait dims when the system rotation hasn't
     * settled yet; force landscape by treating the larger side as horizontal. */
    const int sw = mode.w > mode.h ? mode.w : mode.h;
    const int sh = mode.w > mode.h ? mode.h : mode.w;
    if (sw <= 0 || sh <= 0) {
        s_base_width = base_width;
        return;
    }

    const float aspect = (float)sw / (float)sh;
    int new_width = round_up_4((int)(base_height * aspect + 0.5f));

    /* Clamp: never narrower than the base spec, never more than 2× wider
     * (a hard ceiling so a phantom display mode can't blow out the pixmap). */
    if (new_width < base_width) {
        new_width = base_width;
    }
    if (new_width > base_width * 2) {
        new_width = base_width * 2;
    }

    LOG_INFO("widescreen: device %dx%d aspect %.3f -> render %dx%d",
             sw, sh, aspect, new_width, base_height);

    /* Patch the matching spec entry so the game's own pixmap allocations
     * (gScreen / gBack_screen / gTemp_screen) come out at the widened width. */
    for (int i = 0; i < 2; i++) {
        if (gGraf_specs[i].total_width == base_width &&
            gGraf_specs[i].total_height == base_height) {
            gGraf_specs[i].total_width = new_width;
            gGraf_specs[i].row_bytes = new_width;
            gGraf_specs[i].phys_width = new_width;
        }
    }

    /* Match the corresponding HUD layout entry so CalcGrafDataIndex passes
     * (it compares spec dims against gGraf_data dims). */
    for (int i = 0; i < 2; i++) {
        if (gGraf_data[i].width == base_width && gGraf_data[i].height == base_height) {
            gGraf_data[i].width = new_width;
            break;
        }
    }

    s_base_width = base_width;
    *width = new_width;
}

#endif /* __ANDROID__ */
