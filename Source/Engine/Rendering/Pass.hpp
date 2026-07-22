/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Reflection/ObjectBase.hpp>
#include <Core/Reflection/Handle.hpp>

#include <Core/Config/Config.hpp>

#include <Core/Debug/Debug.hpp>

#include <Core/Utilities/Span.hpp>

#include <Core/Containers/SparsePagedArray.hpp>

#include <Rendering/GraphicsPipelineCache.hpp>
#include <Rendering/RenderSetup.hpp>

namespace Hyperion {

class PassData;
class PassBase;
class EntityBatchAllocatorBase;
class FullScreenPass;

struct PassDataExt
{
    HYP_DEF_POOL_NEW_DELETE(g_renderPool);

    TypeId typeId;

    PassDataExt()
        : typeId(TypeId::Void())
    {
    }

    virtual ~PassDataExt() = default;

    HYP_FORCE_INLINE explicit operator bool() const
    {
        return typeId != TypeId::Void();
    }

    HYP_FORCE_INLINE bool operator!() const
    {
        return !bool(*this);
    }

    template <class OtherPassDataExt>
    HYP_FORCE_INLINE OtherPassDataExt* AsType()
    {
        const TypeId otherTypeId = TypeId::ForType<OtherPassDataExt>();

        if (typeId != otherTypeId)
        {
            return nullptr;
        }

        return reinterpret_cast<OtherPassDataExt*>(this);
    }

    template <class OtherPassDataExt>
    HYP_FORCE_INLINE const OtherPassDataExt* AsType() const
    {
        const TypeId otherTypeId = TypeId::ForType<OtherPassDataExt>();

        if (typeId != otherTypeId)
        {
            return nullptr;
        }

        return reinterpret_cast<const OtherPassDataExt*>(this);
    }

    // Create a new instance of this PassDataExt (caller owns the allocation)
    virtual HYP_NODISCARD PassDataExt* Clone() = 0;

protected:
    explicit PassDataExt(TypeId subtype)
        : typeId(subtype)
    {
    }
};

/*! \brief Data and passes used for rendering a View in the Deferred Renderer. */

HYP_CLASS(NoScriptBindings)
class ENGINE_API PassData : public ObjectBase
{
    HYP_OBJECT_BODY(PassData);

public:
    static Pool* GetAllocator()
    {
        return g_renderPool;
    }

    PassData() = default;

    PassData(PassData&& other) noexcept = default;
    PassData& operator=(PassData&& other) noexcept = default;

    virtual ~PassData();

    WeakHandle<View> view;

    PassDataExt* next = nullptr;
};

class PassBase
{
public:
    HYP_DEF_POOL_NEW_DELETE(g_renderPool);

    using PassDataMap = Map<View*, PassData*, RenderAllocator>;

    virtual ~PassBase();

    virtual void Initialize() = 0;
    virtual void Shutdown() = 0;

    virtual void RenderFrame(Frame* frame, const RenderSetup& renderSetup) = 0;

    virtual void OnFrameEnd(uint32 prevFrameIndex);

protected:
    PassBase();

    virtual PassData* CreateViewPassData(View* view, PassDataExt& ext)
    {
        return nullptr;
    }

    PassData* TryGetViewPassData(View* view);
    PassData* FetchViewPassData(View* view, PassDataExt* ext = nullptr, bool forceNew = false);

private:
    PassDataMap m_viewPassData;
};

} // namespace Hyperion
