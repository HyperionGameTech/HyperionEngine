/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <EditorPch.hpp>

#include <ArborEditor/ArborShared.hpp>

#include <Asset/AssetRegistry.hpp>

#include <Rendering/Texture.hpp>
#include <Rendering/Util/TextureCompression.hpp>

#include <Core/Math/MathUtil.hpp>

#ifdef HYP_ARBOR
#include <arbor.h>
#endif

#include <mutex>

namespace Hyperion {

Name GetArborTextureAssetName(const UTF8StringView& mapName)
{
    return NAME_FMT("Arbor_{}", mapName);
}

bool ReadArborMap(const UTF8StringView& file, Vec2u& outExtent, ByteBuffer& outRgba)
{
    const UTF8StringView mapName = file.EndsWith(".png")
        ? file.Substr(0, file.Length() - 4)
        : file;

    const Handle<AssetRegistry> registry = GetEngineAssetRegistry();

    if (!registry.IsValid())
    {
        return false;
    }

    const Handle<Texture> texture = TryCast<Texture>(registry->GetAsset(AssetBuckets::Textures, GetArborTextureAssetName(mapName)));

    if (!texture.IsValid())
    {
        return false;
    }

    auto readScope = texture->GetReadScope();

    const TextureDesc& desc = texture->GetTextureDesc();
    const ConstByteView imageData = texture->GetImageData();

    outExtent = Vec2u { desc.extent.x, desc.extent.y };

    const size_t numTexels = size_t(outExtent.x) * size_t(outExtent.y);

    switch (desc.format)
    {
    case TextureFormat::RGBA8:
    case TextureFormat::RGBA8_SRGB:
        if (imageData.Size() < numTexels * 4)
        {
            return false;
        }

        outRgba = ByteBuffer(numTexels * 4, imageData.Data());

        return true;
    default:
        break;
    }

    if (!TextureCompression::DecodeMip(desc, imageData, 0, outRgba) || outRgba.Size() < numTexels * 4)
    {
        return false;
    }

    ubyte* texels = outRgba.Data();

    if (desc.format == TextureFormat::BC4)
    {
        for (size_t index = 0; index < numTexels; index++)
        {
            texels[index * 4 + 1] = texels[index * 4];
            texels[index * 4 + 2] = texels[index * 4];
        }
    }
    else if (desc.format == TextureFormat::BC5)
    {
        for (size_t index = 0; index < numTexels; index++)
        {
            const float x = float(texels[index * 4]) / 127.5f - 1.0f;
            const float y = float(texels[index * 4 + 1]) / 127.5f - 1.0f;
            const float z = MathUtil::Sqrt(MathUtil::Max(0.0f, 1.0f - x * x - y * y));

            texels[index * 4 + 2] = ubyte(MathUtil::Round(z * 127.5f + 127.5f));
        }
    }

    return true;
}

#ifdef HYP_ARBOR

static bool ArborMapReaderCallback(void* user, const char* file, ArborMapSink* sink)
{
    Vec2u extent;
    ByteBuffer rgba;

    if (!ReadArborMap(UTF8StringView(file), extent, rgba))
    {
        return false;
    }

    return arbor_map_sink_put(sink, extent.x, extent.y, rgba.Data());
}

static void EnsureTextureDirectory()
{
    static std::once_flag s_onceFlag;

    std::call_once(s_onceFlag, []()
        {
            arbor_set_map_reader(&ArborMapReaderCallback, nullptr);
        });
}

bool ArborRecipeHandle::IsAvailable()
{
    return true;
}

String ArborRecipeHandle::GetLastError()
{
    return String(arbor_last_error());
}

uint32 ArborRecipeHandle::GetPresetCount(uint32 kind)
{
    return arbor_preset_count(kind);
}

String ArborRecipeHandle::GetPresetName(uint32 kind, uint32 index)
{
    const char* name = arbor_preset_name(kind, index);

    return name ? String(name) : String::empty;
}

ArborRecipeHandle ArborRecipeHandle::FromPreset(uint32 kind, const String& name)
{
    EnsureTextureDirectory();

    return ArborRecipeHandle(arbor_recipe_from_preset(kind, name.Data()));
}

ArborRecipeHandle ArborRecipeHandle::FromRon(uint32 kind, const String& ron)
{
    EnsureTextureDirectory();

    return ArborRecipeHandle(arbor_recipe_from_ron(kind, ron.Data()));
}

ArborRecipeHandle::ArborRecipeHandle(ArborRecipeHandle&& other) noexcept
    : m_recipe(other.m_recipe)
{
    other.m_recipe = nullptr;
}

ArborRecipeHandle& ArborRecipeHandle::operator=(ArborRecipeHandle&& other) noexcept
{
    if (this != &other)
    {
        arbor_recipe_free(m_recipe);

        m_recipe = other.m_recipe;
        other.m_recipe = nullptr;
    }

    return *this;
}

ArborRecipeHandle::~ArborRecipeHandle()
{
    arbor_recipe_free(m_recipe);
}

ArborRecipeHandle ArborRecipeHandle::Clone() const
{
    return ArborRecipeHandle(m_recipe ? arbor_recipe_clone(m_recipe) : nullptr);
}

String ArborRecipeHandle::GetName() const
{
    const char* name = m_recipe ? arbor_recipe_name(m_recipe) : nullptr;

    return name ? String(name) : String::empty;
}

String ArborRecipeHandle::ToRon() const
{
    const char* ron = m_recipe ? arbor_recipe_to_ron(m_recipe) : nullptr;

    return ron ? String(ron) : String::empty;
}

uint64 ArborRecipeHandle::GetSeed() const
{
    return m_recipe ? arbor_get_seed(m_recipe) : 0;
}

void ArborRecipeHandle::SetSeed(uint64 seed)
{
    if (m_recipe)
    {
        arbor_set_seed(m_recipe, seed);
    }
}

uint32 ArborRecipeHandle::GetControlCount() const
{
    return m_recipe ? arbor_control_count(m_recipe) : 0;
}

bool ArborRecipeHandle::GetControl(uint32 index, ArborControlDesc& outDesc) const
{
    ArborControlInfo info {};

    if (!m_recipe || !arbor_control_info(m_recipe, index, &info))
    {
        return false;
    }

    outDesc.id = info.id;
    outDesc.section = info.section;
    outDesc.group = info.group;
    outDesc.label = info.label;
    outDesc.help = info.help;
    outDesc.kind = ArborControlKind(info.kind);
    outDesc.min = info.min;
    outDesc.max = info.max;
    outDesc.log = info.log != 0;
    outDesc.countMin = info.count_min;
    outDesc.countMax = info.count_max;
    outDesc.regrows = info.regrows != 0;

    outDesc.options.Clear();

    for (uint32 option = 0; option < info.option_count; option++)
    {
        const char* text = arbor_control_option(m_recipe, index, option);
        outDesc.options.PushBack(text ? String(text) : String::empty);
    }

    return true;
}

bool ArborRecipeHandle::GetValue(uint32 index, ArborControlValue& outValue) const
{
    ArborValue value {};

    if (!m_recipe || !arbor_control_get(m_recipe, index, &value))
    {
        return false;
    }

    outValue.isRange = value.is_range != 0;
    outValue.lo = value.lo;
    outValue.hi = value.hi;
    outValue.landed = value.landed;
    outValue.integer = value.integer;

    return true;
}

bool ArborRecipeHandle::SetValue(uint32 index, const ArborControlValue& value, bool& outReshaped)
{
    const ArborValue arborValue {
        value.isRange ? 1u : 0u,
        value.lo,
        value.hi,
        value.landed,
        value.integer
    };

    outReshaped = false;

    return m_recipe && arbor_control_set(m_recipe, index, &arborValue, &outReshaped);
}

bool ArborRecipeHandle::GenerateGlb(ByteBuffer& outGlb) const
{
    ArborBytes bytes {};

    if (!m_recipe || !arbor_generate_glb(m_recipe, &bytes))
    {
        return false;
    }

    outGlb = ByteBuffer(bytes.len, bytes.data);
    arbor_bytes_free(&bytes);

    return true;
}

#else

bool ArborRecipeHandle::IsAvailable()
{
    return false;
}

String ArborRecipeHandle::GetLastError()
{
    return "ArborToolkit was built without arbor_ffi. Install Rust, ensure External/arbor submodule is checked out, and rebuild the engine";
}

uint32 ArborRecipeHandle::GetPresetCount(uint32 kind)
{
    return 0;
}

String ArborRecipeHandle::GetPresetName(uint32 kind, uint32 index)
{
    return String::empty;
}

ArborRecipeHandle ArborRecipeHandle::FromPreset(uint32 kind, const String& name)
{
    return ArborRecipeHandle();
}

ArborRecipeHandle ArborRecipeHandle::FromRon(uint32 kind, const String& ron)
{
    return ArborRecipeHandle();
}

ArborRecipeHandle::ArborRecipeHandle(ArborRecipeHandle&& other) noexcept = default;

ArborRecipeHandle& ArborRecipeHandle::operator=(ArborRecipeHandle&& other) noexcept = default;

ArborRecipeHandle::~ArborRecipeHandle() = default;

ArborRecipeHandle ArborRecipeHandle::Clone() const
{
    return ArborRecipeHandle();
}

String ArborRecipeHandle::GetName() const
{
    return String::empty;
}

String ArborRecipeHandle::ToRon() const
{
    return String::empty;
}

uint64 ArborRecipeHandle::GetSeed() const
{
    return 0;
}

void ArborRecipeHandle::SetSeed(uint64 seed)
{
}

uint32 ArborRecipeHandle::GetControlCount() const
{
    return 0;
}

bool ArborRecipeHandle::GetControl(uint32 index, ArborControlDesc& outDesc) const
{
    return false;
}

bool ArborRecipeHandle::GetValue(uint32 index, ArborControlValue& outValue) const
{
    return false;
}

bool ArborRecipeHandle::SetValue(uint32 index, const ArborControlValue& value, bool& outReshaped)
{
    outReshaped = false;

    return false;
}

bool ArborRecipeHandle::GenerateGlb(ByteBuffer& outGlb) const
{
    return false;
}

#endif

} // namespace Hyperion
