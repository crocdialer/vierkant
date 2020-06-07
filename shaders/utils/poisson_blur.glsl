// blur using a random poisson-disk sampling pattern

const int NUM_TAPS = 12;
const vec2 fTaps_Poisson[NUM_TAPS] = vec2[](vec2(-.326,-.406), vec2(-.840,-.074), vec2(-.696, .457), vec2(-.203, .621),
    vec2(.962,-.195), vec2( .473,-.480), vec2( .519, .767), vec2( .185,-.893), vec2( .507, .064), vec2( .896, .412),
    vec2(-.322,-.933), vec2(-.792,-.598));

float nrand(vec2 n)
{
	return fract(sin(dot(n.xy, vec2(12.9898, 78.233))) * 43758.5453);
}

vec2 rot2d(vec2 p, float a)
{
	vec2 sc = vec2(sin(a),cos(a));
	return vec2(dot(p, vec2(sc.y, -sc.x)), dot(p, sc.xy));
}

vec4 poisson_blur(in sampler2D the_texture, in vec2 tex_coord, in vec2 size)
{
    float rnd = 6.28 * nrand(tex_coord);
    vec4 color_sum = vec4(0);

	vec4 basis = vec4(rot2d(vec2(1, 0), rnd), rot2d(vec2(0, 1), rnd));

	for(int i = 0; i < NUM_TAPS; i++)
	{
	    vec2 ofs = fTaps_Poisson[i];
	    ofs = vec2(dot(ofs, basis.xz), dot(ofs, basis.yw));

	    vec2 poisson_coord = tex_coord + size * ofs;
        color_sum += texture(the_texture, poisson_coord);
    }
    return color_sum / NUM_TAPS;
}