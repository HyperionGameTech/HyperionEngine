/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <dotnet/core/RefCountedPtrBindings.hpp>

#include <ui/UIObject.hpp>

#include <core/memory/RefCountedPtr.hpp>

#include <math/Vector2.hpp>

#include <Engine.hpp>
#include <Types.hpp>

using namespace hyperion;

extern "C" {

HYP_EXPORT UIObjectType UIObject_GetType(ManagedRefCountedPtr obj)
{
    RC<UIObject> ui_object = MarshalRefCountedPtr<UIObject>(obj);

    if (!ui_object) {
        return UIObjectType::UNKNOWN;
    }

    return ui_object->GetType();
}

HYP_EXPORT void UIObject_GetName(ManagedRefCountedPtr obj, Name *out_name)
{
    RC<UIObject> ui_object = MarshalRefCountedPtr<UIObject>(obj);

    if (!ui_object) {
        *out_name = Name::Invalid();
        return;
    }

    *out_name = ui_object->GetName().GetID();
}

HYP_EXPORT void UIObject_SetName(ManagedRefCountedPtr obj, Name *name)
{
    RC<UIObject> ui_object = MarshalRefCountedPtr<UIObject>(obj);

    if (!ui_object) {
        return;
    }

    ui_object->SetName(*name);
}

HYP_EXPORT void UIObject_GetPosition(ManagedRefCountedPtr obj, Vec2i *position)
{
    RC<UIObject> ui_object = MarshalRefCountedPtr<UIObject>(obj);

    if (!ui_object) {
        return;
    }

    *position = ui_object->GetPosition();
}

HYP_EXPORT void UIObject_SetPosition(ManagedRefCountedPtr obj, Vec2i *position)
{
    RC<UIObject> ui_object = MarshalRefCountedPtr<UIObject>(obj);

    if (!ui_object) {
        return;
    }

    ui_object->SetPosition(*position);
}

HYP_EXPORT void UIObject_GetSize(ManagedRefCountedPtr obj, Vec2i *size)
{
    RC<UIObject> ui_object = MarshalRefCountedPtr<UIObject>(obj);

    if (!ui_object) {
        return;
    }

    *size = ui_object->GetActualSize();
}

HYP_EXPORT void UIObject_SetSize(ManagedRefCountedPtr obj, Vec2i *size)
{
    RC<UIObject> ui_object = MarshalRefCountedPtr<UIObject>(obj);

    if (!ui_object) {
        return;
    }

    ui_object->SetSize(*size);
}

HYP_EXPORT UIObjectAlignment UIObject_GetOriginAlignment(ManagedRefCountedPtr obj)
{
    RC<UIObject> ui_object = MarshalRefCountedPtr<UIObject>(obj);

    if (!ui_object) {
        return UIObjectAlignment::TOP_LEFT;
    }

    return ui_object->GetOriginAlignment();
}

HYP_EXPORT void UIObject_SetOriginAlignment(ManagedRefCountedPtr obj, UIObjectAlignment alignment)
{
    RC<UIObject> ui_object = MarshalRefCountedPtr<UIObject>(obj);

    if (!ui_object) {
        return;
    }

    ui_object->SetOriginAlignment(alignment);
}

HYP_EXPORT UIObjectAlignment UIObject_GetParentAlignment(ManagedRefCountedPtr obj)
{
    RC<UIObject> ui_object = MarshalRefCountedPtr<UIObject>(obj);

    if (!ui_object) {
        return UIObjectAlignment::TOP_LEFT;
    }

    return ui_object->GetParentAlignment();
}

HYP_EXPORT void UIObject_SetParentAlignment(ManagedRefCountedPtr obj, UIObjectAlignment alignment)
{
    RC<UIObject> ui_object = MarshalRefCountedPtr<UIObject>(obj);

    if (!ui_object) {
        return;
    }

    ui_object->SetParentAlignment(alignment);
}

} // extern "C"