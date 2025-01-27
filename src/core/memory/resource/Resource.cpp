#include <core/memory/resource/Resource.hpp>

#include <core/containers/TypeMap.hpp>

#include <core/threading/Mutex.hpp>

namespace hyperion {

#pragma region Memory Pool

static TypeMap<UniquePtr<IResourceMemoryPool>> g_resource_memory_pools;
static Mutex g_resource_memory_pools_mutex;

HYP_API IResourceMemoryPool *GetOrCreateResourceMemoryPool(TypeID type_id, UniquePtr<IResourceMemoryPool>(*create_fn)(void))
{
    Mutex::Guard guard(g_resource_memory_pools_mutex);

    auto it = g_resource_memory_pools.Find(type_id);

    if (it == g_resource_memory_pools.End()) {
        it = g_resource_memory_pools.Set(type_id, create_fn()).first;
    }

    return it->second.Get();
}

#pragma endregion Memory Pool

class NullResource final : public IResource
{
public:
    NullResource()                              = default;
    NullResource(NullResource &&other) noexcept = default;
    virtual ~NullResource() override            = default;

    virtual Name GetTypeName() const override
    {
        return NAME("NullResource");
    }

    virtual bool IsNull() const override
    {
        return true;
    }

    virtual int Claim() override
    {
        return 0;
    }

    virtual int Unclaim() override
    {
        return 0;
    }

    virtual void WaitForCompletion() override
    {
        // Do nothing
    }

    virtual ResourceMemoryPoolHandle GetPoolHandle() const override
    {
        HYP_NOT_IMPLEMENTED();
    }

    virtual void SetPoolHandle(ResourceMemoryPoolHandle) override
    {
        HYP_NOT_IMPLEMENTED_VOID();
    }
};

HYP_API IResource &GetNullResource()
{
    static NullResource null_resource;

    return null_resource;
}

} // namespace hyperion