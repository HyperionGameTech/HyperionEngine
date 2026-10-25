/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <Core/Reflection/Class.hpp>
#include <Core/Reflection/ClassRegistry.hpp>
#include <Core/Reflection/Object.hpp>
#include <Core/Reflection/Method.hpp>
#include <Core/Reflection/Property.hpp>

#include <Core/Logging/LogChannels.hpp>

#include <Core/Utilities/GlobalContext.hpp>
#include <Core/Reflection/TypeInfoFwd.hpp>

#include <Core/Name/Name.hpp>

#include <DotNET/DotNETHost.hpp>
#include <DotNET/Interop/ManagedGuid.hpp>
#include <DotNET/ManagedObject.hpp>
#include <DotNET/ManagedClass.hpp>
#include <DotNET/ManagedProperty.hpp>

#if defined(HYP_DOTNET)
#include <Scripting/ScriptObjectResource.hpp>
#endif

using namespace Hyperion;

template <class MemberType, auto GetMemberFunc>
uint32 Class_GetMembersGeneric(const Class* cls, const MemberType** outMembers)
{
    Assert(cls != nullptr);

    Array<MemberType*> allMembers;

    const Class* curr = cls;

    while (curr != nullptr)
    {
        allMembers.Concat((curr->*GetMemberFunc)());

        curr = curr->GetParent();
    }

    if (!outMembers || allMembers.Empty())
    {
        return uint32(allMembers.Size());
    }

    for (uint32 i = 0; i < allMembers.Size(); i++)
    {
        outMembers[i] = allMembers[i];
    }

    return (uint32)allMembers.Size();
}

#ifdef HYP_DOTNET

struct ManagedPropertyAttributeDesc
{
    const char* name;
    uint32 kind; // 0 = bool, 1 = int, 2 = string
    int32 intValue;
    const char* stringValue;
};

static ScriptObjectResource* GetScriptObjectResourceForTarget(const BoxedValue& target)
{
    const AnyRef targetRef = target.ToRef();
    void* targetPtr = targetRef.GetPointer();

    if (!targetPtr)
    {
        return nullptr;
    }

    const Class* targetClass = targetRef.GetClass();

    if (targetClass && targetClass->IsObjectType())
    {
        return static_cast<ObjectBase*>(targetPtr)->GetScriptObjectResource();
    }

    return static_cast<ScriptObjectResource*>(targetPtr);
}

template <class Func>
static bool WithManagedProperty(const BoxedValue& target, const ANSIString& propertyName, Func&& func)
{
    ScriptObjectResource* scriptObjectResource = GetScriptObjectResourceForTarget(target);

    if (!scriptObjectResource)
    {
        return false;
    }

    ResourceGuard resourceScope = scriptObjectResource->GetReadScope();

    dotnet::ManagedObject* managedObject = scriptObjectResource->GetManagedObject();

    if (!managedObject || !managedObject->IsValid() || !managedObject->GetClass())
    {
        return false;
    }

    dotnet::ManagedProperty* managedProperty = managedObject->GetClass()->GetProperty(propertyName.Data());

    if (!managedProperty)
    {
        return false;
    }

    return func(managedObject, managedProperty);
}

#endif

extern "C"
{

#pragma region Class

    HYP_EXPORT const Class* Class_GetClassByName(const char* name)
    {
        if (!name)
        {
            return nullptr;
        }

        const StringHash stringHash(name);

        return ClassRegistry::GetInstance().GetClass(stringHash);
    }

    HYP_EXPORT const Class* Class_GetClassByTypeId(const TypeId* typeId)
    {
        if (!typeId)
        {
            return nullptr;
        }

        return ClassRegistry::GetInstance().GetClass(*typeId);
    }

    HYP_EXPORT const Class* Class_GetClassForManagedClass(const dotnet::ManagedClass* managedClass)
    {
        if (!managedClass)
        {
            return nullptr;
        }

        return managedClass->GetClass();
    }

    HYP_EXPORT const Class* Class_GetClassByTypeHash(dotnet::Assembly* assembly, int64 typeHash)
    {
        Assert(assembly != nullptr);

        SharedPtr<dotnet::ManagedClass> managedClass = assembly->FindClassByTypeHash(typeHash);

        if (!managedClass)
        {
            return nullptr;
        }

        const Class* cls = managedClass->GetClass();
        AssertDebug(cls != nullptr);

        return cls;
    }

    HYP_EXPORT void Class_GetName(const Class* cls, Name* outName)
    {
        if (!cls || !outName)
        {
            return;
        }

        *outName = cls->GetName();
    }

    HYP_EXPORT void Class_GetTypeId(const Class* cls, TypeId* outTypeId)
    {
        if (!cls || !outTypeId)
        {
            return;
        }

        *outTypeId = cls->GetTypeId();
    }

    HYP_EXPORT void Class_GetTypeInfo(const Class* cls, TypeInfo const** outPTypeInfo)
    {
        if (!cls || !outPTypeInfo)
        {
            return;
        }

        *outPTypeInfo = cls->GetTypeInfo();
    }

    HYP_EXPORT uint32 Class_GetSize(const Class* cls)
    {
        if (!cls)
        {
            return 0;
        }

        return uint32(cls->GetSize());
    }

    HYP_EXPORT uint32 Class_GetFlags(const Class* cls)
    {
        if (!cls)
        {
            return 0;
        }

        return uint32(cls->GetFlags());
    }

    HYP_EXPORT int32 Class_GetStaticIndex(const Class* cls)
    {
        if (!cls)
        {
            return -1;
        }

        return int32(cls->GetStaticIndex());
    }

    HYP_EXPORT const ClassAttribute* Class_GetAttribute(const Class* cls, const Name* name)
    {
        if (!cls || !name)
        {
            return nullptr;
        }

        auto it = cls->GetAttributes().Find(StringHash(*name));

        if (it == cls->GetAttributes().End())
        {
            return nullptr;
        }

        return &*it;
    }

    HYP_EXPORT uint32 Class_GetAttributes(const Class* cls, const ClassAttribute** outAttributes)
    {
        if (!cls)
        {
            return 0;
        }

        const ClassAttributeSet& attributes = cls->GetAttributes();

        if (!outAttributes)
        {
            return uint32(attributes.Size());
        }

        uint32 index = 0;

        for (const ClassAttribute& attribute : attributes)
        {
            outAttributes[index++] = &attribute;
        }

        return index;
    }

    HYP_EXPORT uint32 Class_GetProperties(const Class* cls, const Property** outProperties)
    {
        return Class_GetMembersGeneric<Property, &Class::GetProperties>(cls, outProperties);
    }

    HYP_EXPORT Property* Class_GetProperty(const Class* cls, const Name* name)
    {
        if (!cls || !name)
        {
            return nullptr;
        }

        return cls->GetProperty(*name);
    }

    HYP_EXPORT uint32 Class_GetMethods(const Class* cls, const Method** outMethods)
    {
        return Class_GetMembersGeneric<Method, &Class::GetMethods>(cls, outMethods);
    }

    HYP_EXPORT Method* Class_GetMethod(const Class* cls, const Name* name)
    {
        if (!cls || !name)
        {
            return nullptr;
        }

        return cls->GetMethod(*name);
    }

    HYP_EXPORT uint32 Class_GetFields(const Class* cls, const Field** outFields)
    {
        return Class_GetMembersGeneric<Field, &Class::GetFields>(cls, outFields);
    }

    HYP_EXPORT Field* Class_GetField(const Class* cls, const Name* name)
    {
        if (!cls || !name)
        {
            return nullptr;
        }

        return cls->GetField(*name);
    }

    HYP_EXPORT uint32 Class_GetStaticFields(const Class* cls, const StaticField** outStaticFields)
    {
        return Class_GetMembersGeneric<StaticField, &Class::GetStaticFields>(cls, outStaticFields);
    }

    HYP_EXPORT StaticField* Class_GetStaticField(const Class* cls, const Name* name)
    {
        if (!cls || !name)
        {
            return nullptr;
        }

        return cls->GetStaticField(*name);
    }

    HYP_EXPORT Class* Class_CreateDynamicClass(const TypeId* typeId, const char* name, const Class* parentClass, int8 isAbstract, const char* assetBucket)
    {
        Assert(typeId != nullptr);
        Assert(name != nullptr);
        Assert(parentClass != nullptr);

#ifdef HYP_DOTNET
        Array<ClassAttribute> attributes;

        if (isAbstract)
        {
            attributes.PushBack(ClassAttribute(NAME("abstract"), ClassAttributeValue(true)));
        }

        if (assetBucket != nullptr && assetBucket[0] != '\0')
        {
            attributes.PushBack(ClassAttribute(NAME("assetbucket"), ClassAttributeValue(assetBucket)));
        }

        auto* dynamicClass = new DynamicClassInstance(
            *typeId,
            CreateNameFromDynamicString(name),
            parentClass,
            nullptr,
            attributes.ToSpan(),
            ClassFlags::CLASS_TYPE,
            Span<MemberVariant>());

        // hold a reference for the .NET wrapper; released in Class_DestroyDynamicClass
        dynamicClass->AddRef();

        return dynamicClass;
#else
        return nullptr;
#endif
    }

    // Reflects a [HypField] / [HypProperty] member of a managed type as a native Property, so it is saved and shown in the editor
    HYP_EXPORT int8 Class_AddManagedProperty(DynamicClassInstance* cls, const char* name, const TypeInfo* valueTypeInfo, const ManagedPropertyAttributeDesc* attributeDescs, uint32 numAttributes, int8 canSet)
    {
        Assert(cls != nullptr);
        Assert(name != nullptr);

#ifdef HYP_DOTNET
        if (!valueTypeInfo)
        {
            return 0;
        }

        const Name propertyName = CreateNameFromDynamicString(name);

        if (cls->GetProperty(propertyName) != nullptr)
        {
            HYP_LOG(DotNET, Warning, "Class {} already has a property named {}", cls->GetName(), propertyName);

            return 0;
        }

        Array<ClassAttribute> attributes;
        attributes.Reserve(numAttributes);

        for (uint32 attributeIndex = 0; attributeIndex < numAttributes; attributeIndex++)
        {
            const ManagedPropertyAttributeDesc& desc = attributeDescs[attributeIndex];

            switch (desc.kind)
            {
            case 0:
                attributes.PushBack(ClassAttribute(CreateNameFromDynamicString(desc.name), ClassAttributeValue(desc.intValue != 0)));
                break;
            case 1:
                attributes.PushBack(ClassAttribute(CreateNameFromDynamicString(desc.name), ClassAttributeValue(int(desc.intValue))));
                break;
            case 2:
                attributes.PushBack(ClassAttribute(CreateNameFromDynamicString(desc.name), ClassAttributeValue(desc.stringValue)));
                break;
            default:
                break;
            }
        }

        const ANSIString managedPropertyName(name);

        PropertyGetter getter;
        getter.getProc = Proc<BoxedValue(const BoxedValue&)>([managedPropertyName](const BoxedValue& target) -> BoxedValue
            {
                BoxedValue value;

                WithManagedProperty(target, managedPropertyName, [&value](dotnet::ManagedObject* managedObject, dotnet::ManagedProperty* managedProperty)
                    {
                        return managedProperty->CanGet() && managedProperty->InvokeGetterBoxed(managedObject, value);
                    });

                return value;
            });
        getter.typeInfo.valueTypeInfo = valueTypeInfo;

        PropertySetter setter;

        if (canSet)
        {
            setter.setProc = Proc<void(BoxedValue&, const BoxedValue&)>([managedPropertyName](BoxedValue& target, const BoxedValue& value) -> void
                {
                    WithManagedProperty(target, managedPropertyName, [&value](dotnet::ManagedObject* managedObject, dotnet::ManagedProperty* managedProperty)
                        {
                            return managedProperty->CanSet() && managedProperty->InvokeSetterBoxed(managedObject, value);
                        });
                });
        }

        setter.typeInfo.valueTypeInfo = valueTypeInfo;

        cls->AddProperty(new Property(propertyName, std::move(getter), std::move(setter), attributes.ToSpan()));

        return 1;
#else
        return 0;
#endif
    }

    HYP_EXPORT void Class_DestroyDynamicClass(DynamicClassInstance* cls)
    {
        Assert(cls != nullptr);

        // release the reference held by the .NET wrapper instead of deleting directly,
        // so the class is not freed while instances still reference it
        cls->Release();
    }

    HYP_EXPORT const Class* Class_GetParent(const Class* cls)
    {
        if (!cls)
        {
            return nullptr;
        }

        return cls->GetParent();
    }

#pragma endregion Class

} // extern "C"
