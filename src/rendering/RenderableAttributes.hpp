/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>
#include <core/Types.hpp>
#include <core/HashCode.hpp>

#include <rendering/Shared.hpp>
#include <rendering/RenderBucket.hpp>

#include <core/utilities/EnumFlags.hpp>

#include <core/reflection/ObjectFwd.hpp>

namespace Hyperion {

enum class ShaderCacheId : uint64;

HYP_ENUM()
enum MaterialAttributeFlags : uint8
{
    MAF_NONE = 0x0,

    MAF_DEPTH_WRITE = 0x1,
    MAF_DEPTH_TEST = 0x2,
    MAF_STENCIL_TEST = 0x4,
    MAF_ALPHA_DISCARD = 0x8
};

HYP_MAKE_ENUM_FLAGS(MaterialAttributeFlags)

HYP_STRUCT()
struct MaterialAttributes
{
    HYP_STRUCT_BODY(MaterialAttributes);

    HYP_FIELD()
    ShaderDefinition shaderDefinition;

    HYP_FIELD()
    RenderBucket bucket = RB_OPAQUE;

    HYP_FIELD()
    FillMode fillMode = FM_FILL;

    HYP_FIELD()
    BlendFunction blendFunction = BlendFunction::None();

    HYP_FIELD()
    FaceCullMode cullFaces = FCM_BACK;

    HYP_FIELD()
    EnumFlags<MaterialAttributeFlags> flags = MAF_DEPTH_WRITE | MAF_DEPTH_TEST;

    HYP_FIELD()
    StencilFunction stencilFunction;

    HYP_FIELD()
    uint8 stencilReference = 0;

    HYP_FIELD(Transient)
    uint32 textureMask = 0;

    HYP_FORCE_INLINE bool operator==(const MaterialAttributes& other) const
    {
        return shaderDefinition == other.shaderDefinition
            && bucket == other.bucket
            && fillMode == other.fillMode
            && blendFunction == other.blendFunction
            && cullFaces == other.cullFaces
            && flags == other.flags
            && stencilFunction == other.stencilFunction
            && stencilReference == other.stencilReference
            && textureMask == other.textureMask;
    }

    HYP_FORCE_INLINE bool operator!=(const MaterialAttributes& other) const
    {
        return shaderDefinition != other.shaderDefinition
            || bucket != other.bucket
            || fillMode != other.fillMode
            || blendFunction != other.blendFunction
            || cullFaces != other.cullFaces
            || flags != other.flags
            || stencilFunction != other.stencilFunction
            || stencilReference != other.stencilReference
            || textureMask != other.textureMask;
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(shaderDefinition.GetHashCode());
        hc.Add(bucket);
        hc.Add(fillMode);
        hc.Add(blendFunction);
        hc.Add(cullFaces);
        hc.Add(flags);
        hc.Add(stencilFunction);
        hc.Add(stencilReference);
        hc.Add(textureMask);

        return hc;
    }
};

struct RuntimeMaterialAttributes
{
    ShaderCacheId shaderCacheId; // id in cache
    RenderBucket bucket;
    FillMode fillMode;
    FaceCullMode cullFaces;
    EnumFlags<MaterialAttributeFlags> flags;
    uint8 stencilReference;
    StencilFunction stencilFunction;
    BlendFunction blendFunction;
    uint32 textureMask;

    HYP_API RuntimeMaterialAttributes();

    HYP_API explicit RuntimeMaterialAttributes(const MaterialAttributes&);

    HYP_API explicit operator MaterialAttributes() const;

    HYP_FORCE_INLINE bool operator==(const RuntimeMaterialAttributes& other) const
    {
        return shaderCacheId == other.shaderCacheId
            && bucket == other.bucket
            && fillMode == other.fillMode
            && blendFunction == other.blendFunction
            && cullFaces == other.cullFaces
            && flags == other.flags
            && stencilFunction == other.stencilFunction
            && stencilReference == other.stencilReference
            && textureMask == other.textureMask;
    }

    HYP_FORCE_INLINE bool operator!=(const RuntimeMaterialAttributes& other) const
    {
        return shaderCacheId != other.shaderCacheId
            || bucket != other.bucket
            || fillMode != other.fillMode
            || blendFunction != other.blendFunction
            || cullFaces != other.cullFaces
            || flags != other.flags
            || stencilFunction != other.stencilFunction
            || stencilReference != other.stencilReference
            || textureMask != other.textureMask;
    }

    constexpr HashCode GetHashCode() const
    {
        return HashCode::GetHashCode(shaderCacheId)
            .Combine(bucket)
            .Combine(fillMode)
            .Combine(cullFaces)
            .Combine(flags)
            .Combine(stencilReference)
            .Combine(stencilFunction)
            .Combine(blendFunction)
            .Combine(textureMask);
    }
};

HYP_STRUCT()
struct MeshAttributes
{
    HYP_STRUCT_BODY(MeshAttributes);

    HYP_FIELD(Property = "VertexAttributes")
    VertexAttributeSet vertexAttributes = VertexAttributeSet::StaticMeshVertexAttributes;

    HYP_FIELD(Property = "Topology")
    Topology topology = TOP_TRIANGLES;

    HYP_FIELD(Property = "IndexBufferElemType")
    GpuElemType indexBufferElemType = GET_UNSIGNED_INT;

    HYP_FORCE_INLINE bool operator==(const MeshAttributes& other) const
    {
        return vertexAttributes == other.vertexAttributes
            && topology == other.topology
            && indexBufferElemType == other.indexBufferElemType;
    }

    HYP_FORCE_INLINE constexpr HashCode GetHashCode() const
    {
        return HashCode::GetHashCode(vertexAttributes)
            .Combine(topology)
            .Combine(indexBufferElemType);
    }
};

class RenderableAttributeSet
{
    MeshAttributes m_meshAttributes;
    MaterialAttributes m_materialAttributes;
    uint32 m_layerIndex;

    mutable HashCode m_cachedHashCode;
    mutable bool m_needsHashCodeRecalculation;

public:
    RenderableAttributeSet(const MeshAttributes& meshAttributes = {}, const MaterialAttributes& materialAttributes = {})
        : m_meshAttributes(meshAttributes),
          m_materialAttributes(materialAttributes),
          m_layerIndex(0),
          m_needsHashCodeRecalculation(true)
    {
    }

    RenderableAttributeSet(const RenderableAttributeSet& other) = default;
    RenderableAttributeSet& operator=(const RenderableAttributeSet& other) = default;

    RenderableAttributeSet(RenderableAttributeSet&& other) noexcept = default;
    RenderableAttributeSet& operator=(RenderableAttributeSet&& other) noexcept = default;

    ~RenderableAttributeSet() = default;

    HYP_FORCE_INLINE bool operator==(const RenderableAttributeSet& other) const
    {
        return GetHashCode() == other.GetHashCode();
    }

    HYP_FORCE_INLINE bool operator!=(const RenderableAttributeSet& other) const
    {
        return GetHashCode() != other.GetHashCode();
    }

    HYP_FORCE_INLINE bool operator<(const RenderableAttributeSet& other) const
    {
        return GetHashCode().Value() < other.GetHashCode().Value();
    }

    HYP_FORCE_INLINE const ShaderDefinition& GetShaderDefinition() const
    {
        return m_materialAttributes.shaderDefinition;
    }

    HYP_FORCE_INLINE void SetShaderDefinition(const ShaderDefinition& shaderDefinition)
    {
        if (m_materialAttributes.shaderDefinition == shaderDefinition)
        {
            return;
        }

        m_materialAttributes.shaderDefinition = shaderDefinition;
        m_needsHashCodeRecalculation = true;
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
        m_needsHashCodeRecalculation = true;
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
        if (m_materialAttributes == materialAttributes)
        {
            return;
        }

        m_materialAttributes = materialAttributes;
        m_needsHashCodeRecalculation = true;
    }

    HYP_FORCE_INLINE uint32 GetLayerIndex() const
    {
        return m_layerIndex;
    }

    HYP_FORCE_INLINE void SetLayerIndex(uint32 layerIndex)
    {
        if (m_layerIndex == layerIndex)
        {
            return;
        }

        m_layerIndex = layerIndex;
        m_needsHashCodeRecalculation = true;
    }

    void Invalidate()
    {
        m_needsHashCodeRecalculation = true;
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        if (m_needsHashCodeRecalculation)
        {
            RecalculateHashCode();

            m_needsHashCodeRecalculation = false;
        }

        return m_cachedHashCode;
    }

private:
    void RecalculateHashCode() const
    {
        HashCode hc;
        hc.Add(m_meshAttributes.GetHashCode());
        hc.Add(m_materialAttributes.GetHashCode());
        hc.Add(m_layerIndex);

        m_cachedHashCode = hc;
    }
};

} // namespace Hyperion
