#include <glad/glad.h>

// this needs to be included after glad.h
#include <SDL.h>
#include <SDL_opengl.h>

#include "../renderers/gl/gl_renderer.h"
#include "harness/config.h"
#include "harness/hooks.h"
#include "harness/trace.h"
#include "sdl2_scancode_to_dinput.h"

#include "globvars.h"
#include "grafdata.h"
#include "pd/sys.h"

SDL_Window* window;
SDL_GLContext context;
uint8_t directinput_key_state[SDL_NUM_SCANCODES];
int render_width, render_height;
int window_width, window_height;
int vp_x, vp_y, vp_width, vp_height;

struct {
    float x;
    float y;
} sdl_window_scale;

static void update_viewport(void) {
    const float target_aspect_ratio = (float)render_width / render_height;
    const float aspect_ratio = (float)window_width / window_height;

    vp_width = window_width;
    vp_height = window_height;
    if (aspect_ratio != target_aspect_ratio) {
        if (aspect_ratio > target_aspect_ratio) {
            vp_width = window_height * target_aspect_ratio + .5f;
        } else {
            vp_height = window_width / target_aspect_ratio + .5f;
        }
    }
    vp_x = (window_width - vp_width) / 2;
    vp_y = (window_height - vp_height) / 2;
    GLRenderer_SetViewport(vp_x, vp_y, vp_width, vp_height);
}

static void create_glcore_context(char* title) {
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);

    window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, window_width, window_height, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    context = SDL_GL_CreateContext(window);
}

static void create_gles_context(char* title) {
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

    window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, window_width, window_height, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    context = SDL_GL_CreateContext(window);
}

#ifdef __ANDROID__
/* gGraf_specs lives in src/DETHRACE/pc-win95/win95sys.c. We need to widen
 * gGraf_specs[].total_width before the game allocates gScreen / gBack_screen. */
#include "common/grafdata.h"
#include "pd/sys.h"

/* Round up to nearest multiple of 4 — keeps texture/row alignment happy. */
static int dr_round_up_4(int v) { return (v + 3) & ~3; }

static void dr_widen_for_device_aspect(int *out_width, int *out_height) {
    LOG_INFO("dr_widen entry: requested=%dx%d", *out_width, *out_height);
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        LOG_PANIC("SDL_INIT_VIDEO error: %s", SDL_GetError());
    }
    SDL_DisplayMode mode;
    if (SDL_GetCurrentDisplayMode(0, &mode) != 0) {
        LOG_WARN("SDL_GetCurrentDisplayMode failed: %s", SDL_GetError());
        return;
    }
    LOG_INFO("dr_widen mode: %dx%d", mode.w, mode.h);
    /* Display reports portrait dims when the system rotation hasn't kicked in
     * yet; treat the larger side as horizontal to compute landscape aspect. */
    int sw = mode.w > mode.h ? mode.w : mode.h;
    int sh = mode.w > mode.h ? mode.h : mode.w;
    if (sw <= 0 || sh <= 0) return;

    int base_height = *out_height;          /* 200 from the game */
    float aspect = (float)sw / (float)sh;
    int new_width = dr_round_up_4((int)(base_height * aspect + 0.5f));

    /* Sanity: don't go narrower than the original 320, don't go absurdly wide. */
    if (new_width < *out_width)  new_width = *out_width;
    if (new_width > *out_width * 2) new_width = *out_width * 2;

    LOG_INFO("widescreen: device %dx%d aspect %.3f -> render %dx%d",
             sw, sh, aspect, new_width, base_height);

    /* Patch the BRender graphics spec so the game's own pixmap allocations
     * (gScreen, gBack_screen, gTemp_screen) come out at the new width. */
    for (int i = 0; i < 2; i++) {
        if (gGraf_specs[i].total_width == *out_width &&
            gGraf_specs[i].total_height == base_height) {
            gGraf_specs[i].total_width = new_width;
            gGraf_specs[i].row_bytes   = new_width;
            gGraf_specs[i].phys_width  = new_width;
        }
    }

    /* Match the corresponding HUD layout entry so CalcGrafDataIndex passes. */
    for (int i = 0; i < 2; i++) {
        if (gGraf_data[i].width == *out_width && gGraf_data[i].height == base_height) {
            gGraf_data[i].width = new_width;
            break;
        }
    }

    *out_width = new_width;
}
#endif

static void* create_window_and_renderer(char* title, int x, int y, int width, int height) {
#ifdef __ANDROID__
    dr_widen_for_device_aspect(&width, &height);
#endif

    window_width = width;
    window_height = height;
    render_width = width;
    render_height = height;
    tOpenGL_profile opengl_profile;

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        LOG_PANIC("SDL_INIT_VIDEO error: %s", SDL_GetError());
    }

#ifdef __ANDROID__
    {
        extern int dr_app_event_watch(void* userdata, SDL_Event* event);
        SDL_AddEventWatch(dr_app_event_watch, NULL);
    }
#endif

#ifdef __ANDROID__
    // Android only has GLES; skip desktop core profile attempt
    create_gles_context(title);
    opengl_profile = eOpenGL_profile_es;
#else
    // prefer OpenGL core profile
    create_glcore_context(title);
    opengl_profile = eOpenGL_profile_core;
    if (window == NULL || context == NULL) {
        if (window != NULL) {
            SDL_DestroyWindow(window);
        }

        LOG_WARN("Failed to create OpenGL core profile: %s. Trying OpenGLES...", SDL_GetError());
        // fallback to OpenGL ES 3
        create_gles_context(title);
        opengl_profile = eOpenGL_profile_es;
    }
#endif

    if (window == NULL || context == NULL) {
        LOG_PANIC("Failed to create OpenGL context: %s", SDL_GetError());
    }

    sdl_window_scale.x = ((float)render_width) / width;
    sdl_window_scale.y = ((float)render_height) / height;

    if (harness_game_config.start_full_screen) {
        SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
    }

    // Load GL extensions using glad
#ifdef __ANDROID__
    if (!gladLoadGLES2Loader((GLADloadproc)SDL_GL_GetProcAddress)) {
        LOG_PANIC("Failed to initialize the OpenGL ES context with GLAD.");
        exit(1);
    }
#else
    if (opengl_profile == eOpenGL_profile_es) {
        if (!gladLoadGLES2Loader((GLADloadproc)SDL_GL_GetProcAddress)) {
            LOG_PANIC("Failed to initialize the OpenGL ES context with GLAD.");
            exit(1);
        }
    } else if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
        LOG_PANIC("Failed to initialize the OpenGL context with GLAD.");
        exit(1);
    }
#endif

#ifdef __ANDROID__
    /* On modern phones the display is typically 120Hz; cap the game to ~60fps to keep physics sane. */
    if (SDL_GL_SetSwapInterval(2) != 0) {
        SDL_GL_SetSwapInterval(1);
    }
#else
    SDL_GL_SetSwapInterval(1);
#endif

    GLRenderer_Init(opengl_profile, render_width, render_height);
    update_viewport();

    return window;
}

static int set_window_pos(void* hWnd, int x, int y, int nWidth, int nHeight) {
    // SDL_SetWindowPosition(hWnd, x, y);
    if (nWidth == 320 && nHeight == 200) {
        nWidth = 640;
        nHeight = 400;
    }
    SDL_SetWindowSize(hWnd, nWidth, nHeight);
    return 0;
}

static void destroy_window(void* hWnd) {
    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    window = NULL;
}

// Checks whether the `flag_check` is the only modifier applied.
// e.g. is_only_modifier(event.key.keysym.mod, KMOD_ALT) returns true when only the ALT key was pressed
static int is_only_key_modifier(int modifier_flags, int flag_check) {
    return (modifier_flags & flag_check) && (modifier_flags & (KMOD_CTRL | KMOD_SHIFT | KMOD_ALT | KMOD_GUI)) == (modifier_flags & flag_check);
}

#ifdef __ANDROID__
extern void Audio_SetPaused(int paused);

int dr_app_event_watch(void* userdata, SDL_Event* event) {
    (void)userdata;
    switch (event->type) {
    case SDL_APP_WILLENTERBACKGROUND:
    case SDL_APP_DIDENTERBACKGROUND:
        Audio_SetPaused(1);
        break;
    case SDL_APP_WILLENTERFOREGROUND:
    case SDL_APP_DIDENTERFOREGROUND:
        Audio_SetPaused(0);
        break;
    }
    return 1;
}

#define DR_MAX_FINGERS 8
typedef struct {
    SDL_FingerID id;
    int dinput_key;
    int active;
} dr_touch_finger_t;
static dr_touch_finger_t dr_fingers[DR_MAX_FINGERS];

static int dr_touch_zone_scancode(float nx, float ny) {
    /* Coordinates normalized 0..1 across the device window, landscape.
     * Zones are intentionally wider than the on-screen icons so a thumb
     * resting on the corner still registers, but they do NOT overlap. */

    /* Top-right pause icon */
    if (nx > 0.88f && ny < 0.18f) return SDL_SCANCODE_ESCAPE;

    /* Everything else only fires in the lower half of the screen so menu
     * mouse-clicks above are unaffected. */
    if (ny < 0.40f) {
        return 0;
    }

    /* Steering pad — left half of the screen */
    if (nx < 0.13f) return SDL_SCANCODE_KP_4;       /* ◀ left disc  */
    if (nx < 0.27f) return SDL_SCANCODE_KP_6;       /* ▶ right disc */

    /* Pedals — right half of the screen, split horizontally to match the
     * brake-and-gas pair drawn side-by-side. */
    if (nx > 0.87f)  return SDL_SCANCODE_KP_8;      /* gas (rightmost)        */
    if (nx > 0.73f)  return SDL_SCANCODE_KP_2;      /* brake (left of the gas) */

    return 0;
}

static void dr_touch_set_dik(int dinput_key, int down) {
    if (dinput_key) {
        directinput_key_state[dinput_key] = down ? 0x80 : 0;
    }
}

static void dr_touch_finger_down(SDL_FingerID id, float nx, float ny) {
    int sc = dr_touch_zone_scancode(nx, ny);
    if (!sc) return;
    int dik = sdlScanCodeToDirectInputKeyNum[sc];
    for (int i = 0; i < DR_MAX_FINGERS; i++) {
        if (!dr_fingers[i].active) {
            dr_fingers[i].id = id;
            dr_fingers[i].dinput_key = dik;
            dr_fingers[i].active = 1;
            dr_touch_set_dik(dik, 1);
            return;
        }
    }
}

static void dr_touch_finger_up(SDL_FingerID id) {
    for (int i = 0; i < DR_MAX_FINGERS; i++) {
        if (dr_fingers[i].active && dr_fingers[i].id == id) {
            dr_touch_set_dik(dr_fingers[i].dinput_key, 0);
            dr_fingers[i].active = 0;
            return;
        }
    }
}

static void dr_touch_finger_motion(SDL_FingerID id, float nx, float ny) {
    int new_sc = dr_touch_zone_scancode(nx, ny);
    int new_dik = new_sc ? sdlScanCodeToDirectInputKeyNum[new_sc] : 0;
    for (int i = 0; i < DR_MAX_FINGERS; i++) {
        if (dr_fingers[i].active && dr_fingers[i].id == id) {
            if (dr_fingers[i].dinput_key != new_dik) {
                dr_touch_set_dik(dr_fingers[i].dinput_key, 0);
                dr_fingers[i].dinput_key = new_dik;
                dr_touch_set_dik(new_dik, 1);
            }
            return;
        }
    }
}
#endif /* __ANDROID__ */

static int get_and_handle_message(MSG_* msg) {
    SDL_Event event;
    int dinput_key;

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_KEYDOWN:
        case SDL_KEYUP:
            if (event.key.keysym.sym == SDLK_RETURN) {
                if (event.key.type == SDL_KEYDOWN) {
                    if ((event.key.keysym.mod & (KMOD_CTRL | KMOD_SHIFT | KMOD_ALT | KMOD_GUI))) {
                        // Ignore keydown of RETURN when used together with some modifier
                        return 0;
                    }
                } else if (event.key.type == SDL_KEYUP) {
                    if (is_only_key_modifier(event.key.keysym.mod, KMOD_ALT)) {
                        SDL_SetWindowFullscreen(window, (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN_DESKTOP) ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
                    }
                }
            }

            // Map incoming SDL scancode to DirectInput DIK_* key code.
            // https://github.com/DanielGibson/Snippets/blob/master/sdl2_scancode_to_dinput.h
            dinput_key = sdlScanCodeToDirectInputKeyNum[event.key.keysym.scancode];
            if (dinput_key == 0) {
                LOG_WARN("unexpected scan code %s (%d)", SDL_GetScancodeName(event.key.keysym.scancode), event.key.keysym.scancode);
                return 0;
            }
            // DInput expects high bit to be set if key is down
            // https://learn.microsoft.com/en-us/previous-versions/windows/desktop/ee418261(v=vs.85)
            directinput_key_state[dinput_key] = (event.type == SDL_KEYDOWN ? 0x80 : 0);
            break;

        case SDL_WINDOWEVENT:
            switch (event.window.event) {
            case SDL_WINDOWEVENT_SIZE_CHANGED:
                SDL_GetWindowSize(window, &window_width, &window_height);
                update_viewport();
                sdl_window_scale.x = (float)render_width / vp_width;
                sdl_window_scale.y = (float)render_height / vp_height;
                break;
            }
            break;

#ifdef __ANDROID__
        case SDL_FINGERDOWN:
            dr_touch_finger_down(event.tfinger.fingerId, event.tfinger.x, event.tfinger.y);
            break;
        case SDL_FINGERUP:
            dr_touch_finger_up(event.tfinger.fingerId);
            break;
        case SDL_FINGERMOTION:
            dr_touch_finger_motion(event.tfinger.fingerId, event.tfinger.x, event.tfinger.y);
            break;
#endif

        case SDL_QUIT:
            msg->message = WM_QUIT;
            return 1;
        }
    }
    return 0;
}

static void swap_window(void) {
    SDL_GL_SwapWindow(window);
}

static void get_keyboard_state(unsigned int count, uint8_t* buffer) {
    memcpy(buffer, directinput_key_state, count);
}

static int get_mouse_buttons(int* pButton1, int* pButton2) {
    int state = SDL_GetMouseState(NULL, NULL);
    *pButton1 = state & SDL_BUTTON_LMASK;
    *pButton2 = state & SDL_BUTTON_RMASK;
    return 0;
}

static int get_mouse_position(int* pX, int* pY) {
    SDL_GetMouseState(pX, pY);

    if (*pX < vp_x) {
        *pX = vp_x;
    } else if (*pX >= vp_x + vp_width) {
        *pX = vp_x + vp_width - 1;
    }
    if (*pY < vp_y) {
        *pY = vp_y;
    } else if (*pY >= vp_y + vp_height) {
        *pY = vp_y + vp_height - 1;
    }
    *pX -= vp_x;
    *pY -= vp_y;
    *pX *= sdl_window_scale.x;
    *pY *= sdl_window_scale.y;

#if defined(DETHRACE_FIX_BUGS)
    // In hires mode (640x480), the menus are still rendered at (320x240),
    // so prescale the cursor coordinates accordingly.
    *pX *= gGraf_specs[gGraf_data_index].phys_width;
    *pX /= gGraf_specs[gReal_graf_data_index].phys_width;
    *pY *= gGraf_specs[gGraf_data_index].phys_height;
    *pY /= gGraf_specs[gReal_graf_data_index].phys_height;
#endif
    return 0;
}

static void set_palette(PALETTEENTRY_* pal) {
    GLRenderer_SetPalette((uint8_t*)pal);
}

int show_error_message(void* window, char* text, char* caption) {
    fprintf(stderr, "%s", text);
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, caption, text, window);
    return 0;
}

void Harness_Platform_Init(tHarness_platform* platform) {
    platform->ProcessWindowMessages = get_and_handle_message;
    platform->Sleep = SDL_Delay;
    platform->GetTicks = SDL_GetTicks;
    platform->CreateWindowAndRenderer = create_window_and_renderer;
    platform->ShowCursor = SDL_ShowCursor;
    platform->SetWindowPos = set_window_pos;
    platform->SwapWindow = swap_window;
    platform->DestroyWindow = destroy_window;
    platform->GetKeyboardState = get_keyboard_state;
    platform->GetMousePosition = get_mouse_position;
    platform->GetMouseButtons = get_mouse_buttons;
    platform->DestroyWindow = destroy_window;
    platform->ShowErrorMessage = show_error_message;

    platform->Renderer_BufferModel = GLRenderer_BufferModel;
    platform->Renderer_BufferMaterial = GLRenderer_BufferMaterial;
    platform->Renderer_BufferTexture = GLRenderer_BufferTexture;
    platform->Renderer_SetPalette = set_palette;
    platform->Renderer_FullScreenQuad = GLRenderer_FullScreenQuad;
    platform->Renderer_Model = GLRenderer_Model;
    platform->Renderer_ClearBuffers = GLRenderer_ClearBuffers;
    platform->Renderer_FlushBuffers = GLRenderer_FlushBuffers;
    platform->Renderer_BeginScene = GLRenderer_BeginScene;
    platform->Renderer_EndScene = GLRenderer_EndScene;
    platform->Renderer_SetViewport = GLRenderer_SetViewport;
}
