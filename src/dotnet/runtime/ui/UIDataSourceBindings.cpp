/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <ui/UIDataSource.hpp>

#include <core/object/HypClass.hpp>
#include <core/object/HypData.hpp>

#include <Types.hpp>

using namespace hyperion;

extern "C"
{

    HYP_EXPORT void UIDataSourceBase_Push(UIDataSourceBase* data_source, const UUID* uuid, HypData* data_ptr, const UUID* parent_uuid)
    {
        AssertThrow(data_source != nullptr);
        AssertThrow(uuid != nullptr);
        AssertThrow(data_ptr != nullptr);
        AssertThrow(parent_uuid != nullptr);

        data_source->Push(*uuid, std::move(*data_ptr), *parent_uuid);
    }

    HYP_EXPORT void UIDataSource_SetElementTypeIdAndFactory(UIDataSource* data_source, const TypeId* element_type_id, UIElementFactoryBase* element_factory)
    {
        AssertThrow(data_source != nullptr);
        AssertThrow(element_type_id != nullptr);
        AssertThrow(element_factory != nullptr);

        data_source->SetElementTypeIdAndFactory(*element_type_id, element_factory);
    }

} // extern "C"