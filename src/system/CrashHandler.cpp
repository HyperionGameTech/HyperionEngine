/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <system/CrashHandler.hpp>
#include <system/Debug.hpp>

#if defined(HYP_AFTERMATH) && HYP_AFTERMATH
#include <Aftermath/GFSDK_Aftermath.h>
#include <Aftermath/GFSDK_Aftermath_GpuCrashDump.h>
#include <Aftermath/GFSDK_Aftermath_GpuCrashDumpDecoding.h>
#include <Aftermath/GFSDK_Aftermath_Defines.h>

#include <asset/ByteWriter.hpp>

#include <chrono>
#include <thread>
#include <iomanip>
#endif

namespace hyperion::v2 {

void CrashHandler::Initialize()
{
    if (m_is_initialized) {
        return;
    }

    m_is_initialized = true;

#if defined(HYP_AFTERMATH) && HYP_AFTERMATH
    auto res = GFSDK_Aftermath_EnableGpuCrashDumps(
        GFSDK_Aftermath_Version_API,
        GFSDK_Aftermath_GpuCrashDumpWatchedApiFlags_Vulkan,
        GFSDK_Aftermath_GpuCrashDumpFeatureFlags_DeferDebugInfoCallbacks,
        [](const void *dump, const uint32 size, void *)
        {
            GFSDK_Aftermath_CrashDump_Status status = GFSDK_Aftermath_CrashDump_Status_Unknown;
            AssertThrow(GFSDK_Aftermath_GetCrashDumpStatus(&status) == GFSDK_Aftermath_Result_Success);

            const auto start = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::milliseconds::zero();

            // Loop while Aftermath crash dump data collection has not finished or
            // the application is still processing the crash dump data.
            while (status != GFSDK_Aftermath_CrashDump_Status_CollectingDataFailed &&
                   status != GFSDK_Aftermath_CrashDump_Status_Finished &&
                   elapsed.count() < 1000)
            {
                // Sleep a couple of milliseconds and poll the status again.
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                AssertThrow(GFSDK_Aftermath_GetCrashDumpStatus(&status) == GFSDK_Aftermath_Result_Success);

                elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
            }
            
            GFSDK_Aftermath_GpuCrashDump_Decoder decoder = {};
            AssertThrow(GFSDK_Aftermath_GpuCrashDump_CreateDecoder(
                GFSDK_Aftermath_Version_API,
                dump,
                size,
                &decoder) == GFSDK_Aftermath_Result_Success);

            // Query GPU page fault information.
            GFSDK_Aftermath_GpuCrashDump_PageFaultInfo fault_info = {};
            GFSDK_Aftermath_Result result = GFSDK_Aftermath_GpuCrashDump_GetPageFaultInfo(decoder, &fault_info);

            if (GFSDK_Aftermath_SUCCEED(result) && result != GFSDK_Aftermath_Result_NotAvailable)
            {
                // Print information about the GPU page fault.
                DebugLog(LogType::Error, "GPU page fault at 0x%016llx", fault_info.faultingGpuVA);
                DebugLog(LogType::Error, "Fault Type: %u", fault_info.faultType);
                DebugLog(LogType::Error, "Access Type: %u", fault_info.accessType);
                DebugLog(LogType::Error, "Engine: %u", fault_info.engine);
                DebugLog(LogType::Error, "Client: %u", fault_info.client);
                if (fault_info.bHasResourceInfo)
                {
                    DebugLog(LogType::Error, "Fault in resource starting at 0x%016llx", fault_info.resourceInfo.gpuVa);
                    DebugLog(LogType::Error, "Size of resource: (w x h x d x ml) = {%u, %u, %u, %u} = %llu bytes",
                        fault_info.resourceInfo.width,
                        fault_info.resourceInfo.height,
                        fault_info.resourceInfo.depth,
                        fault_info.resourceInfo.mipLevels,
                        fault_info.resourceInfo.size);
                    DebugLog(LogType::Error, "Format of resource: %u", fault_info.resourceInfo.format);
                    DebugLog(LogType::Error, "Resource was destroyed: %d", fault_info.resourceInfo.bWasDestroyed);
                }
            }

            {
                uint32 count = 0;
                GFSDK_Aftermath_Result result = GFSDK_Aftermath_GpuCrashDump_GetGpuInfoCount(decoder, &count);

                if (GFSDK_Aftermath_SUCCEED(result) && result != GFSDK_Aftermath_Result_NotAvailable)
                {
                    // Allocate buffer for results.
                    std::vector<GFSDK_Aftermath_GpuCrashDump_GpuInfo> infos(count);

                    // Query active shaders information.
                    result = GFSDK_Aftermath_GpuCrashDump_GetGpuInfo(decoder, count, infos.data());

                    if (GFSDK_Aftermath_SUCCEED(result))
                    {
                        // Print information for each active shader
                        for (const GFSDK_Aftermath_GpuCrashDump_GpuInfo& info : infos)
                        {
                            //HYP_BREAKPOINT;
                        }
                    }
                }
            }

            {
                uint32 count = 0;
                GFSDK_Aftermath_Result result = GFSDK_Aftermath_GpuCrashDump_GetActiveShadersInfoCount(decoder, &count);

                if (GFSDK_Aftermath_SUCCEED(result) && result != GFSDK_Aftermath_Result_NotAvailable)
                {
                    // Allocate buffer for results.
                    std::vector<GFSDK_Aftermath_GpuCrashDump_ShaderInfo> infos(count);

                    // Query active shaders information.
                    result = GFSDK_Aftermath_GpuCrashDump_GetActiveShadersInfo(decoder, count, infos.data());

                    if (GFSDK_Aftermath_SUCCEED(result))
                    {
                        // Print information for each active shader
                        for (const GFSDK_Aftermath_GpuCrashDump_ShaderInfo &info : infos)
                        {
                            DebugLog(LogType::Error, "Active shader: ShaderHash = 0x%016llx ShaderInstance = 0x%016llx Shadertype = %u",
                                info.shaderHash,
                                info.shaderInstance,
                                info.shaderType);
                        }
                    }
                }
            }

            std::vector<char> bytes;
            bytes.resize(size);

            Memory::MemCpy(bytes.data(), dump, size);

            FileByteWriter writer("./dump.nv-gpudmp");
            writer.Write(bytes.data(), bytes.size());
            writer.Close();
        },
        [](const void *info, const uint32 size, void *)
        {
            GFSDK_Aftermath_ShaderDebugInfoIdentifier identifier = {};
            AssertThrow(GFSDK_Aftermath_GetShaderDebugInfoIdentifier(GFSDK_Aftermath_Version_API, info, size, &identifier) == GFSDK_Aftermath_Result_Success);

            std::stringstream ss;
            ss << std::hex << std::setfill('0') << std::setw(2 * sizeof(uint64)) << identifier.id[0];
            ss << "-";
            ss << std::hex << std::setfill('0') << std::setw(2 * sizeof(uint64)) << identifier.id[1];
            std::string str = ss.str();

            std::transform(str.begin(), str.end(), str.begin(), [](char ch) {
                return std::toupper(ch);
            });

            std::vector<char> bytes;
            bytes.resize(size);

            Memory::MemCpy(bytes.data(), info, size);

            FileByteWriter writer("shader-" + str + ".nvdbg");
            writer.Write(bytes.data(), bytes.size());
            writer.Close();
        },
        [](PFN_GFSDK_Aftermath_AddGpuCrashDumpDescription, void *)
        {
        },
        [](const void *, void *, void **, uint32 *)
        {
        },
        nullptr
    );

    AssertThrow(res == GFSDK_Aftermath_Result_Success);
#endif
}

void CrashHandler::HandleGPUCrash(Result result)
{
    if (result) {
        return;
    }

    DebugLog(LogType::Error, "GPU Crash Detected!\n");

#if defined(HYP_AFTERMATH) && HYP_AFTERMATH
    GFSDK_Aftermath_CrashDump_Status status = GFSDK_Aftermath_CrashDump_Status_Unknown;
    AssertThrow(GFSDK_Aftermath_GetCrashDumpStatus(&status) == GFSDK_Aftermath_Result_Success);

    const auto start = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::milliseconds::zero();

    // Loop while Aftermath crash dump data collection has not finished or
    // the application is still processing the crash dump data.
    while (status != GFSDK_Aftermath_CrashDump_Status_CollectingDataFailed &&
           status != GFSDK_Aftermath_CrashDump_Status_Finished &&
           elapsed.count() < 1000000)
    {
        // Sleep a couple of milliseconds and poll the status again.
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        AssertThrow(GFSDK_Aftermath_GetCrashDumpStatus(&status) == GFSDK_Aftermath_Result_Success);

        elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
    }

    std::terminate();
#endif
}

} // namespace hyperion::v2