#version 130

uniform sampler2D heightMap;
uniform sampler2D normalX;
uniform sampler2D normalY;
uniform sampler2D normalZ;
uniform float depthMin;
uniform float depthDiff;

uniform mat3 osg_NormalMatrix;
uniform mat4 osg_ModelViewProjectionMatrix;
uniform mat4 osg_ModelViewMatrix;

in vec4 osg_MultiTexCoord0;
in vec4 osg_Vertex;
out float depthOut;
out int undef;

@if("hasGeomShader")
out vec2 texCoordPass;
out float diffuseValuePass;
@else
out vec2 texCoordOut;
out float diffuseValue;
@endif

void main(void)
{
    // Extract texture coordinate
    vec2 texCoord = osg_MultiTexCoord0.st;

    // fetch value from the height map. Luminance component
    // contains elevation value, alpha contains undefined flag
    vec4 depthMask = texture2D(heightMap, texCoord);
    float depthComp = depthMask.r;

    // denormalize depth value
    depthOut = depthMin + depthComp * depthDiff;
    vec4 pos = vec4(osg_Vertex.xy, depthOut, 1.0);
    gl_Position = osg_ModelViewProjectionMatrix * pos;

    // pass undefined value to geometry shader
    undef = (depthMask.a > 0.1) ? 1 : 0;

    // Normals and lighting

    //read normal components from textures
    vec3 normX = texture2D(normalX, texCoord).xyz;
    vec3 normY = texture2D(normalY, texCoord).xyz;
    vec3 normZ = texture2D(normalZ, texCoord).xyz;

    vec3 normal = vec3(normX.x, normY.x, normZ.x);

    // Transforming The Normal To ModelView-Space
    vec3 vertex_normal = normalize(osg_NormalMatrix * (-normal));

    vec3 vertex_light_position = gl_LightSource[0].position.xyz;

    float diffuse_value = dot(vertex_normal, vertex_light_position);
    if(diffuse_value < 0)
      diffuse_value *= -1;

    // trickery to pass values to either geometry or fragment shader.
    @if("hasGeomShader")
        texCoordPass = texCoord;
        diffuseValuePass = diffuse_value;
    @else
        texCoordOut = texCoord;
        diffuseValue = diffuse_value;
    @endif
}
