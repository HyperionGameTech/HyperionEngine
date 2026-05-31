/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <HyperionPch.hpp>

#include <Framework/DeviceDetails.hpp>

#include <DeviceDetails.generated.inl>

namespace Hyperion {

DeviceDetails::DeviceDetails()
{
}

DeviceDetails::~DeviceDetails()
{
}

GpuType DeviceDetails::GetGpuType() const
{
    return info.gpuType;
}

GpuVendor DeviceDetails::GetGpuVendor() const
{
    return info.gpuVendor;
}

const String& DeviceDetails::GetGpuModel() const
{
    return info.gpuModel;
}

const String& DeviceDetails::GetGpuVendorName() const
{
    return m_gpuVendorName;
}

const String& DeviceDetails::GetDriverVersion() const
{
    return info.driverVersion;
}

bool DeviceDetails::IsDiscreteGpu() const
{
    return info.isDiscrete;
}

bool DeviceDetails::SupportsRayTracing() const
{
    return info.supportsRayTracing;
}

bool DeviceDetails::SupportsRayQueries() const
{
    return info.supportsRayQueries;
}

bool DeviceDetails::SupportsVariableRateShading() const
{
    return info.supportsVariableRateShading;
}

bool DeviceDetails::SupportsSamplerFeedback() const
{
    return info.supportsSamplerFeedback;
}

bool DeviceDetails::SupportsBindless() const
{
    return info.supportsBindless;
}

static GpuVendor GetVendorFromVendorId(uint32 vendorId)
{
    switch (vendorId)
    {
    case 0x10DE: return GpuVendor::Nvidia;
    case 0x1002: return GpuVendor::Amd;
    case 0x8086: return GpuVendor::Intel;
    case 0x5143: return GpuVendor::Qualcomm;
    case 0x106B: return GpuVendor::Apple;
    default: return GpuVendor::Unknown;
    }
}

static const char* GetVendorNameFromEnum(GpuVendor vendor)
{
    switch (vendor)
    {
    case GpuVendor::Nvidia: return "NVIDIA";
    case GpuVendor::Amd: return "AMD";
    case GpuVendor::Intel: return "Intel";
    case GpuVendor::Qualcomm: return "Qualcomm";
    case GpuVendor::Apple: return "Apple";
    case GpuVendor::Microsoft: return "Microsoft";
    case GpuVendor::Unknown:
    default: return "Unknown";
    }
}

void DeviceDetails::Set(const GpuInfo& gpuInfo)
{
    info = gpuInfo;

    if (info.gpuVendor == GpuVendor::Unknown && info.vendorId != 0)
    {
        info.gpuVendor = GetVendorFromVendorId(info.vendorId);
    }

    m_gpuVendorName = GetVendorNameFromEnum(info.gpuVendor);
}

} // namespace Hyperion
