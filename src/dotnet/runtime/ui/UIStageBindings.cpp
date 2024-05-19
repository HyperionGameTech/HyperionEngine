/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <ui/UIStage.hpp>
#include <ui/UIButton.hpp>
#include <ui/UIText.hpp>
#include <ui/UIPanel.hpp>
#include <ui/UIImage.hpp>
#include <ui/UITabView.hpp>
#include <ui/UIGrid.hpp>
#include <ui/UIMenuBar.hpp>
#include <ui/UIDockableContainer.hpp>

#include <dotnet/core/RefCountedPtrBindings.hpp>
#include <dotnet/runtime/ManagedHandle.hpp>

#include <Engine.hpp>
#include <Types.hpp>

using namespace hyperion;

extern "C" {

HYP_EXPORT void UIStage_GetScene(ManagedRefCountedPtr obj, ManagedHandle *out_scene)
{
    RC<UIStage> stage = GetRefCountedPtrFromManaged<UIStage>(obj);

    if (!stage || !stage->GetScene()) {
        *out_scene = { };

        return;
    }
    
    *out_scene = CreateManagedHandleFromHandle(Handle<Scene>(stage->GetScene()->GetID()));
}

HYP_EXPORT void UIStage_GetSurfaceSize(ManagedRefCountedPtr obj, Vec2i *out_surface_size)
{
    RC<UIStage> stage = GetRefCountedPtrFromManaged<UIStage>(obj);

    if (!stage) {
        *out_surface_size = Vec2i::Zero();

        return;
    }

    *out_surface_size = stage->GetSurfaceSize();
}

// HYP_EXPORT void UIStage_CreateUIObject(ManagedRefCountedPtr obj, Name *name, Vec2i *position, Vec2i *size, bool attach_to_root, ManagedRefCountedPtr *out_ptr)
// {
//     RC<UIStage> stage = GetRefCountedPtrFromManaged<UIStage>(obj);

//     if (!stage) {
//         *out_ptr = ManagedRefCountedPtr { 0 };
//         return;
//     }

//     RC<UIObject> ui_object = stage->CreateUIObject<UIObject>(*name, *position, UIObjectSize({ size->x, UIObjectSize::PIXEL }, { size->y, UIObjectSize::PIXEL }), attach_to_root);

//     *out_ptr = CreateManagedRefCountedPtr(ui_object);
// }

#define DEF_CREATE_UI_OBJECT(T) \
    HYP_EXPORT void UIStage_CreateUIObject_##T(ManagedRefCountedPtr obj, Name *name, Vec2i *position, Vec2i *size, bool attach_to_root, ManagedRefCountedPtr *out_ptr) \
    { \
        RC<UIStage> stage = GetRefCountedPtrFromManaged<UIStage>(obj); \
        \
        if (!stage) { \
            *out_ptr = ManagedRefCountedPtr { 0 }; \
            return; \
        } \
        \
        RC<T> ui_object = stage->CreateUIObject<T>(*name, *position, UIObjectSize({ size->x, UIObjectSize::PIXEL }, { size->y, UIObjectSize::PIXEL }), attach_to_root); \
        \
        *out_ptr = CreateManagedRefCountedPtr(ui_object); \
    }

DEF_CREATE_UI_OBJECT(UIButton)
DEF_CREATE_UI_OBJECT(UIText)
DEF_CREATE_UI_OBJECT(UIPanel)
DEF_CREATE_UI_OBJECT(UIImage)
DEF_CREATE_UI_OBJECT(UITabView)
DEF_CREATE_UI_OBJECT(UITab)
DEF_CREATE_UI_OBJECT(UIGrid)
DEF_CREATE_UI_OBJECT(UIGridRow)
DEF_CREATE_UI_OBJECT(UIGridColumn)
DEF_CREATE_UI_OBJECT(UIMenuBar)
DEF_CREATE_UI_OBJECT(UIMenuItem)
DEF_CREATE_UI_OBJECT(UIDockableItem)
DEF_CREATE_UI_OBJECT(UIDockableContainer)

#undef DEF_CREATE_UI_OBJECT

} // extern "C"