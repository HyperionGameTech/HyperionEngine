/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <HyperionPch.hpp>

#include <Framework/DeviceTier/DeviceScoring.hpp>

#include <Core/Math/MathUtil.hpp>

namespace Hyperion {

static constexpr uint64 BytesPerGiB = 1024ull * 1024ull * 1024ull;

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
        score = 45.0f + MathUtil::Min(vramGiB, 16.0f);
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

    if (facts.gpu.isDiscrete && facts.gpu.supportsRayTracing)
    {
        score += 10.0f;
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
