#version 330

uniform samplerCube u_cubemapTex;
uniform float u_glassAlpha;

in vec3 v_position, v_normal;
in vec2 v_texcoord;

in vec3 v_refraction;
in vec3 v_reflection;
in float v_fresnel;

out vec4 f_color;

void main()
{
    vec4 refractionColor = texture(u_cubemapTex, normalize(v_refraction));
    vec4 reflectionColor = texture(u_cubemapTex, normalize(v_reflection));

    f_color = mix(refractionColor, reflectionColor, v_fresnel);
    f_color.a *= 1.0; // u_glassAlpha
}
