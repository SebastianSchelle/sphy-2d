$input v_color0
$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(u_texColor,  0);

void main()
{
    // Use vertex color directly for now (texture support can be added later)
    gl_FragColor = v_color0;
}
