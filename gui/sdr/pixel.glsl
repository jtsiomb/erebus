uniform sampler2D tex;
uniform float inv_gamma;

void main()
{
	vec4 texel = texture2D(tex, gl_TexCoord[0].st);
	gl_FragColor.rgb = pow(texel.xyz / texel.w, vec3(inv_gamma));
	gl_FragColor.a = 1.0;
}
