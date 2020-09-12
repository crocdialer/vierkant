/*
DoF with bokeh GLSL shader v2.4
by Martins Upitis (martinsh) (devlog-martinsh.blogspot.com)

----------------------
The shader is Blender Game Engine ready, but it should be quite simple to adapt for your engine.

This work is licensed under a Creative Commons Attribution 3.0 Unported License.
So you are free to share, modify and adapt it for your needs, and even use it for commercial use.
I would also love to hear about a project you are using it.

Have fun,
Martins
----------------------

changelog:

2.4:
- physically accurate DoF simulation calculated from "u_focal_depth" ,"u_focal_length", "f-stop" and "u_circle_of_confusion_sz" parameters.
- option for artist controlled DoF simulation calculated only from "u_focal_depth" and individual controls for near and far blur
- added "circe of confusion" (u_circle_of_confusion_sz) parameter in mm to accurately simulate DoF with different camera sensor or film sizes
- cleaned up the code
- some optimization

2.3:
- new and physically little more accurate DoF
- two extra input variables - focal length and aperture iris diameter
- added a debug visualization of focus point and focal range

2.1:
- added an option for pentagonal bokeh shape
- minor fixes

2.0:
- variable sample count to increase quality/performance
- option to blur depth buffer to reduce hard edges
- option to dither the samples with noise or pattern
- bokeh chromatic aberration/fringing
- bokeh bias to bring out bokeh edges
- image thresholding to bring out highlights when image is out of focus

*/

struct dof_settings_t
{
    //! flag indicating if DoF should be applied
    bool enabled;

    //! focal distance value in meters, but you may use u_auto_focus option below
    float focal_depth;

    //! focal length in mm
    float focal_length;

    //! f-stop value
    float fstop;

    //! circle of confusion size in mm (35mm film = 0.03mm)
    float circle_of_confusion_sz;

    //! highlight gain;
    float gain;

    //! bokeh chromatic aberration/fringing
    float fringe;

    //! use auto-focus in shader? disable if you use external focal_depth value
    bool auto_focus;

    //! show debug focus point and focal range (red = focal point, green = focal range)
    bool debug_focus;
};

#define PI 3.1415926535897932384626433832795

//------------------------------------------
//user variables

int samples = 3;//samples on the first ring
int rings = 3;//ring count

//manual dof calculation
bool manualdof = false;

float ndofstart = 1.0;//near dof blur start
float ndofdist = 2.0;//near dof blur falloff distance
float fdofstart = 1.0;//far dof blur start
float fdofdist = 3.0;//far dof blur falloff distance

bool vignetting = true;//use optical lens vignetting?
float vignout = 1.3;//vignetting outer border
float vignin = 0.0;//vignetting inner border
float vignfade = 22.0;//f-stops till vignete fades

// u_auto_focus point on screen (0.0,0.0 - left lower corner, 1.0,1.0 - upper right)
vec2 focus = vec2(0.5, 0.5);

//clamp value of max blur (0.0 = no blur,1.0 default)
float maxblur = 1.0;

// highlight threshold;
float threshold = 0.5;

//bokeh edge bias
float bias = 0.5;

//use noise instead of pattern for sample dithering
bool noise = true;

//dither amount
float namount = 0.0001;

//blur the depth buffer?
bool depthblur = false;

//depthblursize
float dbsize = 1.25;

/*
next part is experimental
not looking good with small sample and ring count
looks okay starting from samples = 4, rings = 4
*/

bool pentagon = false;//use pentagon as bokeh shape?
float feather = 0.4;//pentagon shape feather

//------------------------------------------


float penta(vec2 coords)//pentagonal shape
{
    float scale = float(rings) - 1.3;
    vec4  HS0 = vec4(1.0, 0.0, 0.0, 1.0);
    vec4  HS1 = vec4(0.309016994, 0.951056516, 0.0, 1.0);
    vec4  HS2 = vec4(-0.809016994, 0.587785252, 0.0, 1.0);
    vec4  HS3 = vec4(-0.809016994, -0.587785252, 0.0, 1.0);
    vec4  HS4 = vec4(0.309016994, -0.951056516, 0.0, 1.0);
    vec4  HS5 = vec4(0.0, 0.0, 1.0, 1.0);

    vec4  one = vec4(1.0);

    vec4 P = vec4((coords), vec2(scale, scale));

    vec4 dist = vec4(0.0);
    float inorout = -4.0;

    dist.x = dot(P, HS0);
    dist.y = dot(P, HS1);
    dist.z = dot(P, HS2);
    dist.w = dot(P, HS3);

    dist = smoothstep(-feather, feather, dist);

    inorout += dot(dist, one);

    dist.x = dot(P, HS4);
    dist.y = HS5.w - abs(P.z);

    dist = smoothstep(-feather, feather, dist);
    inorout += dist.x;

    return clamp(inorout, 0.0, 1.0);
}

// blur depth values
float blur_depth(sampler2D tex, vec2 coord, vec2 viewport_size)
{
    float d = 0.0;

    float kernel[9];
    kernel[0] = 1.0/16.0;   kernel[1] = 2.0/16.0;   kernel[2] = 1.0/16.0;
    kernel[3] = 2.0/16.0;   kernel[4] = 4.0/16.0;   kernel[5] = 2.0/16.0;
    kernel[6] = 1.0/16.0;   kernel[7] = 2.0/16.0;   kernel[8] = 1.0/16.0;

    vec2 offset[9];

    // texel size
    vec2 texel = vec2(1.0 / viewport_size.x, 1.0 / viewport_size.y);
    vec2 wh = vec2(texel.x, texel.y) * dbsize;

    offset[0] = vec2(-wh.x, -wh.y);
    offset[1] = vec2(0.0, -wh.y);
    offset[2] = vec2(wh.x -wh.y);

    offset[3] = vec2(-wh.x, 0.0);
    offset[4] = vec2(0.0, 0.0);
    offset[5] = vec2(wh.x, 0.0);

    offset[6] = vec2(-wh.x, wh.y);
    offset[7] = vec2(0.0, wh.y);
    offset[8] = vec2(wh.x, wh.y);

    for (int i = 0; i < 9; i++)
    {
        float tmp = texture(tex, coord + offset[i]).r;
        d += tmp * kernel[i];
    }
    return d;
}

// processing the sample
vec3 color(sampler2D tex, vec2 coord, vec2 viewport_size, float blur, const dof_settings_t p)
{
    vec3 col = vec3(0.0);

    // texel size
    vec2 texel = vec2(1.0 / viewport_size.x, 1.0 / viewport_size.y);

    col.r = texture(tex, coord + vec2(0.0, 1.0) * texel * p.fringe * blur).r;
    col.g = texture(tex, coord + vec2(-0.866, -0.5) * texel * p.fringe * blur).g;
    col.b = texture(tex, coord + vec2(0.866, -0.5) * texel * p.fringe * blur).b;

    vec3 lumcoeff = vec3(0.299, 0.587, 0.114);
    float lum = dot(col.rgb, lumcoeff);
    float thresh = max((lum - threshold) * p.gain, 0.0);
    return col + mix(vec3(0.0), col, thresh * blur);
}

//generating noise/pattern texture for dithering
vec2 rand(vec2 size, vec2 coord)
{
    float noiseX = ((fract(1.0 - coord.s*(size.x/2.0))*0.25)+(fract(coord.t*(size.y/2.0))*0.75))*2.0-1.0;
    float noiseY = ((fract(1.0-coord.s*(size.x/2.0))*0.75)+(fract(coord.t*(size.y/2.0))*0.25))*2.0-1.0;

    if (noise)
    {
        noiseX = clamp(fract(sin(dot(coord, vec2(12.9898, 78.233))) * 43758.5453), 0.0, 1.0)*2.0-1.0;
        noiseY = clamp(fract(sin(dot(coord, vec2(12.9898, 78.233)*2.0)) * 43758.5453), 0.0, 1.0)*2.0-1.0;
    }
    return vec2(noiseX, noiseY);
}

vec3 debugFocus(vec3 col, float blur, float depth)
{
    //distance based edge smoothing
    float edge = 0.002 * depth;
    float m = clamp(smoothstep(0.0, edge, blur), 0.0, 1.0);
    float e = clamp(smoothstep(1.0 - edge, 1.0, blur), 0.0, 1.0);

    col = mix(col, vec3(1.0, 0.5, 0.0), (1.0 - m) * 0.6);
    col = mix(col, vec3(0.0, 0.5, 1.0), ((1.0 - e) - (1.0 - m)) * 0.2);

    return col;
}

float linearize(float depth, float near, float far)
{
//    return -far * near / (depth * (far - near) - far);
    return near * far / (far + depth * (near - far));
}

float vignette(vec2 coord, dof_settings_t p)
{
    float dist = distance(coord, vec2(0.5, 0.5));
    dist = smoothstep(vignout + (p.fstop / vignfade), vignin + (p.fstop / vignfade), dist);
    return clamp(dist, 0.0, 1.0);
}

vec4 depth_of_field(sampler2D color_map, sampler2D depth_map, vec2 coord, vec2 viewport_size, vec2 clip_distances,
                    const dof_settings_t params)
{
    // scene depth calculation
    float raw_depth = texture(depth_map, coord).x;
    float depth = linearize(raw_depth, clip_distances.x, clip_distances.y);

    if(depthblur)
    {
        float depth_blurred = blur_depth(depth_map, coord, viewport_size);
        depth = linearize(depth_blurred, clip_distances.x, clip_distances.y);
    }

    // focal plane calculation
    float fDepth = params.focal_depth;

    // autofocus will use a sampled depth-value at the focus point
    if(params.auto_focus){ fDepth = linearize(texture(depth_map, focus).x, clip_distances.x, clip_distances.y); }

    // dof blur factor calculation
    float blur = 0.0;

    if(manualdof)
    {
        //focal plane
        float a = depth - fDepth;

        //far DoF
        float b = (a - fdofstart) / fdofdist;

        //near Dof
        float c = (-a - ndofstart) / ndofdist;
        blur = (a > 0.0) ? b : c;
    }
    else
    {
        // focal length in mm
        float f = params.focal_length;

        // focal plane in mm
        float d = fDepth * 1000.0;

        // depth in mm
        float o = depth * 1000.0;

        float a = (o * f) / (o - f);
        float b = (d * f) / (d - f);
        float c = (d - f) / (d * params.fstop * params.circle_of_confusion_sz);

        blur = abs(a - b) * c;
    }

    blur = clamp(blur, 0.0, maxblur);

    // calculation of pattern for ditering
    vec2 noise = rand(viewport_size, coord) * namount * blur;

    // getting blur x and y step factor
    float w = (1.0 / viewport_size.x) * blur * maxblur + noise.x;
    float h = (1.0 / viewport_size.y) * blur * maxblur + noise.y;

    // keep raw color
    vec4 raw_color = texture(color_map, coord);

    // calculation of final color
    vec3 col = vec3(0.0);

    //some optimization thingy
    if (blur < 0.05){ col = raw_color.rgb; }
    else
    {
        col = raw_color.rgb;
        float s = 1.0;
        int ringsamples;

        for (int i = 1; i <= rings; i += 1)
        {
            ringsamples = i * samples;

            for (int j = 0; j < ringsamples; j += 1)
            {
                float step = PI * 2.0 / float(ringsamples);
                float pw = (cos(float(j) * step) * float(i));
                float ph = (sin(float(j) * step) * float(i));
                float p = 1.0;

                // pentagon shape
                if (pentagon){ p = penta(vec2(pw, ph)); }

                col += color(color_map, coord + vec2(pw * w, ph * h), viewport_size, blur, params) * mix(1.0, float(i) / float(rings), bias) * p;
                s += 1.0 * mix(1.0, (float(i))/(float(rings)), bias)*p;
            }
        }
        col /= s;//divide by sample count
    }

    // debug output
    if(params.debug_focus){ col = debugFocus(col, blur, depth); }

    // optional vignetting
    if(vignetting){ col *= vignette(coord, params); }

    return vec4(col, raw_color.a);
}
