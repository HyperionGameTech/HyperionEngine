/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <ui/UIDataSource.hpp>

namespace hyperion {

HYP_API extern const HypClass* GetClass(TypeId typeId);
HYP_API extern bool IsA(const HypClass* hypClass, const HypClass* instanceHypClass);
HYP_API extern int GetSubclassIndex(TypeId baseTypeId, TypeId subclassTypeId);

#pragma region UIDataSource

Handle<UIElementFactoryBase> UIDataSource::GetElementFactoryForType(TypeId typeId) const
{
    auto it = m_elementFactories.Find(typeId);

    if (it != m_elementFactories.End())
    {
        AssertDebug(it->second != nullptr);
        
        return it->second;
    }

    if (it == m_elementFactories.End())
    {
        const HypClass* hypClass = GetClass(typeId);

        if (hypClass != nullptr)
        {
            // slow path (using derived types to look up chain)
            int subclassIndex = -1;
            for (auto jt = m_elementFactories.Begin(); jt != m_elementFactories.End(); ++jt)
            {
                if (::hyperion::IsA(GetClass(jt->first), hypClass))
                {
                    const int currSubclassIndex = GetSubclassIndex(jt->first, typeId);
                    
                    if (currSubclassIndex >= 0)
                    {
                        if (currSubclassIndex < subclassIndex || subclassIndex < 0)
                        {
                            subclassIndex = currSubclassIndex;
                            it = jt;
                        }
                    }
                }
            }
        }
    }

    if (it != m_elementFactories.End())
    {
        AssertDebug(it->second != nullptr);
        
        return it->second;
    }

    return nullptr;
}

#pragma endregion UIDataSource

#pragma region UIElementFactoryRegistry

UIElementFactoryRegistry& UIElementFactoryRegistry::GetInstance()
{
    static UIElementFactoryRegistry instance {};

    return instance;
}

Handle<UIElementFactoryBase> UIElementFactoryRegistry::GetFactory(TypeId typeId)
{
    auto it = m_elementFactories.Find(typeId);

    if (it == m_elementFactories.End())
    {
        const HypClass* hypClass = GetClass(typeId);

        if (hypClass != nullptr)
        {
            // slow path (using derived types to look up chain)
            int subclassIndex = -1;
            for (auto jt = m_elementFactories.Begin(); jt != m_elementFactories.End(); ++jt)
            {
                if (IsA(GetClass(jt->first), hypClass))
                {
                    const int currSubclassIndex = GetSubclassIndex(jt->first, typeId);
                    if (currSubclassIndex < subclassIndex || (currSubclassIndex > 0 && subclassIndex < 0))
                    {
                        subclassIndex = currSubclassIndex;
                        it = jt;
                    }
                }
            }
        }
    }
    
    if (it == m_elementFactories.End())
    {
        return nullptr;
    }

    FactoryInstance& factoryInstance = it->second;

    if (!factoryInstance.factoryInstance)
    {
        factoryInstance.factoryInstance = factoryInstance.makeFactoryFunction();
    }

    return factoryInstance.factoryInstance;
}

void UIElementFactoryRegistry::RegisterFactory(TypeId typeId, Handle<UIElementFactoryBase> (*makeFactoryFunction)(void))
{
    m_elementFactories.Set(typeId, FactoryInstance { makeFactoryFunction, nullptr });
}

#pragma endregion UIElementFactoryRegistry

#pragma region UIElementFactoryRegistrationBase

UIElementFactoryRegistrationBase::UIElementFactoryRegistrationBase(TypeId typeId, Handle<UIElementFactoryBase> (*makeFactoryFunction)(void))
    : m_makeFactoryFunction(makeFactoryFunction)
{
    UIElementFactoryRegistry::GetInstance().RegisterFactory(typeId, makeFactoryFunction);
}

UIElementFactoryRegistrationBase::~UIElementFactoryRegistrationBase()
{
}

#pragma endregion UIElementFactoryRegistrationBase

} // namespace hyperion
