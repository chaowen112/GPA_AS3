#version 410 core
uniform sampler2D tex;
uniform int barSwitch;
uniform int filterIndex;
uniform vec2 iResolution;
out vec4 color;
const float PI = 3.1415926535;

in VS_OUT {
    vec2 texcoord;
} fs_in;

float sigma_e = 2.0f;
float sigma_r = 2.8f;
float phi = 3.4f;
float tau = 0.99f;
float twoSigmaESquared = 2.0 * sigma_e * sigma_e;
float twoSigmaRSquared = 2.0 * sigma_r * sigma_r;
int halfWidth = int(ceil( 2.0 * sigma_r ));
vec2 tcOffset[9] =
vec2[](vec2(-1,-1),
       vec2( 0,-1),
       vec2( 1,-1),
       vec2(-1, 0),
       vec2( 0, 0),
       vec2( 1, 0),
       vec2( 1,-1),
       vec2( 1, 0),
       vec2( 1, 1)
       );

void main(void)
{
    switch(filterIndex)
    {
        case 0:
            // DOG
            vec2 sum = vec2(0.0);
            vec2 norm = vec2(0.0);
            
            for(int i = -halfWidth; i <= halfWidth; ++i){
                for(int j = -halfWidth; j <= halfWidth; ++j){
                    float d = length(vec2(i,j));
                    vec2 kernel = vec2(exp(-d*d/twoSigmaESquared),
                                       exp(-d*d/twoSigmaRSquared));
                    vec4 c = texture(tex, fs_in.texcoord+vec2(i,j)/vec2(600,600));
                    vec2 L = vec2(0.299 * c.r + 0.587*c.g + 0.114*c.b);
                    
                    norm += 2.0*kernel;
                    sum +=kernel * L;
                }
            }
            sum /= norm;
            float H = 100.0 * (sum.x - tau * sum.y);
            float edge = (H > 0.0)?1.0:2.0 * smoothstep(-2.0, 2.0, phi*H);
            
            // blur
            int half_size = 2;
            vec4 color_sum = vec4(0);
            for (int i = -half_size; i <= half_size ; ++i)
            {
                for (int j = -half_size; j <= half_size ; ++j) {
                    ivec2 coord = ivec2(gl_FragCoord.xy) + ivec2(i, j);
                    color_sum += texelFetch(tex, coord, 0);
                }
            }
            // drag
            int sample_count = (half_size * 2 + 1) * (half_size * 2 + 1);
            float nbins = 8.0;
            vec3 tex_color = texelFetch(tex, ivec2(gl_FragCoord.xy), 0).rgb; tex_color = floor(tex_color * nbins) / nbins;
            
            color = (
                     (color_sum / sample_count)
                     + vec4(tex_color, 1.0)
                     +vec4(edge, edge, edge, 1.0))/3;
            break;
        case 1:
            // blue red
            vec4 texture_color_Left = texture(tex,fs_in.texcoord-vec2(0.01,0.0));
            vec4 texture_color_Right = texture(tex,fs_in.texcoord+vec2(0.01,0.0));
            vec4 texture_color = vec4(texture_color_Left.r*0.29
                                      +texture_color_Left.g*0.58
                                      +texture_color_Left.b*0.114,
                                      texture_color_Right.g,
                                      texture_color_Right.b,0.0);
            color = texture_color;
            break;
        case 2:
            // laplacian
            vec3 sample_tex[9];
            vec3 lap_color = vec3(0.0);
            for (int i = 0; i < 9; i++)
            {
                // Sample a grid around and including our texel
                vec3 lap_gray = texture(tex, fs_in.texcoord + tcOffset[i]/600).rgb;
                float gray = (lap_gray.r+lap_gray.g+lap_gray.b)/3;
                sample_tex[i] = vec3(gray, gray, gray);
            }
            lap_color = 8 * sample_tex[4];
            for(int i = 0; i < 9; i++){
                if(i != 4){
                    lap_color -= sample_tex[i].xyz;
                }
            }
            color = vec4(lap_color,1);
            break;
        case 3:
            vec2 img_size = vec2(600,600);
            color = vec4(0);
            int n = 0;
            half_size = 2;
            for ( int i = -half_size; i <= half_size; ++i ) {
                for ( int j = -half_size; j <= half_size; ++j ) {
                    vec4 c = texture(tex, fs_in.texcoord + vec2(i,j)/img_size);
                    color+= c;
                    n++;
                }
            }
            vec4 blur_color1 = color / n;
            
            n = 0;
            half_size = 3;
            for ( int i = -half_size; i <= half_size; ++i ) {
                for ( int j = -half_size; j <= half_size; ++j ) {
                    vec4 c = texture(tex, fs_in.texcoord + vec2(i,j)/img_size);
                    color+= c;
                    n++;
                }
            }
            vec4 blur_color2 = color / n;
            
            vec4 origin_color = texture(tex, fs_in.texcoord.xy);
            
            color = origin_color * 0.7 + blur_color1 * 0.2 + blur_color2 * 0.4;
        default:
            color = texture(tex,fs_in.texcoord);
    }
    if(barSwitch == 1){
        if(gl_FragCoord.x < 300)
            color = texture(tex,fs_in.texcoord);
    }
}
