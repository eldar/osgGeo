#version 330 compatibility

uniform vec4 colour;
uniform sampler2D heightMap;
varying vec2 texCoordOut;

void main(void)
{
    vec4 pix = texture2D(heightMap, texCoordOut);
    vec4 col = vec4(pix.g, pix.g, pix.g, 1.0);
    gl_FragColor = pix;
}
