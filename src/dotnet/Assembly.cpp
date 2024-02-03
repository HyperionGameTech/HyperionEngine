#include <dotnet/DotNetSystem.hpp>
#include <dotnet/Assembly.hpp>
#include <dotnet/ClassObject.hpp>

namespace hyperion {
namespace dotnet {

Assembly::Assembly()
{
}

Assembly::~Assembly()
{
}

// ClassObjectHolder

ClassObjectHolder::ClassObjectHolder()
    : m_invoke_method_fptr(nullptr)
{
}

ClassObject *ClassObjectHolder::GetOrCreateClassObject(Int32 type_hash, const char *type_name)
{
    auto it = m_class_objects.Find(type_hash);

    if (it != m_class_objects.End()) {
        return it->second.Get();
    }

    it = m_class_objects.Insert(type_hash, UniquePtr<ClassObject>::Construct(this, type_name)).first;

    return it->second.Get();
}

ClassObject *ClassObjectHolder::FindClassByName(const char *type_name)
{
    for (auto &pair : m_class_objects) {
        if (pair.second->GetName() == type_name) {
            return pair.second.Get();
        }
    }

    return nullptr;
}

} // namespace dotnet
} // namespace hyperion