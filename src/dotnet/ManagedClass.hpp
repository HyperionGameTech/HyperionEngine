/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Types.hpp>

#include <core/memory/RefCountedPtr.hpp>

#include <core/containers/HashMap.hpp>
#include <core/containers/String.hpp>

#include <core/utilities/StringView.hpp>
#include <core/utilities/EnumFlags.hpp>

#include <core/reflection/HypData.hpp>

#include <dotnet/ManagedMethod.hpp>
#include <dotnet/ManagedProperty.hpp>
#include <dotnet/ManagedAttribute.hpp>
#include <dotnet/ManagedObject.hpp>

#include <dotnet/interop/ManagedGuid.hpp>

namespace hyperion {

class Class;

enum class ManagedClassFlags : uint32
{
    NONE = 0x0,
    CLASS_TYPE = 0x1,
    STRUCT_TYPE = 0x2,
    ENUM_TYPE = 0x4,

    ABSTRACT = 0x8
};

HYP_MAKE_ENUM_FLAGS(ManagedClassFlags)

} // namespace hyperion

namespace hyperion::dotnet {
class ManagedObject;
class ManagedClass;
class Assembly;

struct ManagedClassDesc
{
    int32 typeHash;
    ManagedClass* pClass;
    ManagedGuid assemblyGuid;
    ManagedGuid newObjectGuid;
    ManagedGuid freeObjectGuid;
    ManagedGuid marshalObjectGuid;
    uint32 flags;
};

class HYP_API ManagedClass : public EnableRefCountedPtrFromThis<ManagedClass>
{
public:
    /*! \brief Function to create a new object of this class.
     *  If keepAlive is true, the object will have a strong GCHandle allocated for it, and it will not be collected by the .NET runtime
     *  until the object is released via \ref{ManagedClass::~ManagedClass}.
     *  If false, only a weak GCHandle will be created.
     *
     *  If cls is provided (not nullptr), the object is constructed as a HypObject instance (must derive HypObject class).
     *  In this case, nativeObjectPtr must also be provided.
     *  Both cls and nativeObjectPtr can be nullptr. */
    using InitializeObjectCallbackFunction = void (*)(void* ctx, void* dst, uint32 dstSize);

    using NewObjectFunction = ObjectReference (*)(bool keepAlive, const Class* cls, void* nativeObjectPtr, void* contextPtr, InitializeObjectCallbackFunction callback);
    using MarshalObjectFunction = ObjectReference (*)(const void* intptr, uint32 size);

    ManagedClass(const Weak<Assembly>& assembly, String name, uint32 size, TypeId typeId, const Class* cls, ManagedClass* parentClass, EnumFlags<ManagedClassFlags> flags)
        : m_assembly(assembly),
          m_name(std::move(name)),
          m_size(size),
          m_typeId(typeId),
          m_class(cls),
          m_parentClass(parentClass),
          m_flags(flags),
          m_newObjectFptr(nullptr),
          m_marshalObjectFptr(nullptr)
    {
    }

    ManagedClass(const ManagedClass&) = delete;
    ManagedClass& operator=(const ManagedClass&) = delete;

    ManagedClass(ManagedClass&&) noexcept = delete;
    ManagedClass& operator=(ManagedClass&&) noexcept = delete;

    ~ManagedClass();

    HYP_FORCE_INLINE const String& GetName() const
    {
        return m_name;
    }

    HYP_FORCE_INLINE uint32 GetSize() const
    {
        return m_size;
    }

    HYP_FORCE_INLINE TypeId GetTypeId() const
    {
        return m_typeId;
    }

    HYP_FORCE_INLINE const Class* GetClass() const
    {
        return m_class;
    }

    HYP_FORCE_INLINE ManagedClass* GetParentClass() const
    {
        return m_parentClass;
    }

    HYP_FORCE_INLINE EnumFlags<ManagedClassFlags> GetFlags() const
    {
        return m_flags;
    }

    HYP_FORCE_INLINE NewObjectFunction GetNewObjectFunction() const
    {
        return m_newObjectFptr;
    }

    HYP_FORCE_INLINE void SetNewObjectFunction(NewObjectFunction newObjectFptr)
    {
        m_newObjectFptr = newObjectFptr;
    }

    HYP_FORCE_INLINE void SetMarshalObjectFunction(MarshalObjectFunction marshalObjectFptr)
    {
        m_marshalObjectFptr = marshalObjectFptr;
    }

    HYP_FORCE_INLINE MarshalObjectFunction GetMarshalObjectFunction() const
    {
        return m_marshalObjectFptr;
    }

    HYP_FORCE_INLINE const ManagedAttributeSet& GetAttributes() const
    {
        return m_attributes;
    }

    HYP_FORCE_INLINE void SetAttributes(ManagedAttributeSet&& attributes)
    {
        m_attributes = std::move(attributes);
    }

    /*! \brief Check if a method exists by name.
     *
     *  \param methodName The name of the method to check.
     *
     *  \return True if the method exists, otherwise false.
     */
    HYP_FORCE_INLINE bool HasMethod(UTF8StringView methodName) const
    {
        return m_methods.FindAs(methodName) != m_methods.End();
    }

    /*! \brief Get a method by the hash code of its name
     *
     *  \param hashCode The hash code of the method name to get.
     *
     *  \return A pointer to the method object if it exists, otherwise nullptr.
     */
    HYP_FORCE_INLINE ManagedMethod* GetMethodByHash(HashCode hashCode)
    {
        auto it = m_methods.FindByHashCode(hashCode);
        if (it == m_methods.End())
        {
            return nullptr;
        }

        return &it->second;
    }

    /*! \brief Get a method by the hash code of its name
     *
     *  \param hashCode The hash code of the method name to get.
     *
     *  \return A pointer to the method object if it exists, otherwise nullptr.
     */
    HYP_FORCE_INLINE const ManagedMethod* GetMethodByHash(HashCode hashCode) const
    {
        auto it = m_methods.FindByHashCode(hashCode);
        if (it == m_methods.End())
        {
            return nullptr;
        }

        return &it->second;
    }

    /*! \brief Get a method by name.
     *
     *  \param methodName The name of the method to get.
     *
     *  \return A pointer to the method object if it exists, otherwise nullptr.
     */
    HYP_FORCE_INLINE ManagedMethod* GetMethod(UTF8StringView methodName)
    {
        auto it = m_methods.FindAs(methodName);
        if (it == m_methods.End())
        {
            return nullptr;
        }

        return &it->second;
    }

    /*! \brief Get a method by name.
     *
     *  \param methodName The name of the method to get.
     *
     *  \return A pointer to the method object if it exists, otherwise nullptr.
     */
    HYP_FORCE_INLINE const ManagedMethod* GetMethod(UTF8StringView methodName) const
    {
        auto it = m_methods.FindAs(methodName);
        if (it == m_methods.End())
        {
            return nullptr;
        }

        return &it->second;
    }

    /*! \brief Add a method to this class.
     *
     *  \param methodName The name of the method to add.
     *  \param methodObject The method object to add.
     */
    HYP_FORCE_INLINE void AddMethod(const String& methodName, ManagedMethod&& methodObject)
    {
        m_methods[methodName] = std::move(methodObject);
    }

    /*! \brief Get all methods of this class.
     *
     *  \return A reference to the map of methods.
     */
    HYP_FORCE_INLINE const HashMap<String, ManagedMethod>& GetMethods() const
    {
        return m_methods;
    }

    /*! \brief Check if a property exists by name.
     *
     *  \param propertyName The name of the property to check.
     *
     *  \return True if the property exists, otherwise false.
     */
    HYP_FORCE_INLINE bool HasProperty(UTF8StringView propertyName) const
    {
        return m_properties.FindAs(propertyName) != m_properties.End();
    }

    /*! \brief Get a property by name.
     *
     *  \param propertyName The name of the property to get.
     *
     *  \return A pointer to the property object if it exists, otherwise nullptr.
     */
    HYP_FORCE_INLINE ManagedProperty* GetProperty(UTF8StringView propertyName)
    {
        auto it = m_properties.FindAs(propertyName);
        if (it == m_properties.End())
        {
            return nullptr;
        }

        return &it->second;
    }

    /*! \brief Get a property by name.
     *
     *  \param propertyName The name of the property to get.
     *
     *  \return A pointer to the property object if it exists, otherwise nullptr.
     */
    HYP_FORCE_INLINE const ManagedProperty* GetProperty(UTF8StringView propertyName) const
    {
        auto it = m_properties.FindAs(propertyName);
        if (it == m_properties.End())
        {
            return nullptr;
        }

        return &it->second;
    }

    /*! \brief Add a property to this class.
     *
     *  \param propertyName The name of the property to add.
     *  \param propertyObject The property object to add.
     */
    HYP_FORCE_INLINE void AddProperty(const String& propertyName, ManagedProperty&& propertyObject)
    {
        m_properties[propertyName] = std::move(propertyObject);
    }

    /*! \brief Get all properties of this class.
     *
     *  \return A reference to the map of properties.
     */
    HYP_FORCE_INLINE const HashMap<String, ManagedProperty>& GetProperties() const
    {
        return m_properties;
    }

    RC<Assembly> GetAssembly() const;

    /*! \brief Create a new managed object of this class.
     *  The new object will be removed from the managed object cache when the object goes out of scope, allowing for the .NET runtime to collect it.
     *  The returned object will hold a reference to this class instance, so it will need to remain valid for the lifetime of the object.
     *
     *  \return The new managed object.
     */
    HYP_NODISCARD ManagedObject* NewObject();

    /*! \brief Create a new managed object of this class.
     *  The new object will be removed from the managed object cache when the object goes out of scope, allowing for the .NET runtime to collect it.
     *  The returned object will hold a reference to this class instance, so it will need to remain valid for the lifetime of the object.
     *
     *  \return The new managed object.
     */
    HYP_NODISCARD ManagedObject* NewObject(const Class* cls, void* pOwner);

    /*! \brief Create a new managed object of this class, but do not allow its lifetime to be managed from the C++ side.
     *  A struct containing the object's GUID and .NET object address will be returned.
     *
     *  \return A struct containing the object's GUID and .NET object address
     */
    HYP_NODISCARD ObjectReference NewManagedObject(void* contextPtr = nullptr, InitializeObjectCallbackFunction callback = nullptr);

    /*! \brief Check if this class has a parent class with the given name.
     *
     *  \param parentClassName The name of the parent class to check.
     *
     *  \return True if this class has the parent class, otherwise false.
     */
    bool HasParentClass(UTF8StringView parentClassName) const;

    /*! \brief Check if this class has \ref{parentClass} as a parent class.
     *
     *  \param parentClass The parent class to check.
     *
     *  \return True if this class has the parent class, otherwise false.
     */
    bool HasParentClass(const ManagedClass* parentClass) const;

    template <class ReturnType, class... Args>
    ReturnType InvokeStaticMethod(UTF8StringView methodName, Args&&... args)
    {
        auto it = m_methods.FindAs(methodName);
        Assert(it != m_methods.End(), "Method not found");

        const ManagedMethod& methodObject = it->second;
        const ManagedMethod* methodPtr = &methodObject;

        if constexpr (sizeof...(args) != 0)
        {
            HypData* argsArray = (HypData*)StackAlloc(sizeof(HypData) * sizeof...(args));
            const HypData* argsArrayPtr[sizeof...(args) + 1]; // Mark last as nullptr so C# can use it as a null terminator

            SetArgs_HypData(std::make_index_sequence<sizeof...(args)>(), argsArray, argsArrayPtr, std::forward<Args>(args)...);

            if constexpr (std::is_void_v<ReturnType>)
            {
                InvokeStaticMethod_Internal(methodPtr, argsArrayPtr, nullptr);
            }
            else
            {
                HypData returnHypData;
                InvokeStaticMethod_Internal(methodPtr, argsArrayPtr, &returnHypData);

                if (returnHypData.IsNull())
                {
                    return ReturnType();
                }

                return std::move(returnHypData.Get<ReturnType>());
            }
        }
        else
        {
            const HypData* argsArrayPtr[] = { nullptr };

            if constexpr (std::is_void_v<ReturnType>)
            {
                InvokeStaticMethod_Internal(methodPtr, argsArrayPtr, nullptr);
            }
            else
            {
                HypData returnHypData;
                InvokeStaticMethod_Internal(methodPtr, argsArrayPtr, &returnHypData);

                if (returnHypData.IsNull())
                {
                    return ReturnType();
                }

                return std::move(returnHypData.Get<ReturnType>());
            }
        }
    }

private:
    void InvokeStaticMethod_Internal(const ManagedMethod* methodPtr, const HypData** argsHypData, HypData* outReturnHypData);

    String m_name;
    uint32 m_size;
    TypeId m_typeId;
    const Class* m_class;
    ManagedClass* m_parentClass;
    EnumFlags<ManagedClassFlags> m_flags;
    HashMap<String, ManagedMethod> m_methods;
    HashMap<String, ManagedProperty> m_properties;

    Weak<Assembly> m_assembly;

    NewObjectFunction m_newObjectFptr;
    MarshalObjectFunction m_marshalObjectFptr;

    ManagedAttributeSet m_attributes;
};

} // namespace hyperion::dotnet
