/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <HyperionPch.hpp>

#include <Framework/DeviceTier/DeviceScoring.hpp>

#include <Core/Math/MathUtil.hpp>

namespace Hyperion {

namespace {

constexpr uint64 BytesPerGiB = 1024ull * 1024ull * 1024ull;

bool ParseAppleSiliconModel(
    const String& gpuModel,
    uint32& outGeneration,
    uint32& outVariantOffset)
{
    constexpr char Prefix[] = "Apple M";
    constexpr size_t PrefixLength = sizeof(Prefix) - 1;

    if (!gpuModel.StartsWith(Prefix))
    {
        return false;
    }

    const char* data = gpuModel.Data();
    const size_t size = gpuModel.Size();

    size_t index = PrefixLength;
    uint32 generation = 0;

    while (index < size && data[index] >= '0' && data[index] <= '9')
    {
        generation = generation * 10 + uint32(data[index] - '0');
        index++;
    }

    if (generation == 0)
    {
        return false;
    }

    outGeneration = generation;

    // Pro / Max / Ultra roughly double the GPU cores each step over the base chip
    if (gpuModel.Contains(" Ultra"))
    {
        outVariantOffset = 36;
    }
    else if (gpuModel.Contains(" Max"))
    {
        outVariantOffset = 20;
    }
    else if (gpuModel.Contains(" Pro"))
    {
        outVariantOffset = 12;
    }
    else
    {
        outVariantOffset = 0;
    }

    return true;
}

float ComputeAppleBaseScore(const GpuInfo& gpu)
{
    uint32 generation = 0;
    uint32 variantOffset = 0;

    if (!ParseAppleSiliconModel(gpu.gpuModel, generation, variantOffset))
    {
        // A-series ?
        return 25.0f;
    }

    // Bump all past M1
    const uint32 generationOffset = generation <= 1 ? 0 : MathUtil::Min(6 + 2 * (generation - 2), 20u);

    return 34.0f + float(generationOffset) + float(variantOffset);
}

} // namespace

int ComputeDeviceGradeScore(const DeviceFacts& facts)
{
    if (!facts.hasGpu)
    {
        return 0;
    }

    const float vramGiB = float(double(facts.gpu.vramBytes) / double(BytesPerGiB));

    float score = 0.0f;

    if (facts.gpu.gpuVendor == GpuVendor::Apple)
    {
        // unified memory, so the VRAM term is capped like an integrated GPU
        score = ComputeAppleBaseScore(facts.gpu) + MathUtil::Min(vramGiB, 4.0f);
    }
    else if (facts.gpu.isDiscrete)
    {
        score = 40.0f + MathUtil::Min(vramGiB, 16.0f) * 1.5f;
    }
    else
    {
        // integrated GPUs report shared system memory as their VRAM
        score = 5.0f + MathUtil::Min(vramGiB, 4.0f);
    }

    if (facts.logicalCores >= 8)
    {
        score += 6.0f;
    }

    // the OS reports a bit less than the installed RAM, so thresholds sit 1 GiB under nominal
    if (facts.systemMemoryBytes >= 15 * BytesPerGiB)
    {
        score += 4.0f;
    }

    const bool isCpuConstrained = (facts.logicalCores != 0 && facts.logicalCores < 4)
        || (facts.systemMemoryBytes != 0 && facts.systemMemoryBytes < 7 * BytesPerGiB);

    if (isCpuConstrained)
    {
        score = MathUtil::Min(score, float(GradeThreshold::High - 1));
    }

    return int(MathUtil::Clamp(score, 0.0f, 100.0f));
}

} // namespace Hyperion
