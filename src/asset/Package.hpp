#ifndef HYPERION_PACKAGE_HPP
#define HYPERION_PACKAGE_HPP

#include <core/Handle.hpp>
#include <core/memory/UniquePtr.hpp>

namespace hyperion {

class PackageRoot
{
public:
    PackageRoot(UniquePtr<HandleBase> &&root_object)
        : m_root_object(std::move(root_object))
    {
    }

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool IsValid() const
        { return m_root_object != nullptr; }

private:
    UniquePtr<HandleBase>   m_root_object;
};

class HYP_API Package
{
public:
    Package();

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool IsValid() const
        { return m_root.IsValid(); }
    
private:
    PackageRoot m_root;
};

} // namespace hyperion

#endif