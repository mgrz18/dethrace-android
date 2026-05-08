#ifndef HARNESS_PLATFORMS_ANDROID_WIDESCREEN_H
#define HARNESS_PLATFORMS_ANDROID_WIDESCREEN_H

#ifdef __ANDROID__

/* Widen the active BRender graphics spec (gGraf_specs[gGraf_spec_index]) and
 * its matching gGraf_data entry so the allocated pixmap matches the device's
 * landscape aspect ratio. The 3D pass writes pixels at the widened size; the
 * 2D HUD/menu draws stay at the original (base) width and are centered inside
 * the widened pixmap by PDAllocateScreenAndBack via origin_x.
 *
 * In/out: *width, *height start at the requested base spec dimensions and are
 * updated to the widened ones on return. Caller passes those to the SDL
 * window creation. */
void DRAndroid_WidenForDeviceAspect(int *width, int *height);

/* Pre-widening width of the active spec. 0 until DRAndroid_WidenForDeviceAspect
 * has been called. PDAllocateScreenAndBack uses it to compute origin_x; the GL
 * swap_window uses it to clear the columns outside the centered draw area. */
int DRAndroid_GetBaseWidth(void);

#endif /* __ANDROID__ */

#endif /* HARNESS_PLATFORMS_ANDROID_WIDESCREEN_H */
