$input v_color0
$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(u_texColor,  0);

void main()
{
    vec4 texColor = texture2D(u_texColor, v_texcoord0);
    //gl_FragColor = texColor * v_color0;
    gl_FragColor = v_color0;
}
