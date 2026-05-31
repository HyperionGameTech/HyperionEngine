/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <UIPch.hpp>

#include <UI/UIDataSource.hpp>
#include <UI/UIObject.hpp>

#include <Core/Reflection/TypeInfo.hpp>

#include <UIDataSource.generated.inl>

namespace Hyperion {

CORE_API extern const Class* GetClass(const TypeId& typeId);
CORE_API extern bool IsA(const Class* cls, const Class* instanceClass);
CORE_API extern int GetSubclassIndex(TypeId baseTypeId, TypeId subclassTypeId);

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
        const Class* cls = Hyperion::GetClass(typeId);

        if (cls != nullptr)
        {
            // slow path (using derived types to look up chain)
            int subclassIndex = -1;
            for (auto jt = m_elementFactories.Begin(); jt != m_elementFactories.End(); ++jt)
            {
                if (::Hyperion::IsA(GetClass(jt->first), cls))
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

Handle<UIElementFactoryBase> UIElementFactoryRegistry::GetFactory(const TypeInfo& typeInfo)
{
    if (!typeInfo.IsValid())
    {
        return nullptr;
    }

    auto it = m_elementFactories.Find(typeInfo.id);

    if (it == m_elementFactories.End())
    {
        const Class* cls = typeInfo.GetClass();

        if (cls != nullptr)
        {
            // slow path (using derived types to look up chain and find the most derived type's factory)
            int subclassIndex = -1;
            for (auto jt = m_elementFactories.Begin(); jt != m_elementFactories.End(); ++jt)
            {
                if (IsA(GetClass(jt->first), cls))
                {
                    const int currSubclassIndex = GetSubclassIndex(jt->first, typeInfo.id);
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

void UIElementFactoryRegistry::Shutdown()
{
    m_elementFactories.Clear();
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

} // namespace Hyperion
