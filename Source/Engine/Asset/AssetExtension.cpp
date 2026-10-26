/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <AssetPch.hpp>

#include <Asset/AssetExtension.hpp>
#include <Asset/AssetObject.hpp>

#include <Core/Reflection/Class.hpp>
#include <Core/Reflection/ClassRegistry.hpp>
#include <Core/Reflection/ClassAttribute.hpp>

#include <Core/Reflection/BoxedValue.hpp>

#include <Core/Utilities/ForEach.hpp>

#include <Core/Logging/Logger.hpp>

#include <AssetExtension.generated.inl>

namespace Hyperion {

Handle<AssetObject> AssetExtension::GetOwner() const
{
    return m_owner.Lock();
}

void AssetExtension::NotifyChanged()
{
    if (Handle<AssetObject> owner = m_owner.Lock(); owner.IsValid())
    {
        owner->OnExtensionChanged(*this);
        owner->MarkDirty();
    }
}

bool AssetExtension::CanExtend(const Class* extensionClass, const Class* assetClass)
{
    if (!extensionClass || !assetClass || extensionClass->IsAbstract() || !extensionClass->IsDerivedFrom(AssetExtension::StaticClass()))
    {
        return false;
    }

    const ClassAttributeValue& extendsAttribute = extensionClass->GetAttributeDeep("extendsasset"_sh);

    if (!extendsAttribute.IsValid())
    {
        return false;
    }

    const String extendedClassName(extendsAttribute.GetString());

    for (const Class* cls = assetClass; cls != nullptr; cls = cls->GetParent())
    {
        if (String(cls->GetName().LookupString()) == extendedClassName)
        {
            return true;
        }
    }

    return false;
}

Handle<AssetExtension> AssetExtension::Create(const Class* extensionClass)
{
    BoxedValue boxed;

    if (!extensionClass || !extensionClass->CreateInstance(boxed) || !boxed.Is<Handle<AssetExtension>>())
    {
        HYP_LOG(Assets, Error, "Failed to create asset extension {}", extensionClass ? extensionClass->GetName() : Name::Invalid());

        return Handle<AssetExtension>::empty;
    }

    return boxed.Get<Handle<AssetExtension>>();
}

Array<const Class*> AssetExtension::GetExtensionClasses(const Class* assetClass)
{
    Array<const Class*> extensionClasses;

    auto collect = [assetClass, &extensionClasses](const Class* cls) -> IterationResult
    {
        if (CanExtend(cls, assetClass))
        {
            extensionClasses.PushBack(cls);
        }

        return IterationResult::CONTINUE;
    };

    ClassRegistry::GetInstance().ForEachClass(collect);

    return extensionClasses;
}

} // namespace Hyperion
