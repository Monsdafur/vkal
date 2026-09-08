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
    uint texture_index;
}

// Custom structs
struct Uniform {
    float4x4 projection;
    float4x4 view;
}

struct Sprite {
    float3 position;
    uint pad0;
    float3 scale;
    uint pad1;
    float rotation;
    uint texture_index;
}

// Descriptors
[[vk::binding(0, 0)]]
ConstantBuffer<Uniform> ubo;

[[vk::binding(1, 0)]]
StructuredBuffer<Sprite> sp;

[[vk::binding(2, 0)]]
SamplerState basic_sampler;

[[vk::binding(3, 0)]]
Texture2D textures[];

// Helper functions
float4x4 translate(float3 position) {
    return float4x4(float4(1.0, 0.0, 0.0, position.x), float4(0.0, 1.0, 0.0, position.y),
                    float4(0.0, 0.0, 1.0, position.z), float4(0.0, 0.0, 0.0, 1.0), );
}

float4x4 scale(float3 scale) {
    return float4x4(float4(scale.x, 0.0, 0.0, 0.0), float4(0.0, scale.y, 0.0, 0.0),
                    float4(0.0, 0.0, scale.z, 0.0), float4(0.0, 0.0, 0.0, 1.0), );
}

float4x4 rotate(float angle) {
    float c = cos(angle);
    float s = sin(angle);
    return float4x4(float4(c, -s, 0.0, 0.0), float4(s, c, 0.0, 0.0), float4(0.0, 0.0, 1.0, 0.0),
                    float4(0.0, 0.0, 0.0, 1.0));
}

// Vertex shader
[shader("vertex")]
VertexOutput vertMain(VertexInput input, uint iid: SV_VulkanInstanceID) {
    VertexOutput output;

    float4x4 cam_matrix = mul(ubo.projection, ubo.view);
    float4x4 model =

        mul(mul(translate(sp[iid].position), rotate(sp[iid].rotation)), scale(sp[iid].scale));

    output.position = mul(cam_matrix, mul(model, float4(input.position, 1.0)));
    output.uv = input.uv;
    output.texture_index = sp[iid].texture_index;

    return output;
}

// Fragment shader
[shader("fragment")]
float4 fragMain(VertexOutput input) {
    return textures[input.texture_index].Sample(basic_sampler, input.uv);
}
