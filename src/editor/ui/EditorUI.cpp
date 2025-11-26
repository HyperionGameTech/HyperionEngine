/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <editor/ui/EditorUI.hpp>

#include <ui/UIDataSource.hpp>

#include <core/reflection/Class.hpp>
#include <core/reflection/TypeInfo.hpp>

#include <core/logging/Logger.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Editor);

HYP_API Handle<UIElementFactoryBase> GetEditorUIElementFactory(const TypeInfo& typeInfo)
{
    Handle<UIElementFactoryBase> factory = UIElementFactoryRegistry::GetInstance().GetFactory(typeInfo);

    if (!factory)
    {
        if (const Class* cls = typeInfo.GetClass())
        {
            factory = UIElementFactoryRegistry::GetInstance().GetFactory(TypeOf<HypData>());
        }

        if (!factory)
        {
            HYP_LOG(Editor, Warning, "No factory registered for type: {}", typeInfo.name);

            return nullptr;
        }
    }

    return factory;
}

} // namespace hyperion
