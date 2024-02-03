#include <dotnet/DotNetSystem.hpp>
#include <dotnet/Assembly.hpp>
#include <dotnet/Class.hpp>

namespace hyperion {
namespace dotnet {

Assembly::Assembly()
{
}

Assembly::~Assembly()
{
}

// ClassHolder

ClassHolder::ClassHolder()
    : m_invoke_method_fptr(nullptr)
{
}

Class *ClassHolder::GetOrCreateClassObject(Int32 type_hash, const char *type_name)
{
    auto it = m_class_objects.Find(type_hash);

    if (it != m_class_objects.End()) {
        return it->second.Get();
    }

    it = m_class_objects.Insert(type_hash, UniquePtr<Class>::Construct(this, type_name)).first;

    return it->second.Get();
}

Class *ClassHolder::FindClassByName(const char *type_name)
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