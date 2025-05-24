/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <ui/UIObject.hpp>

#include <core/object/HypClass.hpp>
#include <core/object/HypData.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <Types.hpp>

using namespace hyperion;

extern "C" {

HYP_EXPORT const char *UIEventHandlerResult_GetMessage(UIEventHandlerResult *result)
{
    AssertThrow(result != nullptr);

    if (Optional<ANSIStringView> message = result->GetMessage()) {
        return message->Data();
    }

    return nullptr;
}

HYP_EXPORT const char *UIEventHandlerResult_GetFunctionName(UIEventHandlerResult *result)
{
    AssertThrow(result != nullptr);

    if (Optional<ANSIStringView> function_name = result->GetFunctionName()) {
        return function_name->Data();
    }

    return nullptr;
}

HYP_EXPORT void UIObject_Spawn(UIObject *spawn_parent, const HypClass *hyp_class, Name *name, Vec2i *position, UIObjectSize *size, HypData *out_hyp_data)
{
    AssertThrow(spawn_parent != nullptr);
    AssertThrow(hyp_class != nullptr);
    AssertThrow(name != nullptr);
    AssertThrow(position != nullptr);
    AssertThrow(size != nullptr);
    AssertThrow(out_hyp_data != nullptr);

    RC<UIObject> ui_object = spawn_parent->CreateUIObject(hyp_class, *name, *position, *size);
    *out_hyp_data = HypData(std::move(ui_object));
}

HYP_EXPORT int8 UIObject_Find(UIObject *parent, const HypClass *hyp_class, Name *name, HypData *out_hyp_data)
{
    AssertThrow(parent != nullptr);
    AssertThrow(hyp_class != nullptr);
    AssertThrow(name != nullptr);
    AssertThrow(out_hyp_data != nullptr);

    if (hyp_class != UIObject::Class() && !hyp_class->HasParent(UIObject::Class())) {
        return false;
    }

    RC<UIObject> ui_object = parent->FindChildUIObject([hyp_class, name](UIObject *ui_object)
    {
        return ui_object->IsInstanceOf(hyp_class) && ui_object->GetName() == *name;
    });

    if (!ui_object) {
        return false;
    }

    *out_hyp_data = HypData(std::move(ui_object));

    return true;
}

} // extern "C"