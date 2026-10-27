uniform sampler2D tex;
uniform float inv_gamma, exposure;

vec3 acesfilm(vec3 x)
{
	return clamp((x * (x * 2.51 + 0.03)) / (x * (x * 2.43 + 0.59) + 0.14), 0.0, 1.0);
}

vec3 reinhard(vec3 x)
{
	return x / (1.0 + x);
}

void main()
{
	vec4 texel = texture2D(tex, gl_TexCoord[0].st);
	vec3 linear = (texel.xyz / texel.w) * exposure;

#ifdef TMAP_ACES
	vec3 color = acesfilm(linear);
#elif defined(TMAP_REINHARD)
	vec3 color = reinhard(linear);
#else
	vec3 color = linear;
#endif

	gl_FragColor.rgb = pow(color, vec3(inv_gamma));
	gl_FragColor.a = 1.0;
}
