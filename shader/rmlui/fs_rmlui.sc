$input v_color0
$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2DARRAY(u_texArray,  0);

uniform vec4 u_texLayer;
uniform vec4 u_atlasPos;

void main()
{
    vec2 uv = u_atlasPos.xy + v_texcoord0.xy * u_atlasPos.zw;
    vec3 uvw = vec3(uv, u_texLayer.x);
    vec4 texColor = texture2DArray(u_texArray, uvw);
    vec4 finalColor = texColor * v_color0;
    // Discard fully transparent pixels to avoid rendering background through transparent borders
    if (finalColor.a < 0.001)
        discard;
    gl_FragColor = finalColor;
}
