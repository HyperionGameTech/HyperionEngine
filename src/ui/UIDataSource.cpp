/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <ui/UIDataSource.hpp>

namespace hyperion {

#pragma region UIElementFactoryRegistry

UIElementFactoryRegistry &UIElementFactoryRegistry::GetInstance()
{
    static UIElementFactoryRegistry instance { };

    return instance;
}

UIElementFactoryBase *UIElementFactoryRegistry::GetFactory(TypeID type_id)
{
    auto it = m_element_factories.Find(type_id);

    if (it == m_element_factories.End()) {
        return nullptr;
    }

    FactoryInstance &factory_instance = it->second;

    if (!factory_instance.factory_instance) {
        factory_instance.factory_instance = factory_instance.make_factory_function();
    }

    return factory_instance.factory_instance.Get();
}

void UIElementFactoryRegistry::RegisterFactory(TypeID type_id, RC<UIElementFactoryBase>(*make_factory_function)(void))
{
    m_element_factories.Set(type_id, FactoryInstance { make_factory_function, nullptr });
}

#pragma endregion UIElementFactoryRegistry

#pragma region UIElementFactoryRegistrationBase

namespace detail {
    
UIElementFactoryRegistrationBase::UIElementFactoryRegistrationBase(TypeID type_id, RC<UIElementFactoryBase>(*make_factory_function)(void))
    : make_factory_function(make_factory_function)
{
    UIElementFactoryRegistry::GetInstance().RegisterFactory(type_id, make_factory_function);
}

UIElementFactoryRegistrationBase::~UIElementFactoryRegistrationBase()
{
}

} // namespace detail

#pragma endregion UIElementFactoryRegistrationBase

} // namespace hyperion