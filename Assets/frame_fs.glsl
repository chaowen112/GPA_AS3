#version 410 core
uniform sampler2D tex;
uniform sampler2D channel;
uniform int barSwitch;
uniform int filterIndex;
uniform vec2 iResolution;
uniform float enlargeRate;
uniform float iTime=3;
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
vec4 PostFX(sampler2D tex, vec2 uv, float time)
{
    float radius = iResolution.x*1.4;
    float angle = sin(iTime);   //-1.+2.*
    vec2 center = vec2(iResolution.x*.8, iResolution.y)*1.5;
    
    vec2 texSize = vec2(iResolution.x/.6,iResolution.y/.5);
    vec2 tc = uv * texSize;
    tc -= center;
    float dist = length(tc*sin(iTime/5.));
    if (dist < radius)
    {
        float percent = (radius - dist) / radius;
        float theta = percent * percent * angle * 8.0;
        float s = sin(theta/2.);
        float c = cos(sin(theta/2.));
        tc = vec2(dot(tc, vec2(c, -s)), dot(tc, vec2(s, c)));
    }
    tc += center;
    vec3 color  = texture(channel,(tc / texSize)).rgb;
    vec3 color2 = texture(channel,(tc / texSize)).rgb;
    vec3 colmix = mix(color,color2,sin(time*.5));
    return vec4(colmix, 1.0);
}

float getVal(vec2 uv)
{
    return length(texture(tex,uv).xyz);
}

vec2 getGrad(vec2 uv,float delta)
{
    vec2 d=vec2(delta,0);
    return vec2(
                getVal(uv+d.xy)-getVal(uv-d.xy),
                getVal(uv+d.yx)-getVal(uv-d.yx)
                )/delta;
}

float noise( in vec3 x ) // From iq
{
    vec3 p = floor(x);
    vec3 f = fract(x);
    f = f*f*(3.0-2.0*f);
    
    vec2 uv = (p.xy+vec2(37.0,17.0)*p.z) + f.xy;
    vec2 rg = texture(channel, (uv+0.5)/256.0, -100.0 ).yx;
    return mix( rg.x, rg.y, f.z )*2.0 - 1.0;
}

vec2 offset( vec2 p, float phase )
{
    return vec2(noise(vec3(p.xy*10.0+iTime*0.25,phase)),
                noise(vec3(p.yx*10.0+iTime*0.25,phase)))*0.02;
}

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
            //color = vec4(tex_color, 1.0);
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
        case 3: // sharp
            lap_color = vec3(0.0);
            for (int i = 0; i < 9; i++)
            {
                sample_tex[i] = texture(tex, fs_in.texcoord + tcOffset[i]/600).rgb;
            }
            lap_color = 9 * sample_tex[4];
            for(int i = 0; i < 9; i++){
                if(i != 4){
                    lap_color -= sample_tex[i].xyz;
                }
            }
            color = vec4(lap_color,1);
            break;
        case 4:
            // magnified
            vec2 p = vec2(fs_in.texcoord.x, fs_in.texcoord.y);
            vec2 m = vec2(0.5, 0.5);
            vec2 d = p - m;
            float r = sqrt(dot(d, d));
            float lensSize = 0.4;
            if(length(d) + r > lensSize + 0.01){
                color = texture(tex, fs_in.texcoord.xy);
            }else if(length(d) + r < lensSize - 0.01){
                vec2 coord = vec2(0.5,0.5) + (fs_in.texcoord.xy - vec2(0.5,0.5)) / enlargeRate;
                color = texture(tex, vec2(coord.x, coord.y));
            }
            break;
        case 5:
            // pixelize
            float pixels = iResolution.x;
            float dx = 5.0 * (1.0 / pixels);
            float dy = 5.0 * (1.0 / pixels);
            vec2 coord = vec2(dx * floor(fs_in.texcoord.x / dx),
                              dy * floor(fs_in.texcoord.y / dy));
            color = texture(tex, coord);
            break;
        case 6:
            // fisheye
            float maxF = sin(0.5 * 178.0 * (PI / 180.0));
            vec2 xy = 2.0 * fs_in.texcoord.xy - 1.0;
            float len = length(xy);
            if(len < (2.0 - maxF)){
                float z = sqrt(1.0 - len*len);
                float r = atan(len, z) / PI;
                float phi = atan(xy.y, xy.x);
                float x = r * cos(phi) + 0.5;
                float y = r * sin(phi) + 0.5;
                vec2 coord = vec2(x, y);
                color = texture(tex, coord);
            }else{
                color = texture(tex, fs_in.texcoord.xy);
            }
            break;
        case 7:
            // gammailze
            float gamma = 0.6;
            float color_num = 8.0;
            vec3 c = texture(tex,  fs_in.texcoord).rgb;
            c = pow(c, vec3(gamma, gamma, gamma));
            c = c * color_num;
            c = floor(c);
            c = c / color_num;
            c = pow(c, vec3(1.0/gamma));
            color = vec4(c, 1.0);
            break;
        case 8:
            // bloom
            vec2 img_size = iResolution;
            color = vec4(0);
            int n = 0;
            half_size = 2;
            for ( int i = -half_size; i <= half_size; ++i ) {
                for ( int j = -half_size; j <= half_size; ++j ) {
                    vec4 c = texture(tex, fs_in.texcoord + vec2(i,j)/600.0);
                    color+= c;
                    n++;
                }
            }
            vec4 blur_color1 = color / n;
            
            n = 0;
            half_size = 3;
            for ( int i = -half_size; i <= half_size; ++i ) {
                for ( int j = -half_size; j <= half_size; ++j ) {
                    vec4 c = texture(tex, fs_in.texcoord + vec2(i,j)/600.0);
                    color+= c;
                    n++;
                }
            }
            vec4 blur_color2 = color / n;
            
            vec4 origin_color = texture(tex, fs_in.texcoord.xy);
            
            color = origin_color * 0.5 + blur_color1 * 0.2 + blur_color2 * 0.4;
            break;
        case 9:
            // Relief
            vec2 uv = fs_in.texcoord.xy;
            vec3 nnn = vec3(getGrad(uv,1.0/600),150.0);
            //n *= n;
            nnn=normalize(nnn);
            color=vec4(nnn,1);
            vec3 light = normalize(vec3(1,1,2));
            float diff=clamp(dot(nnn,light),0.5,1.0);
            float spec=clamp(dot(reflect(light,nnn),vec3(0,0,-1)),0.0,1.0);
            spec=pow(spec,36.0)*2.5;
            spec=0.0;
            color = texture(tex,uv)*vec4(diff)+vec4(spec);
            break;
        case 10:
            // kaleidoscope
            vec2 tc = fs_in.texcoord.xy;
            p = -1.0 + 2.0 * tc;
            len = length(p);
            coord = tc + (p/len)*cos(len*12.0-iTime*4.0)*0.03;
            color = texture(tex, coord);
            break;
        case 11:
            // better filtering
            p = fs_in.texcoord.xy;
            uv = p*0.1;
            
            //---------------------------------------------
            // regular texture map filtering
            //---------------------------------------------
            vec3 colA = texture( tex, uv ).xyz;
            
            //---------------------------------------------
            // my own filtering
            //---------------------------------------------
            float textureResolution = 64.0;
            uv = uv*textureResolution + 0.5;
            vec2 iuv = floor( uv );
            vec2 fuv = fract( uv );
            uv = iuv + fuv*fuv*(3.0-2.0*fuv); // fuv*fuv*fuv*(fuv*(fuv*6.0-15.0)+10.0);;
            uv = (uv - 0.5)/textureResolution;
            vec3 colB = texture( tex, uv ).xyz;
            
            //---------------------------------------------
            // mix between the two colors
            //---------------------------------------------
            float f = sin(3.14*p.x + 0.7*iTime);
            vec3 col = mix( colA, colB, smoothstep( -0.1, 0.1, f ) );
            col *= smoothstep( 0.0, 0.01, abs(f-0.0) );
            
            color = vec4( col, 1.0 );
            break;
        case 12:
            // spirlt
            p = fs_in.texcoord.xy;
            p.x *= iResolution.x / iResolution.y;
            vec4 fragcolor = vec4(1);
            for (int i = 0; i < 3; i++) {
            float phase = float(i)/3.0;
            vec4 s1 = texture(tex, p + offset(p, fract(phase)));
            vec4 s2 = texture(tex, p + offset(p, fract(phase+0.5)));
            fragcolor += mix(s1, s2, abs(mod(phase*2.0, 2.0)-1.0))*fragcolor*fragcolor;
            }
            color = fragcolor/20.0;
            break;
        case 13:
            // swirl
            uv = fs_in.texcoord.xy;
            color = PostFX(tex,uv*1.5,iTime);
            break;
        default:
            // normal
            color = texture(tex,fs_in.texcoord);
    }
    if(barSwitch == 1){
        if(gl_FragCoord.x < iResolution.x/2)
            color = texture(tex,fs_in.texcoord);
    }
}
