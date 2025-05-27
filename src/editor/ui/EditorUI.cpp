/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <editor/ui/EditorUI.hpp>

#include <ui/UIDataSource.hpp>

#include <core/object/HypClass.hpp>

#include <core/logging/Logger.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Editor);

HYP_API UIElementFactoryBase* GetEditorUIElementFactory(TypeID type_id)
{
    UIElementFactoryBase* factory = UIElementFactoryRegistry::GetInstance().GetFactory(type_id);

    if (!factory)
    {
        if (const HypClass* hyp_class = GetClass(type_id))
        {
            factory = UIElementFactoryRegistry::GetInstance().GetFactory(TypeID::ForType<HypData>());
        }

        if (!factory)
        {
            HYP_LOG(Editor, Warning, "No factory registered for TypeID {}", type_id.Value());

            return nullptr;
        }
    }

    return factory;
}

} // namespace hyperion