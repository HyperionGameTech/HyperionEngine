/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Reflection/Handle.hpp>

namespace Hyperion {

class AssetObject;
class Entity;
class EditorActionBase;
class EditorSubsystem;

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
