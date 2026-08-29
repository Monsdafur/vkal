// Vertex input
struct VertexInput {
    [[vk::location(0)]]
    float3 position;

    [[vk::location(1)]]
    float3 color;
}
// Vertex output
struct VertexOutput {
    float4 position : SV_Position;
    float3 color;
}

// Custom structs
struct Uniform {
    float4x4 rotation;
}

// Descriptors
[[vk::binding(0, 0)]]
ConstantBuffer<Uniform> ubo;

// Vertex shader
[shader("vertex")]
VertexOutput vertMain(VertexInput input) {
    VertexOutput output;
    output.position = mul(ubo.rotation, float4(input.position, 1.0));
    output.color = input.color;
    return output;
}

// Fragment shader
[shader("fragment")]
float4 fragMain(VertexOutput input) {
    return float4(input.color, 1.0);
}
