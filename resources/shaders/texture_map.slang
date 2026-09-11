// Vertex input
struct VertexInput {
    [[vk::location(0)]]
    float3 position;

    [[vk::location(1)]]
    float3 normal;

    [[vk::location(2)]]
    float2 uv;
}
// Vertex output
struct VertexOutput {
    float4 position : SV_Position;
    float2 uv;
    float3 normal;
    uint texture_index;
}

// Custom structs
struct Uniform {
    float4x4 projection;
    float4x4 view;
    float3 light_direction;
    float diffuse;
    float ambient;
}

struct Object {
    float4x4 model;
    float4x4 inv_model;
    uint texture_index;
}

// Descriptors
[[vk::binding(0, 0)]]
ConstantBuffer<Uniform> ubo;

[[vk::binding(1, 0)]]
StructuredBuffer<Object> objs;

[[vk::binding(2, 0)]]
SamplerState basic_sampler;

[[vk::binding(3, 0)]]
Texture2D textures[];

// Vertex shader
[shader("vertex")]
VertexOutput vertMain(VertexInput input, uint iid: SV_VulkanInstanceID) {
    VertexOutput output;

    float4x4 converted_projection = ubo.projection;
    converted_projection[1][1] *= -1.0;
    float4x4 cam_matrix = mul(converted_projection, ubo.view);
    output.position = mul(cam_matrix, mul(objs[iid].model, float4(input.position, 1.0)));
    output.uv = input.uv;
    output.texture_index = objs[iid].texture_index;
    output.normal = mul(float3x3(transpose(objs[iid].inv_model)), input.normal);

    return output;
}

// Fragment shader
[shader("fragment")]
float4 fragMain(VertexOutput input) {
    float4 color = textures[input.texture_index].Sample(basic_sampler, input.uv);
    float lighting = ubo.diffuse * max(dot(-ubo.light_direction, input.normal), ubo.ambient);
    color.xyz = color.xyz * lighting;
    return color;
}
