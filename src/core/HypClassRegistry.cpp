/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/HypClassRegistry.hpp>
#include <core/HypClass.hpp>

#include <Engine.hpp>

namespace hyperion {

#pragma region NullHypClassInstance
namespace detail {
class NullHypClassInstance : public HypClass
{
public:
    static NullHypClassInstance &GetInstance()
    {
        static NullHypClassInstance instance { };

        return instance;
    }

    NullHypClassInstance()
        : HypClass(TypeID::Void(), { })
    {
    }

    virtual ~NullHypClassInstance() = default;

    virtual Name GetName() const override
    {
        static const Name name = NAME("NullClass");

        return name;
    }
};
} // namespace detail

#pragma endregion NullHypClassInstance

#pragma region HypClassRegistry

HypClassRegistry &HypClassRegistry::GetInstance()
{
    static HypClassRegistry instance;

    return instance;
}

const HypClass *HypClassRegistry::GetClass(TypeID type_id)
{
    const auto it = m_registered_classes.Find(type_id);

    if (it == m_registered_classes.End()) {
        return &GetNullHypClassInstance();
    }

    return it->second;
}

void HypClassRegistry::RegisterClass(TypeID type_id, HypClass *hyp_class)
{
    AssertThrow(hyp_class != nullptr);

    const auto it = m_registered_classes.Find(type_id);
    AssertThrowMsg(it == m_registered_classes.End(), "Class already registered for type: %s", *hyp_class->GetName());

    m_registered_classes.Set(type_id, hyp_class);
}

const HypClass &HypClassRegistry::GetNullHypClassInstance()
{
    static const detail::NullHypClassInstance null_hyp_class_instance { };

    return null_hyp_class_instance;
}

#pragma endregion HypClassRegistry

#pragma region HypClassRegistrationBase

namespace detail {
    
HypClassRegistrationBase::HypClassRegistrationBase(TypeID type_id, HypClass *hyp_class)
    : m_type_id(type_id),
      m_hyp_class(hyp_class)
{
    HypClassRegistry::GetInstance().RegisterClass(type_id, hyp_class);
}

} // namespace detail

#pragma endregion HypClassRegistrationBase

} // namespace hyperion