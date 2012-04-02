#version 130

uniform sampler2D heightMap;
in vec2 texCoordOut;

in float diffuseValue;

void main(void)
{
    vec4 pix = texture2D(heightMap, texCoordOut);
    vec4 col = vec4(pix.g, pix.g, pix.g, 1.0);
    gl_FragColor = col * diffuseValue;
}
