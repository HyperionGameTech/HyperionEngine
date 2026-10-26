/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Reflection/Handle.hpp>

#include <Core/Containers/Array.hpp>
#include <Core/Containers/String.hpp>

#include <Core/Functional/Proc.hpp>

#include <Core/Memory/UniquePtr.hpp>

#include <Core/Threading/Mutex.hpp>

#include <Plugins/PluginContext.hpp>

#include <Editor/EditorMemory.hpp>

namespace Hyperion {

class AssetObject;
class Entity;
class EditorActionBase;
class EditorSubsystem;

class IEntityAssetDropHandler
{
public:
    virtual ~IEntityAssetDropHandler() = default;

    virtual bool AcceptsAsset(const AssetObject* asset) const = 0;
    virtual bool CanApply(const AssetObject* asset, const Entity* entity) const = 0;

    virtual Handle<EditorActionBase> CreateAction(
        EditorSubsystem* subsystem,
        const Handle<AssetObject>& asset,
        const Handle<Entity>& entity) const = 0;
};

class EDITOR_API EntityAssetDropHandlerRegistry
{
public:
    static EntityAssetDropHandlerRegistry& GetInstance();

    EntityAssetDropHandlerRegistry();

    template <class HandlerType>
    uint32 Register()
    {
        return Register(MakeUniqueWithAllocator<HandlerType, EditorAllocator>());
    }

    uint32 Register(UniquePtr<IEntityAssetDropHandler, EditorAllocator>&& handler);
    void Unregister(uint32 id);

    const IEntityAssetDropHandler* FindHandler(const AssetObject* asset) const;

private:
    struct Entry
    {
        uint32 id = 0;
        UniquePtr<IEntityAssetDropHandler, EditorAllocator> handler;
    };

    mutable Mutex m_mutex;
    Array<Entry> m_entries;
    uint32 m_nextId = 1;
};

template <class HandlerType>
void RegisterEntityAssetDropHandler(PluginContext& context)
{
    const uint32 id = EntityAssetDropHandlerRegistry::GetInstance().Register<HandlerType>();

    context.AddCleanup([id]()
        {
            EntityAssetDropHandlerRegistry::GetInstance().Unregister(id);
        });
}

EDITOR_API Handle<EditorActionBase> MakeEntityDropAction(
    EditorSubsystem* subsystem,
    const String& text,
    const Handle<Entity>& entity,
    Proc<void(EditorSubsystem*)>&& apply,
    Proc<void(EditorSubsystem*)>&& revert);

class EDITOR_API EditorEntityAssetDrop final
{
public:
    static bool TargetsEntity(const AssetObject* asset);

    static bool CanApplyToEntity(const AssetObject* asset, const Entity* entity);

    static Handle<EditorActionBase> CreateApplyAction(
        EditorSubsystem* subsystem,
        const Handle<AssetObject>& asset,
        const Handle<Entity>& entity);
};

} // namespace Hyperion
