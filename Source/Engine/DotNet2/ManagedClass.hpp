/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Types.hpp>

#include <Core/memory/RefCountedPtr.hpp>

#include <Core/containers/Map.hpp>
#include <Core/containers/String.hpp>

#include <Core/utilities/StringView.hpp>
#include <Core/utilities/EnumFlags.hpp>

#include <Core/reflection/BoxedValue.hpp>

#include <DotNET/ManagedMethod.hpp>
#include <DotNET/ManagedProperty.hpp>
#include <DotNET/ManagedAttribute.hpp>
#include <DotNET/ManagedObject.hpp>

#include <DotNET/interop/ManagedGuid.hpp>

namespace Hyperion {

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

} // namespace Hyperion

namespace Hyperion::dotnet {
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

class ENGINE_API ManagedClass : public EnableRefCountedPtrFromThis<ManagedClass>
{
public:
    /*! \brief Function to create a new object of this class.
     *  If keepAlive is true, the object will have a strong GCHandle allocated for it, and it will not be collected by the .NET runtime
     *  until the object is released via \ref{ManagedClass::~ManagedClass}.
     *  If false, only a weak GCHandle will be created.
     *
     *  If cls is provided (not nullptr), the object is constructed as a Object instance (must derive Object class).
     *  In this case, nativeObjectPtr must also be provided.
     *  Both cls and nativeObjectPtr can be nullptr. */
    using InitializeObjectCallbackFunction = void (*)(void* ctx, void* dst, uint32 dstSize);

    using NewObjectFunction = ObjectReference (*)(bool keepAlive, const Class* cls, void* nativeObjectPtr, void* contextPtr, InitializeObjectCallbackFunction callback);
    using MarshalObjectFunction = ObjectReference (*)(const void* intptr, uint32 size);

    ManagedClass(const Weak<Assembly>& assembly, const ANSIString& name, uint32 size, TypeId typeId, const Class* cls, ManagedClass* parentClass, EnumFlags<ManagedClassFlags> flags)
        : m_assembly(assembly),
          m_name(name),
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

    HYP_FORCE_INLINE const ANSIString& GetName() const
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
    HYP_FORCE_INLINE bool HasMethod(ANSIStringView methodName) const
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
    HYP_FORCE_INLINE ManagedMethod* GetMethod(ANSIStringView methodName)
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
    HYP_FORCE_INLINE const ManagedMethod* GetMethod(ANSIStringView methodName) const
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
    HYP_FORCE_INLINE void AddMethod(const ANSIString& methodName, ManagedMethod&& methodObject)
    {
        m_methods[methodName] = std::move(methodObject);
    }

    /*! \brief Get all methods of this class.
     *
     *  \return A reference to the map of methods.
     */
    HYP_FORCE_INLINE const TMap<ANSIString, ManagedMethod>& GetMethods() const
    {
        return m_methods;
    }

    /*! \brief Check if a property exists by name.
     *
     *  \param propertyName The name of the property to check.
     *
     *  \return True if the property exists, otherwise false.
     */
    HYP_FORCE_INLINE bool HasProperty(ANSIStringView propertyName) const
    {
        return m_properties.FindAs(propertyName) != m_properties.End();
    }

    /*! \brief Get a property by name.
     *
     *  \param propertyName The name of the property to get.
     *
     *  \return A pointer to the property object if it exists, otherwise nullptr.
     */
    HYP_FORCE_INLINE ManagedProperty* GetProperty(ANSIStringView propertyName)
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
    HYP_FORCE_INLINE const ManagedProperty* GetProperty(ANSIStringView propertyName) const
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
    HYP_FORCE_INLINE void AddProperty(const ANSIString& propertyName, ManagedProperty&& propertyObject)
    {
        m_properties[propertyName] = std::move(propertyObject);
    }

    /*! \brief Get all properties of this class.
     *
     *  \return A reference to the map of properties.
     */
    HYP_FORCE_INLINE const TMap<ANSIString, ManagedProperty>& GetProperties() const
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
    HYP_NODISCARD ManagedObject* NewObject(const Class* cls, void* owner);

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
    bool HasParentClass(ANSIStringView parentClassName) const;

    /*! \brief Check if this class has \p parentClass as a parent class.
     *
     *  \param parentClass The parent class to check.
     *
     *  \return True if this class has the parent class, otherwise false.
     */
    bool HasParentClass(const ManagedClass* parentClass) const;

    template <class ReturnType, class... Args>
    ReturnType InvokeStaticMethod(ANSIStringView methodName, Args&&... args)
    {
        auto it = m_methods.FindAs(methodName);
        Assert(it != m_methods.End(), "Method not found");

        const ManagedMethod& methodObject = it->second;
        const ManagedMethod* methodPtr = &methodObject;

        if constexpr (sizeof...(args) != 0)
        {
            BoxedValue* argsArray = (BoxedValue*)StackAlloc(sizeof(BoxedValue) * sizeof...(args));
            const BoxedValue* argsArrayPtr[sizeof...(args) + 1]; // Mark last as nullptr so C# can use it as a null terminator

            SetArgsBoxed(std::make_index_sequence<sizeof...(args)>(), argsArray, argsArrayPtr, std::forward<Args>(args)...);

            // BoxedValue elements are placement-new'd into alloca memory, so their destructors
            // must be called explicitly to release any owned resources (e.g. Handle<> ref counts).
            HYP_DEFER({
                for (size_t i = 0; i < sizeof...(args); ++i) {
                    argsArray[i].~BoxedValue();
                }
            });

            if constexpr (std::is_void_v<ReturnType>)
            {
                InvokeStaticMethod_Internal(methodPtr, argsArrayPtr, nullptr);
            }
            else
            {
                BoxedValue boxed;
                InvokeStaticMethod_Internal(methodPtr, argsArrayPtr, &boxed);

                if (boxed.IsNull())
                {
                    return ReturnType();
                }

                return std::move(boxed.Get<ReturnType>());
            }
        }
        else
        {
            const BoxedValue* argsArrayPtr[] = { nullptr };

            if constexpr (std::is_void_v<ReturnType>)
            {
                InvokeStaticMethod_Internal(methodPtr, argsArrayPtr, nullptr);
            }
            else
            {
                BoxedValue boxed;
                InvokeStaticMethod_Internal(methodPtr, argsArrayPtr, &boxed);

                if (boxed.IsNull())
                {
                    return ReturnType();
                }

                return std::move(boxed.Get<ReturnType>());
            }
        }
    }

private:
    void InvokeStaticMethod_Internal(const ManagedMethod* method, const BoxedValue** args, BoxedValue* outReturn);

    ANSIString m_name;
    uint32 m_size;
    TypeId m_typeId;
    const Class* m_class;
    ManagedClass* m_parentClass;
    EnumFlags<ManagedClassFlags> m_flags;
    TMap<ANSIString, ManagedMethod> m_methods;
    TMap<ANSIString, ManagedProperty> m_properties;

    Weak<Assembly> m_assembly;

    NewObjectFunction m_newObjectFptr;
    MarshalObjectFunction m_marshalObjectFptr;

    ManagedAttributeSet m_attributes;
};

} // namespace Hyperion::dotnet
