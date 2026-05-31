/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Defines.hpp>

#include <Core/Reflection/ObjectBase.hpp>
#include <Core/Reflection/Handle.hpp>

#include <Core/Containers/String.hpp>

namespace Hyperion {

HYP_ENUM()
enum class GpuType : uint8
{
    Unknown = 0,
    Integrated,
    Dedicated
};

HYP_ENUM()
enum class GpuVendor : uint8
{
    Unknown = 0,
    Nvidia,
    Amd,
    Intel,
    Qualcomm,
    Apple,
    Microsoft
};

struct GpuInfo
{
    GpuType gpuType = GpuType::Unknown;
    GpuVendor gpuVendor = GpuVendor::Unknown;
    String gpuModel;
    String driverVersion;
    uint32 deviceId = 0;
    uint32 vendorId = 0;

    bool isDiscrete = false;
    bool supportsRayTracing = false;
    bool supportsRayQueries = false;
    bool supportsVariableRateShading = false;
    bool supportsSamplerFeedback = false;
    bool supportsBindless = false;
};

HYP_CLASS()
class DeviceDetails : public ObjectBase
{
    HYP_OBJECT_BODY(DeviceDetails);

public:
    DeviceDetails();
    ~DeviceDetails();

    HYP_METHOD()
    GpuType GetGpuType() const;

    HYP_METHOD()
    GpuVendor GetGpuVendor() const;

    HYP_METHOD()
    const String& GetGpuModel() const;

    HYP_METHOD()
    const String& GetGpuVendorName() const;

    HYP_METHOD()
    const String& GetDriverVersion() const;

    HYP_METHOD()
    bool IsDiscreteGpu() const;

    HYP_METHOD()
    bool SupportsRayTracing() const;

    HYP_METHOD()
    bool SupportsRayQueries() const;

    HYP_METHOD()
    bool SupportsVariableRateShading() const;

    HYP_METHOD()
    bool SupportsSamplerFeedback() const;

    HYP_METHOD()
    bool SupportsBindless() const;

    void Set(const GpuInfo& info);

    GpuInfo info;

private:
    String m_gpuVendorName;
};

} // namespace Hyperion
