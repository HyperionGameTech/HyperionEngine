/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <editor/ui/EditorUI.hpp>

#include <ui/UIDataSource.hpp>

#include <core/object/HypClass.hpp>

#include <core/logging/Logger.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Editor);

HYP_API Handle<UIElementFactoryBase> GetEditorUIElementFactory(TypeId typeId)
{
    Handle<UIElementFactoryBase> factory = UIElementFactoryRegistry::GetInstance().GetFactory(typeId);

    if (!factory)
    {
        if (const HypClass* hypClass = GetClass(typeId))
        {
            factory = UIElementFactoryRegistry::GetInstance().GetFactory(TypeId::ForType<HypData>());
        }

        if (!factory)
        {
            HYP_LOG(Editor, Warning, "No factory registered for TypeId {}", typeId.Value());

            return nullptr;
        }
    }

    return factory;
}

} // namespace hyperion
