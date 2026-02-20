vec2 a_position : POSITION;
half4 a_color0 : COLOR0;
vec2 a_texcoord0 : TEXCOORD0;
vec2 a_texcoord1 : TEXCOORD1;
float a_texcoord2 : TEXCOORD2;
vec3 a_texcoord3 : TEXCOORD3;

vec4 v_color : COLOR0 = vec4(1.0, 1.0, 1.0, 1.0);
vec2 v_uv : TEXCOORD0 = vec2(0.0, 0.0);
vec2 v_shape : TEXCOORD1 = vec2(0.0, 0.0);
float v_thicknessY : TEXCOORD2 = 0.0;
