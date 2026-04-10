#include "RxShader.hpp"

#include "FxEngine.hpp"
#include "RxDescriptorCache.hpp"

#include <Asset/AxPaths.hpp>
#include <Asset/FxShaderCompiler.hpp>
#include <Core/FxFile.hpp>
#include <Core/FxRefUtil.hpp>
#include <Core/FxTypes.hpp>
#include <Core/MemPool/FxMemPool.hpp>
#include <Math/FxMathUtil.hpp>
#include <Renderer/RxGlobals.hpp>
#include <Renderer/RxRenderBackend.hpp>


#define DEBUG_FORCE_OUT_OF_DATE 1

/////////////////////////////////////
// Shader Outline Functions
/////////////////////////////////////


uint32 RxShaderOutline::GetReflectionSize() const
{
    uint32 reflection_header_size = sizeof(uint32);                                // Size of reflection header
    reflection_header_size += sizeof(uint32) * RxShaderUtil::scNumShaderTypes;     // Push constant buffer sizes
    reflection_header_size += sizeof(uint32);                                      // Number of descriptor entries
    reflection_header_size += DescriptorEntryCount * sizeof(RxShaderOutlineEntry); // The descriptor entries data

    return FxMath::AlignValue<4>(reflection_header_size);
}

void RxShaderOutline::Print() const
{
    FxLogInfo("=== Outline Debug ===");
    FxLogInfo("Maximum Sets: {}", scNumSets);
    for (uint32 bucket_index = 0; bucket_index < scNumSets; bucket_index++) {
        const EntryList& list = SetBuckets[bucket_index];
        FxLogInfo("Set {} -> {} entries", bucket_index, list.Size);

        for (uint32 i = 0; i < list.Size; i++) {
            const DescEntry& entry = list[i];
            FxLogInfo("\tType={}, IsDynamic?={}, NameHash={}, Set={}, Binding={}",
                      RxShaderOutlineUtil::GetTypeName(entry.Type), entry.bUseDynamicType, entry.NameHash, entry.Set,
                      entry.Binding);
        }
    }
}


void RxShaderOutline::WriteToBuffer(uint32* buffer) const
{
    *(buffer++) = GetReflectionSize();

    uint32 pc_index = 0;
    // Write the push constant sizes for each shader type
    for (; pc_index < RxShaderUtil::scNumShaderTypes; pc_index++) {
        *(buffer++) = PushConstantSizes[pc_index];
    }

    // Number of descriptor entries across buckets
    *(buffer++) = static_cast<uint32>(DescriptorEntryCount);

    uint8* sd_buffer = reinterpret_cast<uint8*>(buffer);


    for (uint32 bucket_index = 0; bucket_index < SetBuckets.Size; bucket_index++) {
        const EntryList& bucket = SetBuckets[bucket_index];

        if (bucket.IsEmpty()) {
            continue;
        }

        // Write each entry in the bucket
        for (DescEntry& entry : bucket) {
            FxLogInfo("ENTRY HAS SHADER TYPE {}", RxShaderUtil::TypeToName(entry.ShaderType));

            memcpy(sd_buffer, &entry, sizeof(RxShaderOutlineEntry));
            sd_buffer += sizeof(RxShaderOutlineEntry);
        }
    }
}


uint32 RxShaderOutline::ReadFromBuffer(const FxSlice<uint32>& data)
{
    uint32* buffer = data.pData;

    const uint32 size_of_outline = *(buffer++);

    // Read the push constant sizes for each shader type
    for (uint32 pc_index = 0; pc_index < RxShaderUtil::scNumShaderTypes; pc_index++) {
        PushConstantSizes[pc_index] = *(buffer++);
    }

    const uint32 num_ds_entries = *(buffer++);

    uint8* sd_buffer = reinterpret_cast<uint8*>(buffer);

    for (uint32 entry_index = 0; entry_index < num_ds_entries; entry_index++) {
        RxShaderOutlineEntry desc_entry;
        memcpy(&desc_entry, sd_buffer, sizeof(RxShaderOutlineEntry));
        sd_buffer += sizeof(RxShaderOutlineEntry);

        EntryList& entry_list = SetBuckets[desc_entry.Set];
        if (!entry_list.IsInited()) {
            entry_list.InitCapacity(scMaxEntriesPerBucket);
        }

        entry_list.Insert(desc_entry);
    }

    return size_of_outline;
}

/////////////////////////////////////
// Shader Functions
/////////////////////////////////////

RxShaderId RxShader::GenerateShaderId(RxShaderType type, const FxSizedArray<FxShaderMacro>& macros)
{
    FxHash64 hash = FX_HASH64_FNV1A_INIT;

    constexpr FxHash64 cPrefixHashVS = FxHashStr64("VERTSHADER");
    constexpr FxHash64 cPrefixHashFS = FxHashStr64("FRAGSHADER");

    if (type == RxShaderType::eVertex) {
        hash = cPrefixHashVS;
    }
    else if (type == RxShaderType::eFragment) {
        hash = cPrefixHashFS;
    }

    FxLogInfo("Building entry ID with {} macros", macros.Size);

    return FxHashData64(FxSlice<FxShaderMacro>(macros), hash);
}

bool RxShader::PreloadCompiledPrograms(const char* pack_path)
{
    bool did_read = mDataPack.ReadFromFile(pack_path);

    if (!did_read) {
        FxLogInfo("Could not read compiled shader from {}. Recompiling...", pack_path);
        return false;
    }

    mDataPack.ReadAllEntries();

    return true;
}

void RxShader::Load(const char* shader_name)
{
    Name = shader_name;

    FxString program_path = GetProgramPath();
    PreloadCompiledPrograms(program_path.CStr());
}


const FxString RxShader::GetSourcePath() const { return FxString(FxAssetPath(AxPathQuery::eShaders)) + Name + ".hlsl"; }
const FxString RxShader::GetProgramPath() const
{
    FxString compiled_folder = FxString(FxAssetPath(AxPathQuery::eShaders)) + "Spirv/";
    return (compiled_folder + Name + ".spack");
}


FxRef<RxShaderProgram> RxShader::LoadUncachedProgram(RxShaderType shader_type,
                                                     const FxSizedArray<FxShaderMacro>& macros)
{
    FxString source_path = GetSourcePath();
    const char* c_source_path = source_path.CStr();

    FxString program_path = GetProgramPath();


    if (!mDataPack.IsOpen() || mDataPack.Entries.IsEmpty()) {
        mDataPack.ReadFromFile(program_path.CStr());
    }

    // Check if the shader is out of date
    bool is_out_of_date = FxShaderCompiler::IsOutOfDate(c_source_path);
#ifdef DEBUG_FORCE_OUT_OF_DATE
    is_out_of_date = true;
#endif

    if (is_out_of_date) {
        FxLogWarning("Shader {} is out of date! ({} macros)", c_source_path, macros.Size);

        RecompileShader(source_path, program_path, macros);

        // If the shader failed to compile, it will not be written back to the datapack. We can continue using the out
        // of date shader.
    }

    // Generate an ID based on the shader type and macros. This is used for querying for the program in the DataPack.
    FxHash64 program_id = RxShader::GenerateShaderId(shader_type, macros);
    FxLogDebug("Getting program from {} (Id={})", Name, program_id);

    // Create the program
    FxRef<RxShaderProgram> program = FxMakeRef<RxShaderProgram>();
    FxLogInfo("** SETTING program type to {}", RxShaderUtil::TypeToName(shader_type));
    program->ShaderType = shader_type;
    program->pShader = this;

    // Check for the program in the DataPack
    FxDataPackEntry* dp_entry = mDataPack.QuerySection(program_id);

    // If there is no compiled version of the program in the DataPack, compile it and save it to disk.
    if (dp_entry == nullptr) {
        FxLogWarning("Shader was not found in datapack, recompiling...");
        RecompileShader(GetSourcePath(), program_path, macros);

        dp_entry = mDataPack.QuerySection(program_id);
        if (dp_entry == nullptr) {
            FxLogError("Pack entry does not exist after compilation! (Id={})", program_id);
            return program;
        }
    }

    // If there is data available, create the program.
    if (dp_entry->Data.IsNotEmpty()) {
        FxLogInfo("Data is available, creating shader...");
        const FxSizedArray<uint8>& program_data = dp_entry->Data;
        FxAssert(program_data.Size == FxMath::AlignValue<4>(program_data.Size));

        CreateShaderModule(*program, program_data.Size, reinterpret_cast<uint32*>(program_data.pData),
                           program->InternalShader);

        return program;
    }

    // The entry exists in the DataPack but the data has not been loaded yet. Load only that section of the DataPack and
    // create the program.
    FxLogInfo("Loading data pack section for shader...");

    uint32 buffer_size = FxMath::AlignValue<4>(dp_entry->DataSize);
    uint32* buffer = gEnginePool->Alloc<uint32>(buffer_size);
    FxSlice<uint8> buffer_slice = FxMakeSlice<uint8>(reinterpret_cast<uint8*>(buffer), buffer_size);

    mDataPack.ReadSection(dp_entry, buffer_slice);

    CreateShaderModule(*program, buffer_size, buffer, program->InternalShader);

    gEnginePool->Free(buffer);

    return program;
}


FxRef<RxShaderProgram> RxShader::GetProgram(RxShaderType shader_type, const FxSizedArray<FxShaderMacro>& macros)
{
    ProgramCache& cached_type = mCachedTypes[static_cast<uint32>(shader_type)];

    const FxHash64 macro_hash = FxHashData64(FxMakeSlice(macros.pData, macros.GetSizeInBytes()));
    auto program_it = cached_type.Programs.find(macro_hash);

    if (program_it != cached_type.Programs.end()) {
        return program_it->second;
    }

    // Program has not been cached, load it from the data pack or compile it, and save it back to the cache.
    FxRef<RxShaderProgram> program = LoadUncachedProgram(shader_type, macros);
    cached_type.Programs[macro_hash] = program;

    return program;
}

void RxShader::RecompileShader(const FxString& source_path, const FxString& compiled_path,
                               const FxSizedArray<FxShaderMacro>& macros)
{
    // Compile to the shader pack
    FxShaderCompiler::Result compile_result = FxShaderCompiler::Compile(source_path.CStr(), mDataPack, macros);

    if (compile_result != FxShaderCompiler::Result::eFailed) {
        mDataPack.WriteToFile(compiled_path.CStr());
    }
}


void RxShaderProgram::BuildDescriptors()
{
    // FxAssert(ShaderOutline.IsValid());

    // Descriptors.InitCapacity(RxShaderOutline::scNumSets);

    // FxLogWarning("Build Descriptors for shader {}", RxShaderUtil::TypeToName(ShaderType));

    // for (uint32 set_index = 0; set_index < RxShaderOutline::scNumSets; set_index++) {
    //     RxShaderOutline::EntryList& entry_list = ShaderOutline->SetBuckets[set_index];

    //     if (entry_list.IsEmpty()) {
    //         FxLogWarning("Skipping descriptors for {} (no entries found)", RxShaderUtil::TypeToName(ShaderType));
    //         continue;
    //     }

    //     Descriptors.Insert(gDescriptorCache->Register(set_index, ShaderType, entry_list));
    // }

    // FxLogInfo("Added {} descriptors to shader program", Descriptors.Size);
}


void RxShaderProgram::Bind(const RxCommandBuffer& cmd, const RxPipeline& pipeline,
                           const RxShaderBindOptions& bind_options)
{
    // for (const RxShaderDescriptorId& ds_id : Descriptors) {
    //     FxRef<RxDescriptorSet> ds = gDescriptorCache->Request(ds_id);
    //     if (bind_options.bUseOffset && ds->HasDynamicOffsets()) {
    //         ds->BindWithOffset(ds_id.Set, cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline, bind_options.BufferOffset);
    //         continue;
    //     }

    //     ds->Bind(ds_id.Set, cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    // }
}

void RxShaderProgram::Destroy()
{
    if (InternalShader == nullptr) {
        return;
    }

    RxGpuDevice* device = gRenderer->GetDevice();
    vkDestroyShaderModule(device->Device, InternalShader, nullptr);
}


void RxShader::CreateShaderModule(RxShaderProgram& program, uint32 file_size, uint32* raw_data,
                                  VkShaderModule& shader_module)
{
    // Load the reflected data into the shader outline
    program.ShaderOutline = FxRef<RxShaderOutline>::New();
    uint32 reflected_size = 0;
    // uint32 reflected_size = program.ShaderOutline->ReadFromBuffer(FxSlice<uint32>(raw_data, file_size));

    // program.BuildDescriptors();
    program.ShaderOutline.DestroyRef();

    uint32* shader_data = reinterpret_cast<uint32*>(reinterpret_cast<uint8*>(raw_data) + reflected_size);

    const VkShaderModuleCreateInfo create_info {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = file_size - reflected_size,
        .pCode = shader_data,
    };

    RxGpuDevice* device = gRenderer->GetDevice();

    const VkResult status = vkCreateShaderModule(device->Device, &create_info, nullptr, &shader_module);

    if (status != VK_SUCCESS) {
        FxLogError("Could not create Vulkan shader module: {}", RxUtil::ResultToStr(status));
        return;
    }
}
