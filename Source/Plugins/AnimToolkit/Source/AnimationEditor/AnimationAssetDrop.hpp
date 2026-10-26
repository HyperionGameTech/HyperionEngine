/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Editor/EditorAssetDrop.hpp>

namespace Hyperion {

class AnimationAssetDropHandler final : public IEntityAssetDropHandler
{
public:
    bool AcceptsAsset(const AssetObject* asset) const override;
    bool CanApply(const AssetObject* asset, const Entity* entity) const override;
    Handle<EditorActionBase> CreateAction(EditorSubsystem* subsystem, const Handle<AssetObject>& asset, const Handle<Entity>& entity) const override;
};

class AnimationGraphAssetDropHandler final : public IEntityAssetDropHandler
{
public:
    bool AcceptsAsset(const AssetObject* asset) const override;
    bool CanApply(const AssetObject* asset, const Entity* entity) const override;
    Handle<EditorActionBase> CreateAction(EditorSubsystem* subsystem, const Handle<AssetObject>& asset, const Handle<Entity>& entity) const override;
};

} // namespace Hyperion
