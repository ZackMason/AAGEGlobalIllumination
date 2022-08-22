#version 460 core
out vec4 FragColor;

in VS_OUT {
    vec3 pos;
    vec3 normal;
    vec2 uv;
    vec2 luv;
    float lum;
} fs_in;

uniform sampler2D uAlbedo;

void main() {
    vec3 color = texture(uAlbedo, fs_in.uv).rgb;

    //FragColor.rgb = color.rgb * max(0.3, dot(normalize(fs_in.normal), normalize(vec3(1,2,1.5))));
    FragColor.rgb = color.rgb * fs_in.lum;

    //FragColor.rgb = fs_in.normal;
    
    FragColor.a = 1.0;
}
