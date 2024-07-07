/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <ui/UIDataSource.hpp>

namespace hyperion {

#pragma region UIDataSourceElementFactoryRegistry

UIDataSourceElementFactoryRegistry &UIDataSourceElementFactoryRegistry::GetInstance()
{
    static UIDataSourceElementFactoryRegistry instance { };

    return instance;
}

IUIDataSourceElementFactory *UIDataSourceElementFactoryRegistry::GetFactory(TypeID type_id) const
{
    const auto it = m_element_factories.Find(type_id);

    if (it == m_element_factories.End()) {
        return nullptr;
    }

    return it->second;
}

void UIDataSourceElementFactoryRegistry::RegisterFactory(TypeID type_id, IUIDataSourceElementFactory *element_factory)
{
    m_element_factories.Set(type_id, element_factory);
}

#pragma endregion UIDataSourceElementFactoryRegistry

#pragma region UIDataSourceElementFactoryRegistrationBase

namespace detail {
    
UIDataSourceElementFactoryRegistrationBase::UIDataSourceElementFactoryRegistrationBase(TypeID type_id, IUIDataSourceElementFactory *element_factory)
    : element_factory(element_factory)
{
    UIDataSourceElementFactoryRegistry::GetInstance().RegisterFactory(type_id, element_factory);
}

UIDataSourceElementFactoryRegistrationBase::~UIDataSourceElementFactoryRegistrationBase()
{
    delete element_factory;
}

} // namespace detail

#pragma endregion UIDataSourceElementFactoryRegistrationBase

} // namespace hyperion