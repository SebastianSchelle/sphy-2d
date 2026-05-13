$input v_texcoord0, v_color0

#include <bgfx_shader.sh>

SAMPLER2DARRAY(u_texArray,  0);

void main()
{
	vec4 texColor = texture2DArray(u_texArray, v_texcoord0);
	// Premultiplied atlas; instance colour is straight RGBA (tint).
	float outA = texColor.a * v_color0.a;
	vec4 finalColor = vec4(texColor.rgb * v_color0.rgb * v_color0.a, outA);
	if (finalColor.a < 0.001)
		discard;
	gl_FragColor = finalColor;
}
