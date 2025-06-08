/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/CrashHandler.hpp>

#include <core/debug/Debug.hpp>

#include <core/utilities/Format.hpp>

#include <core/logging/Logger.hpp>

#include <core/io/ByteWriter.hpp>

#if defined(HYP_AFTERMATH) && HYP_AFTERMATH
#include <Aftermath/GFSDK_Aftermath.h>
#include <Aftermath/GFSDK_Aftermath_GpuCrashDump.h>
#include <Aftermath/GFSDK_Aftermath_GpuCrashDumpDecoding.h>
#include <Aftermath/GFSDK_Aftermath_Defines.h>

#include <chrono>
#include <thread>
#include <iomanip>
#endif

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Rendering);

CrashHandler::CrashHandler()
    : m_is_initialized(false)
{
}

HYP_DISABLE_OPTIMIZATION;

void CrashHandler::Initialize()
{
    if (m_is_initialized)
    {
        return;
    }

    m_is_initialized = true;

#if defined(HYP_AFTERMATH) && HYP_AFTERMATH
    auto res = GFSDK_Aftermath_EnableGpuCrashDumps(
        GFSDK_Aftermath_Version_API,
        GFSDK_Aftermath_GpuCrashDumpWatchedApiFlags_Vulkan,
        GFSDK_Aftermath_GpuCrashDumpFeatureFlags_DeferDebugInfoCallbacks,
        [](const void* dump, const uint32 size, void*)
        {
            GFSDK_Aftermath_CrashDump_Status status = GFSDK_Aftermath_CrashDump_Status_Unknown;
            AssertThrow(GFSDK_Aftermath_GetCrashDumpStatus(&status) == GFSDK_Aftermath_Result_Success);

            const auto start = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::milliseconds::zero();

            // Loop while Aftermath crash dump data collection has not finished or
            // the application is still processing the crash dump data.
            while (status != GFSDK_Aftermath_CrashDump_Status_CollectingDataFailed && status != GFSDK_Aftermath_CrashDump_Status_Finished && elapsed.count() < 1000)
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
                            &decoder)
                == GFSDK_Aftermath_Result_Success);

            // Query GPU page fault information.
            GFSDK_Aftermath_GpuCrashDump_PageFaultInfo fault_info = {};
            GFSDK_Aftermath_Result result = GFSDK_Aftermath_GpuCrashDump_GetPageFaultInfo(decoder, &fault_info);

            if (GFSDK_Aftermath_SUCCEED(result) && result != GFSDK_Aftermath_Result_NotAvailable)
            {
                // Print information about the GPU page fault.
                HYP_LOG(Rendering, Error, "GPU page fault at {}", fault_info.faultingGpuVA);
                HYP_LOG(Rendering, Error, "Fault Type: {}", fault_info.faultType);
                HYP_LOG(Rendering, Error, "Access Type: {}", fault_info.accessType);
                HYP_LOG(Rendering, Error, "Engine: {}", fault_info.engine);
                HYP_LOG(Rendering, Error, "Client: {}", fault_info.client);
                if (fault_info.bHasResourceInfo)
                {
                    HYP_LOG(Rendering, Error, "Fault in resource starting at {}", fault_info.resourceInfo.gpuVa);
                    HYP_LOG(Rendering, Error, "Size of resource: (w x h x d x ml) = ({}, {}, {}, {}) = {} bytes",
                        fault_info.resourceInfo.width,
                        fault_info.resourceInfo.height,
                        fault_info.resourceInfo.depth,
                        fault_info.resourceInfo.mipLevels,
                        fault_info.resourceInfo.size);
                    HYP_LOG(Rendering, Error, "Format of resource: {}", fault_info.resourceInfo.format);
                    HYP_LOG(Rendering, Error, "Resource was destroyed: {}", fault_info.resourceInfo.bWasDestroyed);
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
                            HYP_BREAKPOINT;
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
                        for (const GFSDK_Aftermath_GpuCrashDump_ShaderInfo& info : infos)
                        {
                            HYP_LOG(Rendering, Error, "Active shader: ShaderHash = {} ShaderInstance = {} Shadertype = {}",
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
        [](const void* info, const uint32 size, void*)
        {
            GFSDK_Aftermath_ShaderDebugInfoIdentifier identifier = {};
            AssertThrow(GFSDK_Aftermath_GetShaderDebugInfoIdentifier(GFSDK_Aftermath_Version_API, info, size, &identifier) == GFSDK_Aftermath_Result_Success);

            std::stringstream ss;
            ss << std::hex << std::setfill('0') << std::setw(2 * sizeof(uint64)) << identifier.id[0];
            ss << "-";
            ss << std::hex << std::setfill('0') << std::setw(2 * sizeof(uint64)) << identifier.id[1];
            std::string str = ss.str();

            std::transform(str.begin(), str.end(), str.begin(), [](char ch)
                {
                    return std::toupper(ch);
                });

            std::vector<char> bytes;
            bytes.resize(size);

            Memory::MemCpy(bytes.data(), info, size);

            FileByteWriter writer(FilePath::Current() / HYP_FORMAT("shader-{}.nvdbg", str.c_str()));
            writer.Write(bytes.data(), bytes.size());
            writer.Close();
        },
        [](PFN_GFSDK_Aftermath_AddGpuCrashDumpDescription, void*)
        {
        },
        [](const void*, void*, void**, uint32*)
        {
        },
        nullptr);

    AssertThrow(res == GFSDK_Aftermath_Result_Success);
#endif
}

HYP_ENABLE_OPTIMIZATION;

void CrashHandler::HandleGPUCrash(RendererResult result)
{
    if (result)
    {
        return;
    }

#if defined(HYP_AFTERMATH) && HYP_AFTERMATH
    GFSDK_Aftermath_CrashDump_Status status = GFSDK_Aftermath_CrashDump_Status_Unknown;
    AssertThrow(GFSDK_Aftermath_GetCrashDumpStatus(&status) == GFSDK_Aftermath_Result_Success);

    const auto start = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::milliseconds::zero();

    // Loop while Aftermath crash dump data collection has not finished or
    // the application is still processing the crash dump data.
    while (status != GFSDK_Aftermath_CrashDump_Status_CollectingDataFailed && status != GFSDK_Aftermath_CrashDump_Status_Finished && elapsed.count() < 1000000)
    {
        // Sleep a couple of milliseconds and poll the status again.
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        AssertThrow(GFSDK_Aftermath_GetCrashDumpStatus(&status) == GFSDK_Aftermath_Result_Success);

        elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
    }
#endif

    HYP_LOG(Rendering, Fatal, "GPU Crash Detected!");
}

} // namespace hyperion