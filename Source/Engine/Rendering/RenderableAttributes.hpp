/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Defines.hpp>
#include <Core/Types.hpp>
#include <Core/HashCode.hpp>

#include <Rendering/Shared.hpp>
#include <Rendering/RenderBucket.hpp>

#include <Core/Utilities/EnumFlags.hpp>

#include <Core/Reflection/ObjectFwd.hpp>

namespace Hyperion {

enum class ShaderCacheId : uint64;

HYP_ENUM()
enum MaterialAttributeFlags : uint8
{
    MAF_NONE = 0x0,             //!< @editor=false
    MAF_DEPTH_WRITE = 0x1,      //!< @title="Depth Write" @description="Enable depth write for objects with this material"
    MAF_DEPTH_TEST = 0x2,       //!< @title="Depth Test" @description="Enable depth testing for objects with this material"
    MAF_DEPTH_BIAS = 0x4,       //!< @title="Depth Bias" @description="Enable depth bias settings"
    MAF_DEPTH_CLAMP = 0x8,      //!< @title="Depth Clamp" @description="Depth clamp enablement - objects with depth outside of the 0..1 range will be clamped at those values respectively, rather than clipped."
    MAF_STENCIL_TEST = 0x10,    //!< @title="Stencil Test" @description="Enable objects with this material to be used in stencil test"
    MAF_ALPHA_DISCARD = 0x20    //!< @title="Has Alpha Discard" @description="Objects with this material will have pixels culled, where they have an opacity below a specified threshold (set on the Material object)"
};

HYP_MAKE_ENUM_FLAGS(MaterialAttributeFlags);

#pragma pack(push, 8)

HYP_STRUCT()
struct MaterialAttributes final
{
    HYP_STRUCT_BODY(MaterialAttributes);

    HYP_FIELD(Property = "ShaderName", Serialize, Editor)
    Name shaderName;

    HYP_FIELD(Property = "ShaderProperties", Serialize, Editor)
    ShaderPropertySet shaderProperties;

    HYP_FIELD(Property = "Bucket", Serialize, Editor)
    RenderBucket bucket = RenderBucket::Opaque;

    HYP_FIELD(Property = "FillMode", Serialize, Editor)
    FillMode fillMode = FM_FILL;

    HYP_FIELD(Property = "BlendFunction", Serialize, Editor)
    BlendFunction blendFunction = BlendFunction::None();

    HYP_FIELD(Property = "FaceCullMode", Serialize, Editor)
    FaceCullMode cullFaces = FCM_BACK;

    HYP_FIELD(Property = "Flags", Serialize, Editor)
    EnumFlags<MaterialAttributeFlags> flags = MAF_DEPTH_WRITE | MAF_DEPTH_TEST;

    HYP_FIELD(Property = "StencilFunction", Serialize, Editor)
    StencilFunction stencilFunction;

    HYP_FIELD(Property = "DepthCompareOp", Serialize, Editor)
    DepthCompareOp depthCompareOp = DCO_LESS;

    HYP_FIELD(Property = "StencilReference", Serialize, Editor)
    uint8 stencilReference = 0;

    uint16 padding = 0;

    HYP_FIELD(Property = "DepthBias", Serialize, Editor)
    int32 depthBias = 0;

    HYP_FIELD(Property = "DepthBiasSlope", Serialize, Editor)
    float depthBiasSlope = 0.0f;

    HYP_FORCE_INLINE bool operator==(const MaterialAttributes& other) const
    {
        return memcmp(this, &other, sizeof(MaterialAttributes)) == 0;
    }

    HYP_FORCE_INLINE bool operator!=(const MaterialAttributes& other) const
    {
        return !(operator==(other));
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(shaderName);
        hc.Add(shaderProperties);
        hc.Add(bucket);
        hc.Add(fillMode);
        hc.Add(blendFunction);
        hc.Add(cullFaces);
        hc.Add(flags);
        hc.Add(stencilFunction);
        hc.Add(depthCompareOp);
        hc.Add(stencilReference);
        hc.Add(depthBias);
        hc.Add(depthBiasSlope);

        return hc;
    }
};

HYP_STRUCT()
struct MeshAttributes final
{
    HYP_STRUCT_BODY(MeshAttributes);

    HYP_FIELD(Property = "InputLayout")
    VertexInputLayoutDesc inputLayout = { VT_Simple };

    HYP_FIELD(Property = "Topology")
    Topology topology = TOP_TRIANGLES;

    HYP_FIELD(Property = "IndexBufferElemType")
    GpuElemType indexBufferElemType = GET_UNSIGNED_INT;

    uint8 padding = 0;

    HYP_FORCE_INLINE bool operator==(const MeshAttributes& other) const
    {
        return memcmp(this, &other, sizeof(MeshAttributes)) == 0;
    }

    HYP_FORCE_INLINE bool operator!=(const MeshAttributes& other) const
    {
        return memcmp(this, &other, sizeof(MeshAttributes)) != 0;
    }

    HYP_FORCE_INLINE constexpr HashCode GetHashCode() const
    {
        return HashCode::GetHashCode(inputLayout)
            .Combine(topology)
            .Combine(indexBufferElemType);
    }
};

#pragma pack(pop)

/*! \brief Compact 32-bit handle into the RenderGroupCache.
 *  Bits 29-31 embed the RenderBucket (3 bits, 8 possible values >= NumRenderBuckets=5).
 *  Bits 0-28 encode the registry index (29 bits = up to ~500 M unique sets).
 *  All bits set (0xFFFFFFFF) is the invalid sentinel. */
struct RenderableAttributeHandle
{
    static constexpr uint32 InvalidValue = ~0u;

    uint32 value = InvalidValue;

    HYP_FORCE_INLINE static RenderableAttributeHandle Create(uint32 index, RenderBucket bucket)
    {
        AssertDebug(index <= 0x1FFFFFFFu);

        RenderableAttributeHandle handle;
        handle.value = (uint32(bucket) << 29) | index;

        return handle;
    }

    HYP_FORCE_INLINE bool IsValid() const
    {
        return value != InvalidValue;
    }

    /*! \brief Returns the RenderBucket encoded in this handle without a registry lookup. */
    HYP_FORCE_INLINE RenderBucket GetBucket() const
    {
        return RenderBucket(value >> 29);
    }

    HYP_FORCE_INLINE uint32 GetIndex() const
    {
        return value & 0x1FFFFFFFu;
    }

    HYP_FORCE_INLINE bool operator==(const RenderableAttributeHandle& other) const
    {
        return value == other.value;
    }

    HYP_FORCE_INLINE bool operator!=(const RenderableAttributeHandle& other) const
    {
        return value != other.value;
    }

    HYP_FORCE_INLINE bool operator<(const RenderableAttributeHandle& other) const
    {
        return value < other.value;
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        return HashCode::GetHashCode(value);
    }
};

class RenderableAttributeSet final
{
    MeshAttributes m_meshAttributes;
    MaterialAttributes m_materialAttributes;
    uint32 m_layerIndex;

public:
    RenderableAttributeSet(const MeshAttributes& meshAttributes = {}, const MaterialAttributes& materialAttributes = {})
        : m_meshAttributes(meshAttributes),
          m_materialAttributes(materialAttributes),
          m_layerIndex(0)
    {
    }

    RenderableAttributeSet(const RenderableAttributeSet& other) = default;
    RenderableAttributeSet& operator=(const RenderableAttributeSet& other) = default;

    RenderableAttributeSet(RenderableAttributeSet&& other) noexcept = default;
    RenderableAttributeSet& operator=(RenderableAttributeSet&& other) noexcept = default;

    ~RenderableAttributeSet() = default;

    HYP_FORCE_INLINE bool operator==(const RenderableAttributeSet& other) const
    {
        return m_meshAttributes == other.m_meshAttributes
            && m_materialAttributes == other.m_materialAttributes
            && m_layerIndex == other.m_layerIndex;
    }

    HYP_FORCE_INLINE bool operator!=(const RenderableAttributeSet& other) const
    {
        return !(operator==(other));
    }

    HYP_FORCE_INLINE bool operator<(const RenderableAttributeSet& other) const
    {
        return GetHashCode().Value() < other.GetHashCode().Value();
    }

    HYP_FORCE_INLINE Name GetShaderName() const
    {
        return m_materialAttributes.shaderName;
    }

    HYP_FORCE_INLINE void SetShaderName(Name shaderName)
    {
        m_materialAttributes.shaderName = shaderName;
    }

    HYP_FORCE_INLINE ShaderPropertySet& GetShaderProperties()
    {
        return m_materialAttributes.shaderProperties;
    }

    HYP_FORCE_INLINE const ShaderPropertySet& GetShaderProperties() const
    {
        return m_materialAttributes.shaderProperties;
    }

    HYP_FORCE_INLINE void SetShaderProperties(const ShaderPropertySet& shaderProperties)
    {
        m_materialAttributes.shaderProperties = shaderProperties;
    }

    HYP_FORCE_INLINE MeshAttributes& GetMeshAttributes()
    {
        return m_meshAttributes;
    }

    HYP_FORCE_INLINE const MeshAttributes& GetMeshAttributes() const
    {
        return m_meshAttributes;
    }

    HYP_FORCE_INLINE void SetMeshAttributes(const MeshAttributes& meshAttributes)
    {
        m_meshAttributes = meshAttributes;
    }

    HYP_FORCE_INLINE MaterialAttributes& GetMaterialAttributes()
    {
        return m_materialAttributes;
    }

    HYP_FORCE_INLINE const MaterialAttributes& GetMaterialAttributes() const
    {
        return m_materialAttributes;
    }

    HYP_FORCE_INLINE void SetMaterialAttributes(const MaterialAttributes& materialAttributes)
    {
        m_materialAttributes = materialAttributes;
    }

    HYP_FORCE_INLINE uint32 GetLayerIndex() const
    {
        return m_layerIndex;
    }

    HYP_FORCE_INLINE void SetLayerIndex(uint32 layerIndex)
    {
        m_layerIndex = layerIndex;
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(m_meshAttributes.GetHashCode());
        hc.Add(m_materialAttributes.GetHashCode());
        hc.Add(m_layerIndex);
        return hc;
    }
};

} // namespace Hyperion
