/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <RenderingPch.hpp>

#include <rendering/CrashHandler.hpp>

#include <Core/debug/Debug.hpp>

#include <Core/threading/Threads.hpp>

#include <Core/io/ByteWriter.hpp>

#include <system/MessageBox.hpp>

#if defined(HYP_AFTERMATH) && HYP_AFTERMATH
#include <Aftermath/GFSDK_Aftermath.h>
#include <Aftermath/GFSDK_Aftermath_GpuCrashDump.h>
#include <Aftermath/GFSDK_Aftermath_GpuCrashDumpDecoding.h>
#include <Aftermath/GFSDK_Aftermath_Defines.h>

#include <chrono>
#include <thread>
#include <iomanip>
#endif

namespace Hyperion {

namespace CoreApi {
extern FilePath GetExecutablePath();
} // namespace CoreApi

HYP_DECLARE_LOG_CHANNEL(Rendering);

thread_local Array<FilePath>* g_savedDumpFiles = nullptr;
static Mutex g_savedDumpFilesPerThreadMutex;
static Array<Array<FilePath>*> g_savedDumpFilesPerThread {};

CrashHandler::CrashHandler()
    : m_isInitialized(false)
{
}

CrashHandler::~CrashHandler()
{
    Mutex::Guard guard(g_savedDumpFilesPerThreadMutex);

    for (Array<FilePath>* savedDumpFiles : g_savedDumpFilesPerThread)
    {
        delete savedDumpFiles;
    }

    g_savedDumpFilesPerThread.Clear();
}

void CrashHandler::Initialize()
{
    if (m_isInitialized)
    {
        return;
    }

    m_isInitialized = true;

#if defined(HYP_AFTERMATH) && HYP_AFTERMATH
    auto res = GFSDK_Aftermath_EnableGpuCrashDumps(
        GFSDK_Aftermath_Version_API,
#if defined(HYP_VULKAN)
        GFSDK_Aftermath_GpuCrashDumpWatchedApiFlags_Vulkan,
#elif defined(HYP_DX12)
        GFSDK_Aftermath_GpuCrashDumpWatchedApiFlags_DX,
#endif
        GFSDK_Aftermath_GpuCrashDumpFeatureFlags_DeferDebugInfoCallbacks,
        [](const void* dump, const uint32 size, void*)
        {
            GFSDK_Aftermath_GpuCrashDump_Decoder decoder = {};
            Assert(GFSDK_Aftermath_GpuCrashDump_CreateDecoder(
                       GFSDK_Aftermath_Version_API,
                       dump,
                       size,
                       &decoder)
                == GFSDK_Aftermath_Result_Success);

            // Query GPU page fault information.
            GFSDK_Aftermath_GpuCrashDump_PageFaultInfo faultInfo = {};
            GFSDK_Aftermath_Result result = GFSDK_Aftermath_GpuCrashDump_GetPageFaultInfo(decoder, &faultInfo);

            if (GFSDK_Aftermath_SUCCEED(result) && result != GFSDK_Aftermath_Result_NotAvailable)
            {
                // Print information about the GPU page fault.
                HYP_LOG(Rendering, Error, "GPU page fault at {}", faultInfo.faultingGpuVA);
                HYP_LOG(Rendering, Error, "Fault Type: {}", faultInfo.faultType);
                HYP_LOG(Rendering, Error, "Access Type: {}", faultInfo.accessType);
                HYP_LOG(Rendering, Error, "Engine: {}", faultInfo.engine);
                HYP_LOG(Rendering, Error, "Client: {}", faultInfo.client);
                if (faultInfo.bHasResourceInfo)
                {
                    HYP_LOG(Rendering, Error, "Fault in resource starting at {}", faultInfo.resourceInfo.gpuVa);
                    HYP_LOG(Rendering, Error, "Size of resource: (w x h x d x ml) = ({}, {}, {}, {}) = {} bytes",
                        faultInfo.resourceInfo.width,
                        faultInfo.resourceInfo.height,
                        faultInfo.resourceInfo.depth,
                        faultInfo.resourceInfo.mipLevels,
                        faultInfo.resourceInfo.size);
                    HYP_LOG(Rendering, Error, "Format of resource: {}", faultInfo.resourceInfo.format);
                    HYP_LOG(Rendering, Error, "Resource was destroyed: {}", faultInfo.resourceInfo.bWasDestroyed);
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
                        for (const GFSDK_Aftermath_GpuCrashDump_GpuInfo& info : infos)
                        {

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

            if (size > 0)
            {
                std::vector<char> bytes;
                bytes.resize(size);

                Memory::Copy(bytes.data(), dump, size);

                FileByteWriter writer(CoreApi::GetExecutablePath() / "dump.nv-gpudmp");
                writer.Write(bytes.data(), bytes.size());
                writer.Close();

                if (!g_savedDumpFiles)
                {
                    g_savedDumpFiles = new Array<FilePath>();

                    Mutex::Guard guard(g_savedDumpFilesPerThreadMutex);
                    g_savedDumpFilesPerThread.PushBack(g_savedDumpFiles);
                }

                g_savedDumpFiles->PushBack(writer.GetFilePath());
            }
        },
        [](const void* info, const uint32 size, void*)
        {
            GFSDK_Aftermath_ShaderDebugInfoIdentifier identifier = {};
            Assert(GFSDK_Aftermath_GetShaderDebugInfoIdentifier(GFSDK_Aftermath_Version_API, info, size, &identifier) == GFSDK_Aftermath_Result_Success);

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

            Memory::Copy(bytes.data(), info, size);

            FileByteWriter writer(CoreApi::GetExecutablePath() / HYP_FORMAT("shader-{}.nvdbg", str.c_str()));
            writer.Write(bytes.data(), bytes.size());
            writer.Close();

            if (!g_savedDumpFiles)
            {
                g_savedDumpFiles = new Array<FilePath>();

                Mutex::Guard guard(g_savedDumpFilesPerThreadMutex);
                g_savedDumpFilesPerThread.PushBack(g_savedDumpFiles);
            }

            g_savedDumpFiles->PushBack(writer.GetFilePath());
        },
        [](PFN_GFSDK_Aftermath_AddGpuCrashDumpDescription, void*)
        {
        },
        [](const void*, void*, void**, uint32*)
        {
        },
        nullptr);

    Assert(res == GFSDK_Aftermath_Result_Success);
#endif
}

void CrashHandler::Dump()
{
#if defined(HYP_AFTERMATH) && HYP_AFTERMATH

    GFSDK_Aftermath_CrashDump_Status status = GFSDK_Aftermath_CrashDump_Status_Unknown;
    Assert(GFSDK_Aftermath_GetCrashDumpStatus(&status) == GFSDK_Aftermath_Result_Success);

    const auto start = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::milliseconds::zero();

    while (status != GFSDK_Aftermath_CrashDump_Status_CollectingDataFailed && status != GFSDK_Aftermath_CrashDump_Status_Finished && elapsed.count() < 30000)
    {
        ThreadSleep(30);

        Assert(GFSDK_Aftermath_GetCrashDumpStatus(&status) == GFSDK_Aftermath_Result_Success);

        elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
    }
#endif

    Mutex::Guard guard(g_savedDumpFilesPerThreadMutex);

    const String message = String("A GPU crash has been detected. The application will now exit.")
        + (g_savedDumpFilesPerThread.Any()
                ? HYP_FORMAT("\nCrash dump(s) has been saved to: {}\n\nPlease attach these when submitting a bug report.",
                      String::Join(g_savedDumpFilesPerThread, '\n', [](const Array<FilePath>* item)
                          {
                              return item ? String::Join(*item, '\n') : String();
                          }))
                : "\nCrash dump state is unknown.");

    HYP_LOG(Rendering, Fatal, "GPU Crash Detected!\n{}", message);
}

} // namespace Hyperion
