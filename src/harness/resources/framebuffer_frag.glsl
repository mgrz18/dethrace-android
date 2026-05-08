in vec2 v_tex_coord;

uniform usampler2D u_pixels;       // current pixmap (3D readback + 2D HUD), low-res
uniform usampler2D u_snapshot;     // pixmap snapshot taken right after the 3D
                                   // readback — anything different in u_pixels
                                   // is a 2D draw that landed after the 3D pass
uniform usampler2D u_super_3d;     // super-res 3D framebuffer (R8UI palette idx)
uniform sampler2D u_palette;


layout (location = 0) out vec4 out_frag_color;

void main(void) {
  uint ix_now  = texture(u_pixels,   v_tex_coord).r;
  uint ix_snap = texture(u_snapshot, v_tex_coord).r;

  // The 3D framebuffer is bottom-up (GL convention) but the pixmap was
  // y-flipped on readback, so flip y back when sampling the super-res image.
  uint ix_super = texture(u_super_3d, vec2(v_tex_coord.x, 1.0 - v_tex_coord.y)).r;

  uint chosen;
  if (ix_now != ix_snap) {
    // Pixel was modified after the 3D readback → it is a HUD/menu draw.
    chosen = ix_now;
  } else if (ix_super != 0u) {
    // Pixmap matches the post-readback snapshot and the super-res 3D wrote
    // something opaque here → use the high-res 3D pixel.
    chosen = ix_super;
  } else {
    // No 3D coverage and no post-readback 2D draw — keep whatever the
    // pixmap had (frontend menus, pre-3D 2D draws, transparent regions).
    chosen = ix_now;
  }

  vec4 texel = texelFetch(u_palette, ivec2(chosen, 0), 0);
  out_frag_color = vec4(texel.bgra);
}
