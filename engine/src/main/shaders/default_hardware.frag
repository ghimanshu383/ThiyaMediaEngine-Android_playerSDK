#version 450
layout(location = 0) out vec4 color;
layout(location = 0) in vec2 uvVec;
layout(location = 1) in flat int computePath;


layout(set = 0, binding = 0) uniform sampler2D hardwareImage;
layout(set = 0, binding = 1) uniform sampler2D[2] lumaChromaImg;
layout(set = 0, binding = 2) uniform sampler2D uiImage;

void main() {
    if (computePath == 0) {
        color = texture(hardwareImage, uvVec);
    } else {
        float Y = texture(lumaChromaImg[0], uvVec).r;
        vec2 uv = texture(lumaChromaImg[1], uvVec).rg;
        float U = uv.r - 0.5;
        float V = uv.g - 0.5;

        float R = Y + 1.402 * V;
        float G = Y - 0.344136 * U - 0.714136 * V;
        float B = Y + 1.772 * U;
        vec3 rgb = clamp(vec3(R, G, B), 0.0, 1.0);

        color = vec4(rgb, 1.0);
    }
    vec4 uiColor = texture(uiImage, uvVec, 0);
    color = vec4(mix(color.rgb, uiColor.rgb, uiColor.a), 1.0);
}