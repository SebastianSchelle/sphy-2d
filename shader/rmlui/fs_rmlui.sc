$input v_color0
$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2DARRAY(u_texArray,  0);

uniform vec4 u_texLayer;

void main()
{
    vec3 uvw = vec3(v_texcoord0.xy, u_texLayer.x);
    vec4 texColor = texture2DArray(u_texArray, uvw);
    gl_FragColor = texColor * v_color0;
    //gl_FragColor = v_color0;
}
