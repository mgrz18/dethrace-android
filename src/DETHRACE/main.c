
#include <stdlib.h>

#ifdef __ANDROID__
#include <SDL.h>
#endif

#ifdef _WIN32
#include <io.h>
#include <stdio.h>
#include <windows.h>
#endif

extern void Harness_Init(int* argc, char* argv[]);
extern int original_main(int pArgc, char* pArgv[]);

#ifdef __ANDROID__
extern int gGraf_spec_index;
#endif

int main(int argc, char* argv[]) {
#ifdef _WIN32
    /* Attach to the console that started us if any */
    if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        /* We attached successfully, lets redirect IO to the consoles handles if not already redirected */
        if (_fileno(stdout) == -2 || _get_osfhandle(_fileno(stdout)) == -2) {
            freopen("CONOUT$", "w", stdout);
        }

        if (_fileno(stderr) == -2 || _get_osfhandle(_fileno(stderr)) == -2) {
            freopen("CONOUT$", "w", stderr);
        }

        if (_fileno(stdin) == -2 || _get_osfhandle(_fileno(stdin)) == -2) {
            freopen("CONIN$", "r", stdin);
        }
    }
#endif

    Harness_Init(&argc, argv);

#ifdef __ANDROID__
    /* Force the 640×480 graphics spec on Android. The 3D scene then renders
     * at the higher resolution before the device's widescreen widening kicks
     * in (DRAndroid_WidenForDeviceAspect stretches it further to fill the
     * panel). The original_main() arg parser also accepts -hires, but on
     * Android the launcher does not pass any argv, so we set the spec index
     * directly. */
    gGraf_spec_index = 1;
#endif

    return original_main(argc, argv);
}
