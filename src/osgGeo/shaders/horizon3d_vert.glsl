#version 330 compatibility

uniform sampler2D heightMap;
uniform float depthMin;
uniform float depthDiff;

varying float depthOut;
out int undef;

@if("hasGeomShader")
out vec2 texCoordPass;
@else
out vec2 texCoordOut;
@endif

void main(void)
{
    vec4 pos = gl_Vertex;
    vec2 texCoord = gl_MultiTexCoord0.st;
    vec4 depthMask = texture2D(heightMap, texCoord);
    float depthComp = depthMask.r;
    depthOut = depthMin + depthComp * depthDiff;
    pos.z = depthOut;
    undef = (depthMask.a > 0.1) ? 1 : 0;
    gl_Position = gl_ModelViewProjectionMatrix * pos;
    @if("hasGeomShader")
        texCoordPass = texCoord;
    @else
        texCoordOut = texCoord;
    @endif
}
