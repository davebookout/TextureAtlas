# Texture Atlas Creation for Sprite Rendering
## Goal
The primary goal of the project was allow the drawing of many sprites using many different textures without repeated updates to the bound shader resources. Bindless textures are not available due to platform limitations. Two algorithms for creating a texture atlas, a single texture composed of many smaller textures provided at runtime were implemented. Optimally allocating a texture atlas is an NP-complete problem. Any implementation balances execution-time, accuracy, and algorithmic complexity. See  https://www.codeandweb.com/texturepacker and http://free-tex-packer.com/ for example tools.

## Recommendation
A simple, internal implementation is nice to have for runtime support, ease of experimentation in the engine, and limiting external dependencies. A fully featured implementation may have deminishing returns when offline tools are available exisiting formats could be used to help portability of assets. At runtime, texture arrays provide many of the benefits of bindless textures, e.g. dynamically selecting a texture in the shader, fewer API calls for binding. 

## Results
Two allocation algorithms were developed. The first, a brute-force, constrained algorithm divides the altas into 32x32 blocks (1024 blocks total) for allocation. The blocks are stored as a UINT32 array[32]. Allocating a texture requires a linear search of all the possible allocations until a free one is found. 

Some tests can be performed in parallel and the total number of possiblities for a texture allocation is relatively small; so, the overall time allocating is small compared to the other texture processing required for the atlas creation. For example, using the full 32x32 block-size, a 4x4 block texture would have 32-4 possible starting row offsets and 32-4 possible starting column offsets. Each candidate allocation can be checked with 1 comaprison per row. So, to test all possibilities would take (32-4)\*(32-4)\*4 or about 3000 comparisons. Efficiency of the storage is highly dependent on allocation ordering with best performance when allocating largest to smallest textures. Performance could be improved by tiling and swizzling the texture layouts.

Another, slightly more complicated, algorithm attempts to find the smallest matching allocation. For this, the number of basic allocation blocks is specified at runtime with the implementation limiting to 4096x4096 blocks. Allocations are stored as vector of unordered sets of allocations. Allocations at vector[0] represent the entire texture size. Allocations at vector[1] represent 2048x2048. This continues with each subsequent vector element 1/4 the size of the previous. When allocating a texture, the smallest allocation is chosen. If no matching allocation is available, a larger allocation is subdivied to the required size. Allocation for square textures in log(n) and because the algorithm attempts to find a the smallest acceptable allocation, the storage efficiency is less dependent on order of allocations.

For example, a new allocator is created with 16x16 blocks, and single 16x16 block is added to vector[0]. If a 8x8 allocation is required, vector[1] is checked for available allocations. Because there are no allocations in vector[1], the single allocation at vector[0] is removed, and 4 suballocations are added to vector[1] with one of those allocations then used to satisfy the initial request.

| State | Start | subdivide | Allocate |
|-------|-------|-----------|----------|
|vector[0] (16x16) | X | ~~X~~ | |
|vector[1] (8x8) |     | **X X X X** | X X X  ~~X~~|
|request(8x8) |   |   |  **X**|



## Additional Considerations

### MipMapping
Because the textures are not guaranteed to be the same size, or even power of 2, mip mapping is not straightforward. To avoid possible sampling across textures,  mip maps were generated per texture and then copied in to the atlas mips. In addition, texture allocations were aligned to power of 2 boundaries to avoid texture conflicts or incorrect offsets at lower levels. Also, the full mip chain for the texture is not desirable and should be clamped to the smallest mip of the largest texture or, more conservatively, the smallest mip of the smallest texture. Very small sprites will have large partial derivatives leading to sampling from invalid mips for a given sprite's textures.

### Rendering

Sprites are rendered as an instanced list with no vertex or index buffer. Vertex parameters can be computed from instance parameters. Most importantly, each instance has an index for a buffer containing sample parameters.
```
struct SampleParameters {
	float start[2] = { 1.0, 0.0 };
	float end[2] = { 0.0, 1.0 };
	int index = 0;
	int padding[3];
};
```
Start and end are the [0..1) bounds of the desired texture in the texture atlas and are combined with the passed in UVs for the sample parameters. Index provides the array slice index if using texture arrays.
```
struct PSInput
{
    float2 uv : TEXCOORD0;
    float2 startUV : TEXCOORD1;
    float2 endUV : TEXCOORD2;
    int texID : TEXTURE_ID;
};

float4 PSMain(PSInput input) : SV_TARGET
{
    float3 sampleUV = float3(lerp(input.startUV, input.endUV, input.uv), input.texID);
    float4 result = g_textureAtlas.Sample(g_sampler, sampleUV); 
    return result;
}
 ```
