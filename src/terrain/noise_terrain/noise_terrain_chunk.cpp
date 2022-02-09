#include "noise_terrain_chunk.h"
#include "../../asset/asset_manager.h"
#include "../../rendering/shader_manager.h"
#include "../terrain_shader.h"
#include "../../rendering/shaders/lighting_shader.h"
#include "../../rendering/environment.h"
#include "../../util/random/worley_noise.h"

#include <noise/noise.h>
#include <noise/module/ridgedmulti.h>
//using namespace noise;

#define MOUNTAIN_SCALE_WIDTH 0.017
#define MOUNTAIN_SCALE_LENGTH 0.017
#define MOUNTAIN_SCALE_HEIGHT 80.0

#define ROUGH_SCALE_WIDTH 0.8
#define ROUGH_SCALE_LENGTH 0.8
#define ROUGH_SCALE_HEIGHT 1.3

#define SMOOTH_SCALE_WIDTH 0.08
#define SMOOTH_SCALE_LENGTH 0.08
#define SMOOTH_SCALE_HEIGHT 1.0

#define MASK_SCALE_WIDTH 0.02
#define MASK_SCALE_LENGTH 0.02

namespace hyperion {

std::vector<double> NoiseTerrainChunk::GenerateHeights(int seed, const ChunkInfo &chunk_info)
{
    std::vector<double> heights;

    WorleyNoise worley(seed);

    SimplexNoiseData data = CreateSimplexNoise(seed);
    SimplexNoiseData biome_data = CreateSimplexNoise(seed + 1);

    heights.resize(chunk_info.m_width * chunk_info.m_length);

    for (int z = 0; z < chunk_info.m_length; z++) {
        for (int x = 0; x < chunk_info.m_width; x++) {
            const double x_offset = x + (chunk_info.m_position.x * (chunk_info.m_width - 1));
            const double z_offset = z + (chunk_info.m_position.y * (chunk_info.m_length - 1));

            const double biome_height = (GetSimplexNoise(&biome_data, x_offset * 0.6, z_offset * 0.6) + 1) * 0.5;

            const double height = (GetSimplexNoise(&data, x_offset, z_offset)) * 30 - 30;

            const double mountain = ((worley.Noise((double)x_offset * MOUNTAIN_SCALE_WIDTH, (double)z_offset * MOUNTAIN_SCALE_LENGTH, 0))) * MOUNTAIN_SCALE_HEIGHT;

            const size_t index = ((x + chunk_info.m_width) % chunk_info.m_width) + ((z + chunk_info.m_length) % chunk_info.m_length) * chunk_info.m_width;

            heights[index] = MathUtil::Lerp(height, mountain, MathUtil::Clamp(biome_height, 0.0, 1.0));
        }
    }

    FreeSimplexNoise(&biome_data);
    FreeSimplexNoise(&data);

    return heights;
}

NoiseTerrainChunk::NoiseTerrainChunk(const std::vector<double> &heights, const ChunkInfo &chunk_info)
    : TerrainChunk(chunk_info),
    m_heights(heights)
{
}

void NoiseTerrainChunk::OnAdded()
{
    std::shared_ptr<Mesh> mesh = BuildMesh(m_heights);

    mesh->SetShader(ShaderManager::GetInstance()->GetShader<TerrainShader>(ShaderProperties()
        .Define("NORMAL_MAPPING", true)
        .Define("PARALLAX_MAPPING", true)
        .Define("ROUGHNESS_MAPPING", true)
        .Define("METALNESS_MAPPING", true)
        .Define("TERRAIN_BIOME_MAP", true)
    ));

    SetRenderable(mesh);

    GetMaterial().SetParameter(MATERIAL_PARAMETER_METALNESS, 0.5f);
    GetMaterial().SetParameter(MATERIAL_PARAMETER_ROUGHNESS, 0.9f);
    GetMaterial().diffuse_color = { 1.0, 1.0, 1.0, 1.0 };
    GetMaterial().cull_faces = MaterialFace_None;

    // GetMaterial().SetTexture("BaseTerrainColorMap", AssetManager::GetInstance()->LoadFromFile<Texture>("textures/snow2/rock-snow-ice1-2k_Base_Color.png"));
    // GetMaterial().SetTexture("BaseTerrainNormalMap", AssetManager::GetInstance()->LoadFromFile<Texture>("textures/snow2/rock-snow-ice1-2k_Normal-ogl.png"));
    // GetMaterial().SetTexture("BaseTerrainParallaxMap", AssetManager::GetInstance()->LoadFromFile<Texture>("textures/snow2/rock-snow-ice1-2k_Height.png"));
    // GetMaterial().SetTexture("BaseTerrainAoMap", AssetManager::GetInstance()->LoadFromFile<Texture>("textures/snow2/rock-snow-ice1-2k_Ambient_Occlusion.png"));

    // GetMaterial().SetTexture("SlopeColorMap", AssetManager::GetInstance()->LoadFromFile<Texture>("textures/dirtwithrocks-ogl/dirtwithrocks_Base_Color.png"));
    // GetMaterial().SetTexture("SlopeNormalMap", AssetManager::GetInstance()->LoadFromFile<Texture>("textures/dirtwithrocks-ogl/dirtwithrocks_Normal-ogl.png"));
    // GetMaterial().SetTexture("SlopeParallaxMap", AssetManager::GetInstance()->LoadFromFile<Texture>("textures/dirtwithrocks-ogl/dirtwithrocks_Height.png"));
    // GetMaterial().SetTexture("SlopeAoMap", AssetManager::GetInstance()->LoadFromFile<Texture>("textures/dirtwithrocks-ogl/dirtwithrocks_AmbientOcculusion.png"));

    GetMaterial().SetTexture("BaseTerrainColorMap", AssetManager::GetInstance()->LoadFromFile<Texture>("textures/grass3.jpg"));
    GetMaterial().SetTexture("BaseTerrainNormalMap", AssetManager::GetInstance()->LoadFromFile<Texture>("textures/grass_nrm.jpg"));

    // GetMaterial().SetTexture("Level1ColorMap", AssetManager::GetInstance()->LoadFromFile<Texture>("textures/snow2/rock-snow-ice1-2k_Base_Color.png"));
    // GetMaterial().SetTexture("Level1NormalMap", AssetManager::GetInstance()->LoadFromFile<Texture>("textures/snow2/rock-snow-ice1-2k_Normal-ogl.png"));
    GetMaterial().SetParameter(MATERIAL_PARAMETER_TERRAIN_LEVEL_0_HEIGHT, 80.0f);
}

SimplexNoiseData NoiseTerrainChunk::CreateSimplexNoise(int seed)
{
    SimplexNoiseData data;

    for (int i = 0; i < OSN_OCTAVE_COUNT; i++) {
        open_simplex_noise(seed, &data.octaves[i]);
        data.frequencies[i] = pow(2.0, double(i));
        data.amplitudes[i] = pow(0.5, OSN_OCTAVE_COUNT - i);
    }

    return data;
}

void NoiseTerrainChunk::FreeSimplexNoise(SimplexNoiseData *data)
{
    for (int i = 0; i < OSN_OCTAVE_COUNT; i++) {
        open_simplex_noise_free(data->octaves[i]);
    }
}

double NoiseTerrainChunk::GetSimplexNoise(SimplexNoiseData *data, int x, int z)
{
    double result = 0.0;

    for (int i = 0; i < OSN_OCTAVE_COUNT; i++) {
        result += open_simplex_noise2(data->octaves[i], x / data->frequencies[i], z / data->frequencies[i]) * data->amplitudes[i];
    }

    return result;
}

} // namespace hyperion
