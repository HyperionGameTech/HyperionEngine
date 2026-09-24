/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <Core/Reflection/Struct.hpp>
#include <Core/Reflection/ClassRegistry.hpp>
#include <Core/Reflection/Field.hpp>
#include <Core/Reflection/Property.hpp>
#include <Core/Reflection/MemberVariant.hpp>

#include <Core/Reflection/TypeInfo.hpp>

#include <Core/Threading/AtomicVar.hpp>

#include <Core/Logging/Logger.hpp>
#include <Core/Logging/LogChannels.hpp>

#ifdef HYP_DOTNET
#include <DotNET/ManagedClass.hpp>
#include <DotNET/ManagedObject.hpp>
#endif

namespace Hyperion {

static void Struct_AddRefIfDynamic(const Struct* pStruct)
{
    if (pStruct->IsDynamic())
    {
        const_cast<DynamicStructInstance*>(static_cast<const DynamicStructInstance*>(pStruct))->AddRef();
    }
}

static void Struct_ReleaseIfDynamic(const Struct* pStruct)
{
    if (pStruct->IsDynamic())
    {
        const_cast<DynamicStructInstance*>(static_cast<const DynamicStructInstance*>(pStruct))->Release();
    }
}

static void StructBox_DestroyObject(void* context, void* object)
{
    const Struct* pStruct = static_cast<const Struct*>(context);

    pStruct->DestructInPlace(object);
    GetDefaultAllocatorInstance<DynamicAllocator>()->Free(object);

    Struct_ReleaseIfDynamic(pStruct);
}

static void* StructBox_CopyBlock(void* context, const void* sourceBlock)
{
    const Any::Block* source = static_cast<const Any::Block*>(sourceBlock);
    const Struct* pStruct = static_cast<const Struct*>(context);

    void* object = GetDefaultAllocatorInstance<DynamicAllocator>()->Allocate(pStruct->GetSize(), pStruct->GetAlignment());
    HYP_CORE_ASSERT(object != nullptr);

    pStruct->CopyConstructInPlace(object, source->objectPtr);

    // the new block keeps its own reference on the struct
    Struct_AddRefIfDynamic(pStruct);

    void* raw = GetDefaultAllocatorInstance<DynamicAllocator>()->Allocate(sizeof(Any::Block), alignof(Any::Block));
    HYP_CORE_ASSERT(raw != nullptr);

    return new (raw) Any::Block {
        source->typeInfo,
        object,
        context,
        source->copyCtor,
        source->objectDtor,
        source->dtor,
        source->objSize,
        source->objAlign
    };
}

#pragma region DynamicStructFields

struct DynamicFieldAccess
{
    BoxedValue (*read)(const DynamicFieldAccess& access, const ubyte* address) = nullptr;
    bool (*write)(const DynamicFieldAccess& access, ubyte* address, const BoxedValue& value) = nullptr;

    ClassRef elementStruct;
};

template <class T>
static BoxedValue ReadArithmeticField(const DynamicFieldAccess& access, const ubyte* address)
{
    T value;
    Memory::Copy(&value, address, sizeof(T));

    return BoxedValue(value);
}

template <class T>
static bool WriteArithmeticField(const DynamicFieldAccess& access, ubyte* address, const BoxedValue& value)
{
    T typedValue {};

    if (!value.IsNull())
    {
        auto converted = value.TryGet<T>();

        if (!converted.HasValue())
        {
            return false;
        }

        typedValue = T(*converted);
    }

    Memory::Copy(address, &typedValue, sizeof(T));

    return true;
}

static BoxedValue ReadStructField(const DynamicFieldAccess& access, const ubyte* address)
{
    const Struct* elementStruct = GetStructFromClass(access.elementStruct.cls);

    BoxedValue value;
    elementStruct->ConstructBoxed(value);
    Memory::Copy(value.ToRef().GetPointer(), address, elementStruct->GetSize());

    return value;
}

static bool WriteStructField(const DynamicFieldAccess& access, ubyte* address, const BoxedValue& value)
{
    const Struct* elementStruct = GetStructFromClass(access.elementStruct.cls);

    if (value.IsNull())
    {
        BoxedValue defaultValue;
        elementStruct->ConstructBoxed(defaultValue);
        Memory::Copy(address, defaultValue.ToRef().GetPointer(), elementStruct->GetSize());

        return true;
    }

    return AssignStructValue(*elementStruct, address, value);
}

// true when T is the field's type; outMade is set when the element size fits it too
template <class T>
static bool TryMakeArithmeticAccess(const TypeInfo& typeInfo, uint32 elementSize, DynamicFieldAccess& outAccess, bool& outMade)
{
    if constexpr (std::is_arithmetic_v<T>)
    {
        if (typeInfo.id == TypeId::ForType<T>())
        {
            outMade = elementSize == sizeof(T);

            if (outMade)
            {
                outAccess.read = &ReadArithmeticField<T>;
                outAccess.write = &WriteArithmeticField<T>;
            }

            return true;
        }
    }

    return false;
}

template <class... Types>
static bool MakeArithmeticAccess(const TypeInfo& typeInfo, uint32 elementSize, DynamicFieldAccess& outAccess, bool& outMade, const Variant<Types...>*)
{
    return (TryMakeArithmeticAccess<Types>(typeInfo, elementSize, outAccess, outMade) || ...);
}

static bool MakeFieldAccess(const DynamicStructFieldDesc& fieldDesc, uint32 elementSize, DynamicFieldAccess& outAccess)
{
    const TypeInfo& typeInfo = *fieldDesc.typeInfo;

    if (typeInfo.IsFundamental())
    {
        bool made = false;
        const bool matched = MakeArithmeticAccess(typeInfo, elementSize, outAccess, made, static_cast<const BoxedValue::VariantType*>(nullptr));

        if (made)
        {
            return true;
        }

        if (matched)
        {
            HYP_LOG(Object, Warning, "Field '{}' has {}-byte elements but {} needs {}; it won't be reflected", fieldDesc.name, elementSize, typeInfo.name, typeInfo.size);
        }
        else
        {
            HYP_LOG(Object, Warning, "Field '{}' has type {}, which BoxedValue can't hold; it won't be reflected", fieldDesc.name, typeInfo.name);
        }

        return false;
    }

    const Struct* elementStruct = GetStructFromClass(typeInfo.GetClass());

    if (!elementStruct)
    {
        HYP_LOG(Object, Warning, "Field '{}' has type {}, which isn't a reflected struct; it won't be reflected", fieldDesc.name, typeInfo.name);

        return false;
    }

    if (elementSize != elementStruct->GetSize())
    {
        HYP_LOG(Object, Warning, "Field '{}' has {}-byte elements but {} needs {}; it won't be reflected", fieldDesc.name, elementSize, elementStruct->GetName(), elementStruct->GetSize());

        return false;
    }

    // the owning struct is copied and destroyed as raw bytes, so its fields can't have copy or destroy logic of their own
    if (!elementStruct->IsTriviallyCopyable() || !elementStruct->CanConstructInPlace())
    {
        HYP_LOG(Object, Warning, "Field '{}' has type {}, which isn't trivially copyable; it won't be reflected", fieldDesc.name, elementStruct->GetName());

        return false;
    }

    outAccess.read = &ReadStructField;
    outAccess.write = &WriteStructField;
    outAccess.elementStruct = ClassRef(elementStruct);

    return true;
}

// A copy of the elements that the reflection layer can't grow or shrink (the storage is inline and fixed-size).
static BoxedValue MakeFixedSizeArrayValue(Array<BoxedValue>&& elements)
{
    GenericArrayWrapper wrapper(GenericArrayWrapper::AS_COPY, std::move(elements));
    wrapper.functionTable.pushBack = nullptr;
    wrapper.functionTable.resize = nullptr;

    return BoxedValue(std::move(wrapper));
}

bool MakeDynamicStructProperty(const DynamicStructFieldDesc& fieldDesc, int editorOrder, MemberVariant& outMember)
{
    if (!fieldDesc.typeInfo)
    {
        return false;
    }

    const uint32 elementCount = fieldDesc.arrayLength != 0 ? fieldDesc.arrayLength : 1;

    if (fieldDesc.size == 0 || fieldDesc.size % elementCount != 0)
    {
        HYP_LOG(Object, Warning, "Field '{}' is {} bytes, which isn't {} whole elements; it won't be reflected", fieldDesc.name, fieldDesc.size, elementCount);

        return false;
    }

    const uint32 elementSize = fieldDesc.size / elementCount;

    DynamicFieldAccess access;

    if (!MakeFieldAccess(fieldDesc, elementSize, access))
    {
        return false;
    }

    Array<ClassAttribute> attributes;
    attributes.PushBack(ClassAttribute("editororder", editorOrder));

    if (fieldDesc.isTransient)
    {
        attributes.PushBack(ClassAttribute("transient", true));
        attributes.PushBack(ClassAttribute("editor", false));
    }

    const uint32 offset = fieldDesc.offset;
    const Name fieldName = fieldDesc.name;

    PropertyGetter getter;
    PropertySetter setter;

    if (fieldDesc.arrayLength == 0)
    {
        getter.getProc = Proc<BoxedValue(const BoxedValue&)>([access, offset](const BoxedValue& target) -> BoxedValue
            {
                return access.read(access, static_cast<const ubyte*>(target.ToRef().GetPointer()) + offset);
            });
        getter.typeInfo.valueTypeInfo = fieldDesc.typeInfo;

        setter.setProc = Proc<void(BoxedValue&, const BoxedValue&)>([access, offset, fieldName](BoxedValue& target, const BoxedValue& value) -> void
            {
                if (!access.write(access, static_cast<ubyte*>(target.ToRef().GetPointer()) + offset, value))
                {
                    HYP_LOG(Object, Warning, "Cannot set field '{}' from a value of type {}", fieldName, value.GetTypeInfo() ? value.GetTypeInfo()->name : Name::Invalid());
                }
            });
        setter.typeInfo.valueTypeInfo = fieldDesc.typeInfo;
    }
    else
    {
        getter.getProc = Proc<BoxedValue(const BoxedValue&)>([access, offset, elementCount, elementSize](const BoxedValue& target) -> BoxedValue
            {
                const ubyte* base = static_cast<const ubyte*>(target.ToRef().GetPointer()) + offset;

                Array<BoxedValue> elements;
                elements.Reserve(elementCount);

                for (uint32 elementIndex = 0; elementIndex < elementCount; elementIndex++)
                {
                    elements.PushBack(access.read(access, base + elementIndex * elementSize));
                }

                return MakeFixedSizeArrayValue(std::move(elements));
            });
        getter.typeInfo.valueTypeInfo = &TypeOf<Array<BoxedValue>>();

        // elements past the saved (or edited) count keep their current values, so a longer array in a newer layout still loads
        setter.setProc = Proc<void(BoxedValue&, const BoxedValue&)>([access, offset, elementCount, elementSize, fieldName](BoxedValue& target, const BoxedValue& value) -> void
            {
                auto wrapper = value.TryGet<GenericArrayWrapper>();

                if (!wrapper.HasValue())
                {
                    HYP_LOG(Object, Warning, "Cannot set array field '{}' from a value that isn't an array", fieldName);

                    return;
                }

                GenericArrayWrapper& elements = const_cast<GenericArrayWrapper&>(*wrapper);
                ubyte* base = static_cast<ubyte*>(target.ToRef().GetPointer()) + offset;

                const size_t count = elements.Size() < elementCount ? elements.Size() : elementCount;

                for (size_t elementIndex = 0; elementIndex < count; elementIndex++)
                {
                    BoxedValue element;

                    if (!elements.GetElementAt(elementIndex, element) || !access.write(access, base + elementIndex * elementSize, element))
                    {
                        HYP_LOG(Object, Warning, "Cannot set element {} of array field '{}'", elementIndex, fieldName);
                    }
                }
            });
        setter.typeInfo.valueTypeInfo = &TypeOf<Array<BoxedValue>>();
    }

    outMember = MemberVariant(Property(fieldDesc.name, std::move(getter), std::move(setter), attributes.ToSpan()));

    return true;
}

DynamicStructInstance* CreateDynamicStruct(const DynamicStructDesc& desc)
{
    if (desc.size == 0 || desc.size > UINT16_MAX || desc.alignment == 0 || (desc.alignment & (desc.alignment - 1)) != 0)
    {
        HYP_LOG(Object, Error, "Cannot create dynamic Struct {}: invalid size {} or alignment {}", desc.name, desc.size, desc.alignment);

        return nullptr;
    }

    // a struct may replace an earlier definition of itself, but not an unrelated class
    if (const Class* existingClass = ClassRegistry::GetInstance().GetClass(desc.typeId); existingClass && !existingClass->IsStructType())
    {
        HYP_LOG(Object, Error, "Cannot create dynamic Struct {}: TypeId {} is already used by {}", desc.name, desc.typeId.Value(), existingClass->GetName());

        return nullptr;
    }

    // reflected fields make the struct visible to serialization and the editor
    Array<MemberVariant> members;
    members.Reserve(desc.fields.Size());

    for (size_t fieldIndex = 0; fieldIndex < desc.fields.Size(); fieldIndex++)
    {
        const DynamicStructFieldDesc& field = desc.fields[fieldIndex];

        if (!field.name.IsValid() || uint64(field.offset) + uint64(field.size) > uint64(desc.size))
        {
            HYP_LOG(Object, Warning, "Skipping invalid field {} of dynamic Struct {}", fieldIndex, desc.name);

            continue;
        }

        MemberVariant member;

        if (MakeDynamicStructProperty(field, int(fieldIndex), member))
        {
            members.PushBack(std::move(member));
        }
    }

    DynamicStructInstance* pStruct = new DynamicStructInstance(
        desc.typeId,
        desc.name,
        desc.size,
        desc.alignment,
        Span<const ClassAttribute>(),
        ClassFlags::STRUCT_TYPE | ClassFlags::DYNAMIC,
        members.ToSpan(),
        DynamicStructInstanceFunctions {});

    pStruct->SetDefaultValue(desc.defaultValue);

    return pStruct;
}

const Struct* GetBoxingStruct(const BoxedValue& value)
{
    const TypeInfo* typeInfo = value.GetTypeInfo();

    if (!typeInfo)
    {
        return nullptr;
    }

    // a dynamic struct's TypeInfo names its own definition; a static struct only ever has the one
    if (const Class* ownerClass = typeInfo->extendedInfo.GetOwnerClass())
    {
        return GetStructFromClass(ownerClass);
    }

    return GetStructFromClass(typeInfo->GetClass());
}

bool AssignStructValue(const Struct& targetStruct, void* destination, const BoxedValue& value)
{
    const AnyRef valueRef = value.ToRef();

    if (!valueRef.HasValue() || valueRef.GetTypeId() != targetStruct.GetTypeId())
    {
        return false;
    }

    const Struct* sourceStruct = GetBoxingStruct(value);

    if (!sourceStruct || sourceStruct == &targetStruct)
    {
        Memory::Copy(destination, valueRef.GetPointer(), targetStruct.GetSize());

        return true;
    }

    // an older definition of the type.
    // its layout may differ, so start from the defaults and copy fields by name
    BoxedValue converted;

    if (!targetStruct.ConstructBoxed(converted))
    {
        return false;
    }

    CopyMatchingProperties(*sourceStruct, value, targetStruct, converted);
    Memory::Copy(destination, converted.ToRef().GetPointer(), targetStruct.GetSize());

    return true;
}

void CopyMatchingProperties(const Struct& sourceStruct, const BoxedValue& source, const Struct& targetStruct, BoxedValue& target)
{
    for (const Property* sourceProperty : sourceStruct.GetProperties())
    {
        if (!sourceProperty->CanGet())
        {
            continue;
        }

        const Property* targetProperty = targetStruct.GetProperty(sourceProperty->GetName(), /* deep */ false);

        if (!targetProperty || !targetProperty->CanSet() || targetProperty->GetTypeInfo().id != sourceProperty->GetTypeInfo().id)
        {
            continue;
        }

        targetProperty->Set(target, sourceProperty->Get(source));
    }
}

#pragma endregion DynamicStructFields

#pragma region Struct

void* Struct::AllocateBoxedObject() const
{
    HYP_CORE_ASSERT(GetSize() != 0, "Struct %s has no size", GetName().LookupString());

    void* object = GetDefaultAllocatorInstance<DynamicAllocator>()->Allocate(GetSize(), GetAlignment() != 0 ? GetAlignment() : alignof(void*));
    HYP_CORE_ASSERT(object != nullptr);

    return object;
}

void Struct::MakeBoxFromObject(void* object, BoxedValue& outBoxed, const TypeInfo* boxedTypeInfo) const
{
    if (!boxedTypeInfo)
    {
        boxedTypeInfo = GetTypeInfo();
    }

    HYP_CORE_ASSERT(boxedTypeInfo != nullptr);

    // the box keeps a reference on this struct; released in StructBox_DestroyObject
    Struct_AddRefIfDynamic(this);

    outBoxed = BoxedValue(Any::FromVoidPointer<DynamicAllocator>(
        boxedTypeInfo,
        object,
        CanCopyConstructInPlace() ? &StructBox_CopyBlock : nullptr,
        &StructBox_DestroyObject,
        const_cast<void*>(static_cast<const void*>(this)),
        GetSize(),
        GetAlignment()));
}

bool Struct::ConstructBoxed(BoxedValue& outBoxed, const TypeInfo* boxedTypeInfo) const
{
    if (!CanConstructInPlace())
    {
        return false;
    }

    void* object = AllocateBoxedObject();
    ConstructInPlace(object);

    MakeBoxFromObject(object, outBoxed, boxedTypeInfo);

    return true;
}

bool Struct::CopyConstructBoxed(const void* source, BoxedValue& outBoxed, const TypeInfo* boxedTypeInfo) const
{
    HYP_CORE_ASSERT(source != nullptr);

    if (!CanCopyConstructInPlace())
    {
        return false;
    }

    void* object = AllocateBoxedObject();
    CopyConstructInPlace(object, source);

    MakeBoxFromObject(object, outBoxed, boxedTypeInfo);

    return true;
}

bool Struct::MoveConstructBoxed(void* source, BoxedValue& outBoxed, const TypeInfo* boxedTypeInfo) const
{
    HYP_CORE_ASSERT(source != nullptr);

    if (!CanMoveConstructInPlace())
    {
        return false;
    }

    void* object = AllocateBoxedObject();
    MoveConstructInPlace(object, source);

    MakeBoxFromObject(object, outBoxed, boxedTypeInfo);

    return true;
}

bool Struct::CreateStructInstance(dotnet::ObjectReference& outObjectReference, const void* objectPtr, size_t size) const
{
    struct ManagedStructInitializerContext
    {
        const void* ptr;
        size_t size;
    };

    AssertDebug(objectPtr != nullptr);

#ifdef HYP_DOTNET
    if (dotnet::ManagedClass* managedClass = Class::GetManagedClass())
    {
        ManagedStructInitializerContext context;
        context.ptr = objectPtr;
        context.size = size;

        ScriptObjectFunctions::ManagedClassNewManagedObject(managedClass, &context, [](void* contextPtr, void* objectPtr, uint32 objectSize)
            {
                ManagedStructInitializerContext& context = *static_cast<ManagedStructInitializerContext*>(contextPtr);

                AssertDebug(objectSize == context.size, "Type size does not match managed struct size! Expected managed struct to have size of %zu but got %u",
                    context.size, objectSize);

                Memory::Copy(objectPtr, context.ptr, context.size);
            }, &outObjectReference);

        return outObjectReference.weakHandle != nullptr;
    }
#endif

    return false;
}

#pragma endregion Struct

#pragma region DynamicStructInstance


DynamicStructInstance::DynamicStructInstance(
    TypeId typeId,
    Name name,
    uint32 size,
    uint32 alignment,
    Span<const ClassAttribute> attributes,
    EnumFlags<ClassFlags> flags,
    Span<MemberVariant> members,
    const DynamicStructInstanceFunctions& functions)
    : Struct(typeId, name, -1, 0, Name::Invalid(), attributes, flags | ClassFlags::STRUCT_TYPE | ClassFlags::DYNAMIC, members),
      m_functions(functions)
{
    // starts at 1 for the caller that created it (released via Struct_DestroyDynamicStruct);
    // boxed instances of this struct take their own references on top of that.
    m_refCount = 1;

    Assert(size > 0);
    Assert(size <= UINT16_MAX, "Dynamic struct size {} exceeds TypeInfo limit", size);
    Assert(alignment != 0 && (alignment & (alignment - 1)) == 0, "Dynamic struct alignment {} must be a power of two", alignment);

    m_size = size;
    m_alignment = alignment;

    // the TypeInfo was built by the Class constructor before size/alignment were known; we own it for dynamic types
    TypeInfo* pTypeInfo = const_cast<TypeInfo*>(GetTypeInfo());
    Assert(pTypeInfo != nullptr);

    pTypeInfo->size = uint16(size);
    pTypeInfo->alignment = uint16(alignment);

    // a newer definition (eg a script reload changed the layout) takes over the TypeId; the previous one stays alive while referenced
    if (const Class* previousDefinition = ClassRegistry::GetInstance().GetClass(typeId); previousDefinition && previousDefinition->IsStructType())
    {
        ClassRegistry::GetInstance().Unregister(previousDefinition);
    }

    /// \todo Register the ManagedClass (dotnet::ManagedClass) for this. We need the assembly.
    ClassRegistry::GetInstance().Register(typeId, this);
}

DynamicStructInstance::~DynamicStructInstance()
{
    Assert(AtomicAdd(&m_refCount, 0) <= 0, "DynamicStructInstance destroyed while still being referenced!");
}

#ifdef HYP_DOTNET
bool DynamicStructInstance::GetManagedObject(const void* objectPtr, dotnet::ObjectReference& outObjectReference) const
{
    Assert(objectPtr != nullptr);

    // Construct a new instance of the struct and return an ObjectReference pointing to it.
    if (!CreateStructInstance(outObjectReference, objectPtr, m_size))
    {
        return false;
    }

    return true;
}
#endif

bool DynamicStructInstance::ToBoxed(ByteView memory, BoxedValue& out) const
{
    Assert(memory.Size() >= m_size);

    return MoveConstructBoxed(memory.Data(), out);
}

bool DynamicStructInstance::CreateInstance_Internal(BoxedValue& out) const
{
    return ConstructBoxed(out);
}

void DynamicStructInstance::SetDefaultValue(const void* defaultValue)
{
    if (!defaultValue)
    {
        m_defaultValue = ByteBuffer();

        return;
    }

    m_defaultValue = ByteBuffer(m_size, defaultValue);
}

void DynamicStructInstance::ConstructInPlace(void* destination) const
{
    if (m_functions.construct != nullptr)
    {
        m_functions.construct(GetFunctionContext(), destination);

        return;
    }

    if (m_defaultValue.Any())
    {
        Memory::Copy(destination, m_defaultValue.Data(), m_size);

        return;
    }

    Memory::Zero(destination, m_size);
}

void DynamicStructInstance::CopyConstructInPlace(void* destination, const void* source) const
{
    if (m_functions.copyConstruct != nullptr)
    {
        m_functions.copyConstruct(GetFunctionContext(), destination, source);

        return;
    }

    Memory::Copy(destination, source, m_size);
}

void DynamicStructInstance::MoveConstructInPlace(void* destination, void* source) const
{
    if (m_functions.moveConstruct != nullptr)
    {
        m_functions.moveConstruct(GetFunctionContext(), destination, source);

        return;
    }

    CopyConstructInPlace(destination, source);
}

void DynamicStructInstance::DestructInPlace(void* target) const
{
    if (m_functions.destruct != nullptr)
    {
        m_functions.destruct(GetFunctionContext(), target);
    }
}

bool DynamicStructInstance::CreateInstanceArray_Internal(Span<BoxedValue> elements, BoxedValue& out) const
{
    HYP_NOT_IMPLEMENTED();

    return false;
}

void DynamicStructInstance::AddRef()
{
    AtomicIncrement(&m_refCount);
}

void DynamicStructInstance::Release()
{
    if (AtomicDecrement(&m_refCount) <= 0)
    {
        // a struct replaced by a newer definition with the same TypeId is no longer registered
        if (ClassRegistry::GetInstance().GetClass(GetTypeId()) == this && !ClassRegistry::GetInstance().Unregister(this))
        {
            HYP_LOG(Object, Warning, "Failed to unregister dynamic Struct \"{}\"", GetName());
        }

        delete this;
    }
}

#pragma endregion DynamicStructInstance

} // namespace Hyperion
