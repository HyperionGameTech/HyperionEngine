/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_DOTNET_CLASS_HPP
#define HYPERION_DOTNET_CLASS_HPP

#include <Types.hpp>

#include <core/memory/RefCountedPtr.hpp>

#include <core/containers/HashMap.hpp>
#include <core/containers/String.hpp>

#include <core/utilities/StringView.hpp>
#include <core/utilities/EnumFlags.hpp>

#include <core/object/HypData.hpp>

#include <dotnet/Method.hpp>
#include <dotnet/Property.hpp>
#include <dotnet/Attribute.hpp>

#include <dotnet/interop/ManagedObject.hpp>
#include <dotnet/interop/ManagedGuid.hpp>

namespace hyperion {

class HypClass;

enum class ManagedClassFlags : uint32
{
    NONE        = 0x0,
    CLASS_TYPE  = 0x1,
    STRUCT_TYPE = 0x2,
    ENUM_TYPE   = 0x4
};

HYP_MAKE_ENUM_FLAGS(ManagedClassFlags)

} // namespace hyperion

namespace hyperion::dotnet {
class Object;
class Class;
class ClassHolder;
class Assembly;

extern "C" {

struct ManagedClass
{
    int32                   type_hash;
    Class                   *class_object;
    ManagedGuid             assembly_guid;
    ManagedGuid             new_object_guid;
    ManagedGuid             free_object_guid;
    ManagedGuid             marshal_object_guid;
    uint32                  flags;
};

} // extern "C"

class HYP_API Class
{
public:
    /*! \brief Function to create a new object of this class.
     *  If keep_alive is true, the object will be stored in the ManagedObjectCache and will not be collected by the .NET runtime
     *  until the object is released via \ref{Class::~Class}.
     *  If it is false, it will be stored in the ManagedObjectCache as a weak object, and is not expected to be removed via \ref{Class::~Class}.
     *  
     *  If hyp_class is provided (not nullptr), the object is constructed as a HypObject instance (must derive HypObject class).
     *  In this case, native_object_ptr must also be provided.
     *  Both hyp_class and native_object_ptr can be nullptr. */
    using InitializeObjectCallbackFunction = void(*)(void *ctx, void *dst, uint32 dst_size);

    using NewObjectFunction = ObjectReference(*)(bool keep_alive, const HypClass *hyp_class, void *native_object_ptr, void *context_ptr, InitializeObjectCallbackFunction callback);
    using FreeObjectFunction = void(*)(ObjectReference);
    using MarshalObjectFunction = ObjectReference(*)(const void *intptr, uint32 size);

    Class(ClassHolder *class_holder, String name, uint32 size, TypeID type_id, Class *parent_class, EnumFlags<ManagedClassFlags> flags)
        : m_class_holder(class_holder),
          m_name(std::move(name)),
          m_size(size),
          m_type_id(type_id),
          m_parent_class(parent_class),
          m_flags(flags),
          m_new_object_fptr(nullptr),
          m_free_object_fptr(nullptr),
          m_marshal_object_fptr(nullptr)
    {
    }

    Class(const Class &)                = delete;
    Class &operator=(const Class &)     = delete;
    Class(Class &&) noexcept            = delete;
    Class &operator=(Class &&) noexcept = delete;
    ~Class();

    HYP_FORCE_INLINE const String &GetName() const
        { return m_name; }

    HYP_FORCE_INLINE uint32 GetSize() const
        { return m_size; }

    HYP_FORCE_INLINE TypeID GetTypeID() const
        { return m_type_id; }

    HYP_FORCE_INLINE Class *GetParentClass() const
        { return m_parent_class; }

    HYP_FORCE_INLINE EnumFlags<ManagedClassFlags> GetFlags() const
        { return m_flags; }

    HYP_FORCE_INLINE ClassHolder *GetClassHolder() const
        { return m_class_holder; }

    HYP_FORCE_INLINE NewObjectFunction GetNewObjectFunction() const
        { return m_new_object_fptr; }

    HYP_FORCE_INLINE void SetNewObjectFunction(NewObjectFunction new_object_fptr)
        { m_new_object_fptr = new_object_fptr; }

    HYP_FORCE_INLINE FreeObjectFunction GetFreeObjectFunction() const
        { return m_free_object_fptr; }

    HYP_FORCE_INLINE void SetFreeObjectFunction(FreeObjectFunction free_object_fptr)
        { m_free_object_fptr = free_object_fptr; }

    HYP_FORCE_INLINE void SetMarshalObjectFunction(MarshalObjectFunction marshal_object_fptr)
        { m_marshal_object_fptr = marshal_object_fptr; }

    HYP_FORCE_INLINE MarshalObjectFunction GetMarshalObjectFunction() const
        { return m_marshal_object_fptr; }

    HYP_FORCE_INLINE const AttributeSet &GetAttributes() const
        { return m_attributes; }

    HYP_FORCE_INLINE void SetAttributes(AttributeSet &&attributes)
        { m_attributes = std::move(attributes); }

    /*! \brief Check if a method exists by name.
     *
     *  \param method_name The name of the method to check.
     *
     *  \return True if the method exists, otherwise false.
     */
    HYP_FORCE_INLINE bool HasMethod(UTF8StringView method_name) const
        { return m_methods.FindAs(method_name) != m_methods.End(); }

    /*! \brief Get a method by the hash code of its name
     *
     *  \param hash_code The hash code of the method name to get.
     *
     *  \return A pointer to the method object if it exists, otherwise nullptr.
     */
    HYP_FORCE_INLINE Method *GetMethodByHash(HashCode hash_code)
    {
        auto it = m_methods.FindByHash(hash_code);
        if (it == m_methods.End()) {
            return nullptr;
        }

        return &it->second;
    }

    /*! \brief Get a method by the hash code of its name
     *
     *  \param hash_code The hash code of the method name to get.
     *
     *  \return A pointer to the method object if it exists, otherwise nullptr.
     */
    HYP_FORCE_INLINE const Method *GetMethodByHash(HashCode hash_code) const
    {
        auto it = m_methods.FindByHash(hash_code);
        if (it == m_methods.End()) {
            return nullptr;
        }

        return &it->second;
    }

    /*! \brief Get a method by name.
     *
     *  \param method_name The name of the method to get.
     *
     *  \return A pointer to the method object if it exists, otherwise nullptr.
     */
    HYP_FORCE_INLINE Method *GetMethod(UTF8StringView method_name)
    {
        auto it = m_methods.FindAs(method_name);
        if (it == m_methods.End()) {
            return nullptr;
        }

        return &it->second;
    }

    /*! \brief Get a method by name.
     *
     *  \param method_name The name of the method to get.
     *
     *  \return A pointer to the method object if it exists, otherwise nullptr.
     */
    HYP_FORCE_INLINE const Method *GetMethod(UTF8StringView method_name) const
    {
        auto it = m_methods.FindAs(method_name);
        if (it == m_methods.End()) {
            return nullptr;
        }

        return &it->second;
    }

    /*! \brief Add a method to this class.
     *
     *  \param method_name The name of the method to add.
     *  \param method_object The method object to add.
     */
    HYP_FORCE_INLINE void AddMethod(const String &method_name, Method &&method_object)
        { m_methods[method_name] = std::move(method_object); }

    /*! \brief Get all methods of this class.
     *
     *  \return A reference to the map of methods.
     */
    HYP_FORCE_INLINE const HashMap<String, Method> &GetMethods() const
        { return m_methods; }

    /*! \brief Check if a property exists by name.
     *
     *  \param property_name The name of the property to check.
     *
     *  \return True if the property exists, otherwise false.
     */
    HYP_FORCE_INLINE bool HasProperty(UTF8StringView property_name) const
        { return m_properties.FindAs(property_name) != m_properties.End(); }

    /*! \brief Get a property by name.
     *
     *  \param property_name The name of the property to get.
     *
     *  \return A pointer to the property object if it exists, otherwise nullptr.
     */
    HYP_FORCE_INLINE Property *GetProperty(UTF8StringView property_name)
    {
        auto it = m_properties.FindAs(property_name);
        if (it == m_properties.End()) {
            return nullptr;
        }

        return &it->second;
    }

    /*! \brief Get a property by name.
     *
     *  \param property_name The name of the property to get.
     *
     *  \return A pointer to the property object if it exists, otherwise nullptr.
     */
    HYP_FORCE_INLINE const Property *GetProperty(UTF8StringView property_name) const
    {
        auto it = m_properties.FindAs(property_name);
        if (it == m_properties.End()) {
            return nullptr;
        }

        return &it->second;
    }

    /*! \brief Add a property to this class.
     *
     *  \param property_name The name of the property to add.
     *  \param property_object The property object to add.
     */
    HYP_FORCE_INLINE void AddProperty(const String &property_name, Property &&property_object)
        { m_properties[property_name] = std::move(property_object); }

    /*! \brief Get all properties of this class.
     *
     *  \return A reference to the map of properties.
     */
    HYP_FORCE_INLINE const HashMap<String, Property> &GetProperties() const
        { return m_properties; }

    void EnsureLoaded() const;

    /*! \brief Create a new managed object of this class.
     *  The new object will be removed from the managed object cache when the object goes out of scope, allowing for the .NET runtime to collect it.
     *  The returned object will hold a reference to this class instance, so it will need to remain valid for the lifetime of the object.
     *
     *  \return The new managed object.
     */
    HYP_NODISCARD Object NewObject();

    /*! \brief Create a new managed object of this class.
     *  The new object will be removed from the managed object cache when the object goes out of scope, allowing for the .NET runtime to collect it.
     *  The returned object will hold a reference to this class instance, so it will need to remain valid for the lifetime of the object.
     *
     *  \return The new managed object.
     */
    HYP_NODISCARD Object NewObject(const HypClass *hyp_class, void *owning_object_ptr);

    /*! \brief Create a new managed object of this class, but do not allow its lifetime to be managed from the C++ side.
     *  A struct containing the object's GUID and .NET object address will be returned.
     *
     *  \return A struct containing the object's GUID and .NET object address
     */
    HYP_NODISCARD ObjectReference NewManagedObject(void *context_ptr = nullptr, InitializeObjectCallbackFunction callback = nullptr);

    /*! \brief Check if this class has a parent class with the given name.
     *
     *  \param parent_class_name The name of the parent class to check.
     *
     *  \return True if this class has the parent class, otherwise false.
     */
    bool HasParentClass(UTF8StringView parent_class_name) const;

    /*! \brief Check if this class has \ref{parent_class} as a parent class.
     *
     *  \param parent_class The parent class to check.
     *
     *  \return True if this class has the parent class, otherwise false.
     */
    bool HasParentClass(const Class *parent_class) const;

    template <class ReturnType, class... Args>
    ReturnType InvokeStaticMethod(UTF8StringView method_name, Args &&... args)
    {
        auto it = m_methods.FindAs(method_name);
        AssertThrowMsg(it != m_methods.End(), "Method not found");

        const Method &method_object = it->second;
        const Method *method_ptr = &method_object;

        if constexpr (sizeof...(args) != 0) {
            HypData *args_array = (HypData *)StackAlloc(sizeof(HypData) * sizeof...(args));
            const HypData *args_array_ptr[sizeof...(args)];

            FillArgs_HypData(std::make_index_sequence<sizeof...(args)>(), args_array, args_array_ptr, std::forward<Args>(args)...);

            if constexpr (std::is_void_v<ReturnType>) {
                InvokeStaticMethod_Internal(method_ptr, &args_array_ptr[0], nullptr);
            } else {
                HypData return_hyp_data;
                InvokeStaticMethod_Internal(method_ptr, &args_array_ptr[0], &return_hyp_data);

                if (return_hyp_data.IsNull()) {
                    return ReturnType();
                }

                return std::move(return_hyp_data.Get<ReturnType>());
            }
        } else {
            if constexpr (std::is_void_v<ReturnType>) {
                InvokeStaticMethod_Internal(method_ptr, nullptr, nullptr);
            } else {
                HypData return_hyp_data;
                InvokeStaticMethod_Internal(method_ptr, nullptr, &return_hyp_data);

                if (return_hyp_data.IsNull()) {
                    return ReturnType();
                }

                return std::move(return_hyp_data.Get<ReturnType>());
            }
        }
    }

private:
    void InvokeStaticMethod_Internal(const Method *method_ptr, const HypData **args_hyp_data, HypData *out_return_hyp_data);

    String                          m_name;
    uint32                          m_size;
    TypeID                          m_type_id;
    Class                           *m_parent_class;
    EnumFlags<ManagedClassFlags>    m_flags;
    HashMap<String, Method>         m_methods;
    HashMap<String, Property>       m_properties;

    ClassHolder                     *m_class_holder;

    NewObjectFunction               m_new_object_fptr;
    FreeObjectFunction              m_free_object_fptr;
    MarshalObjectFunction           m_marshal_object_fptr;

    AttributeSet                    m_attributes;
};

} // namespace hyperion::dotnet

#endif