/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <ui/UIDataSource.hpp>

using namespace Hyperion;

extern "C"
{

    HYP_EXPORT void UIDataSourceBase_Push(UIDataSourceBase* dataSource, const Uuid* uuid, BoxedValue* dataPtr, const Uuid* parentUuid)
    {
        Assert(dataSource != nullptr);
        Assert(uuid != nullptr);
        Assert(dataPtr != nullptr);
        Assert(parentUuid != nullptr);

        dataSource->Push(*uuid, std::move(*dataPtr), *parentUuid);
    }

    HYP_EXPORT void UIDataSource_SetElementFactory(UIDataSource* dataSource, const TypeId* elementTypeId, UIElementFactoryBase* elementFactory)
    {
        Assert(dataSource != nullptr);
        Assert(elementTypeId != nullptr);
        Assert(elementFactory != nullptr);

        dataSource->SetElementFactory(*elementTypeId, MakeStrongRef(elementFactory));
    }

} // extern "C"
