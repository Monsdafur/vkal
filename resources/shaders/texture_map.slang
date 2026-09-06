// Vertex input
struct VertexInput {
    [[vk::location(0)]]
    float3 position;

    [[vk::location(1)]]
    float2 uv;
}
// Vertex output
struct VertexOutput {
    float4 position : SV_Position;
    float2 uv;
}

// Custom structs
struct Uniform {
    float4x4 translation;
    float4x4 rotation;
    float4x4 scale;
    float4x4 projection;
    float4x4 view;
}

// Descriptors
[[vk::binding(0, 0)]]
ConstantBuffer<Uniform> ubo;

[[vk::binding(1, 0)]]
Texture2D albedo;

[[vk::binding(2, 0)]]
SamplerState basic_sampler;

// Vertex shader
[shader("vertex")]
VertexOutput vertMain(VertexInput input) {
    VertexOutput output;

    float4x4 cam_matrix = mul(ubo.projection, ubo.view);
    float4x4 model = mul(mul(ubo.translation, ubo.rotation), ubo.scale);

    output.position = mul(cam_matrix, mul(model, float4(input.position, 1.0)));
    output.uv = input.uv;

    return output;
}

// Fragment shader
[shader("fragment")]
float4 fragMain(VertexOutput input) {
    return albedo.Sample(basic_sampler, input.uv);
}
