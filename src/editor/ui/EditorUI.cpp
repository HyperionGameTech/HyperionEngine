/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <editor/ui/EditorUI.hpp>

#include <ui/UIDataSource.hpp>

#include <core/object/HypClass.hpp>

#include <core/logging/Logger.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Editor);

HYP_API IUIDataSourceElementFactory *GetEditorUIDataSourceElementFactory(TypeID type_id)
{
    IUIDataSourceElementFactory *factory = UIDataSourceElementFactoryRegistry::GetInstance().GetFactory(type_id);
        
    if (!factory) {
        if (const HypClass *hyp_class = GetClass(type_id)) {
            factory = UIDataSourceElementFactoryRegistry::GetInstance().GetFactory(TypeID::ForType<HypData>());
        }

        if (!factory) {
            HYP_LOG(Editor, LogLevel::WARNING, "No factory registered for TypeID {}", type_id.Value());

            return nullptr;
        }
    }

    return factory;
}

} // namespace hyperion