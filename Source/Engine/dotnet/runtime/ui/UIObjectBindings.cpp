/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <ui/UIObject.hpp>

#include <Core/reflection/Class.hpp>

using namespace Hyperion;

extern "C"
{

    HYP_EXPORT const char* UIEventHandlerResult_GetMessage(UIEventHandlerResult* result)
    {
        Assert(result != nullptr);

        if (Optional<ANSIStringView> message = result->GetMessage())
        {
            return message->Data();
        }

        return nullptr;
    }

    HYP_EXPORT const char* UIEventHandlerResult_GetFunctionName(UIEventHandlerResult* result)
    {
        Assert(result != nullptr);

        if (Optional<ANSIStringView> functionName = result->GetFunctionName())
        {
            return functionName->Data();
        }

        return nullptr;
    }

    HYP_EXPORT void UIObject_Spawn(UIObject* spawnParent, const Class* cls, Name* name, Vec2i* position, UIObjectSize* size, BoxedValue* outBoxed)
    {
        Assert(spawnParent != nullptr);
        Assert(cls != nullptr);
        Assert(name != nullptr);
        Assert(position != nullptr);
        Assert(size != nullptr);
        Assert(outBoxed != nullptr);

        Handle<UIObject> uiObject = spawnParent->CreateUIObject(cls, *name, *position, *size);
        *outBoxed = BoxedValue(std::move(uiObject));
    }

    HYP_EXPORT int8 UIObject_Find(UIObject* parent, const Class* cls, Name* name, BoxedValue* outBoxed)
    {
        Assert(parent != nullptr);
        Assert(cls != nullptr);
        Assert(name != nullptr);
        Assert(outBoxed != nullptr);

        if (!cls->IsDerivedFrom(UIObject::StaticClass()))
        {
            return false;
        }

        Handle<UIObject> uiObject = parent->FindChildUIObject([cls, name](UIObject* uiObject)
            {
                return uiObject->IsA(cls) && uiObject->GetName() == *name;
            });

        if (!uiObject)
        {
            return false;
        }

        *outBoxed = BoxedValue(std::move(uiObject));

        return true;
    }

} // extern "C"
