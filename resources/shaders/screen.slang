// Vertex output
struct VertexOutput {
    float4 position : SV_Position;
    float2 uv;
}

// Constant values
const static float4 VERTICES[4] = {
    float4(-1.0, -1.0, 0.0, 1.0),
    float4(-1.0, 1.0, 0.0, 1.0),
    float4(1.0, 1.0, 0.0, 1.0),
    float4(1.0, -1.0, 0.0, 1.0),
};

const static float2 UV[4] = { float2(0.0, 0.0), float2(0.0, 1.0), float2(1.0, 1.0),
                              float2(1.0, 0.0) };

// Descriptors
[[vk::binding(0, 0)]]
Texture2D albedo;

[[vk::binding(1, 0)]]
SamplerState basic_sampler;

// Vertex shader
[shader("vertex")]
VertexOutput vertMain(uint vid: SV_VulkanVertexID) {
    VertexOutput output;
    output.position = VERTICES[vid];
    output.uv = UV[vid];
    return output;
}

// Fragment shader
[shader("fragment")]
float4 fragMain(VertexOutput input) {
    return albedo.Sample(basic_sampler, input.uv);
}
