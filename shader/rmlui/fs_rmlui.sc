$input v_color0
$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2DARRAY(u_texArray,  0);

uniform vec4 u_texLayer;
uniform vec4 u_uvRect;

void main()
{
    vec2 atlasUv = mix(u_uvRect.xy, u_uvRect.zw, v_texcoord0.xy);
    vec3 uvw = vec3(atlasUv, u_texLayer.x);
    vec4 texColor = texture2DArray(u_texArray, uvw);
    gl_FragColor = texColor * v_color0;
    //gl_FragColor = v_color0;
}
