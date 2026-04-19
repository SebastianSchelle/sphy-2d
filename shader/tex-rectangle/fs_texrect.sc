$input v_texcoord0, v_color0

#include <bgfx_shader.sh>

SAMPLER2DARRAY(u_texArray,  0);

void main()
{
	vec4 texColor = texture2DArray(u_texArray, v_texcoord0);
	vec4 finalColor = texColor * v_color0;
	//vec4 finalColor = v_color0;
	//if (finalColor.a < 0.001)
	//	discard;
	gl_FragColor = finalColor;
}
