#version 450

layout(location = 0) in vec3 pos;
layout(location = 1) in vec2 uv;
layout (location = 0) out vec2 uvVec;
layout (location = 1) out flat int  compute;

layout(push_constant) uniform FrameInfo {
    mat4 rotation;
    vec2 scale;
    int width;
    int height;
    int computePath;
} info;

vec3 tri [3] = { vec3(-1, 0, 0), vec3(0, -1, 0), vec3(1, 0, 0) };
void main() {
    vec2 scale = pos.xy * info.scale.xy;
    gl_Position = info.rotation *   vec4(scale, pos.z, 1.0f);
    uvVec = uv;
    compute = info.computePath;
}