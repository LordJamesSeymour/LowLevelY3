uniform sampler2D u_texture;
uniform float u_time;
uniform vec2 u_resolution;

void main()
{
	vec2 uv = gl_TexCoord[0].xy;

	// Slow vertical refresh sweep from top to bottom.
	float refreshCenter = fract(u_time * 0.08);
	float refreshDistance = abs(uv.y - refreshCenter);
	float refreshBand = smoothstep(0.18, 0.0, refreshDistance) * 0.18;

	// Tiny horizontal wobble so the image feels slightly analog.
	float wobble = sin((uv.y * 140.0) + (u_time * 3.0)) * 0.0012;
	float refreshShift = refreshBand * 0.010;
	vec2 shiftedUv = uv + vec2(wobble + refreshShift, 0.0);

	shiftedUv.x = clamp(shiftedUv.x, 0.0, 1.0);
	shiftedUv.y = clamp(shiftedUv.y, 0.0, 1.0);

	// Mild RGB split.
	vec2 redUv = clamp(shiftedUv + vec2(0.0018, 0.0), 0.0, 1.0);
	vec2 blueUv = clamp(shiftedUv - vec2(0.0018, 0.0), 0.0, 1.0);

	vec4 baseSample = texture2D(u_texture, shiftedUv);
	float red = texture2D(u_texture, redUv).r;
	float green = baseSample.g;
	float blue = texture2D(u_texture, blueUv).b;

	vec3 color = vec3(red, green, blue);

	// Scanlines.
	float scanline = 0.92 + 0.08 * sin(uv.y * u_resolution.y * 1.15);

	// Slight flicker.
	float flicker = 0.992 + 0.008 * sin(u_time * 65.0);

	// Soft vignette so edges feel more screen-like.
	vec2 centered = uv - vec2(0.5, 0.5);
	float vignette = 1.0 - dot(centered, centered) * 0.65;
	vignette = clamp(vignette, 0.75, 1.0);

	// Brighten the moving refresh band a little.
	color *= scanline;
	color *= flicker;
	color *= vignette;
	color += vec3(refreshBand * 0.20);

	gl_FragColor = vec4(color, baseSample.a);
}