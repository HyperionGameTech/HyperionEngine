/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <EditorPch.hpp>

#include <Editor/ui/EditorUI.hpp>

#include <UI/UIDataSource.hpp>

#include <Core/reflection/TypeInfo.hpp>

namespace Hyperion {

EDITOR_API HYP_DECLARE_LOG_CHANNEL(Editor);

HYP_EXPORT Handle<UIElementFactoryBase> GetEditorUIElementFactory(const TypeInfo& typeInfo)
{
    Handle<UIElementFactoryBase> factory = UIElementFactoryRegistry::GetInstance().GetFactory(typeInfo);

    if (!factory)
    {
        if (const Class* cls = typeInfo.GetClass())
        {
            factory = UIElementFactoryRegistry::GetInstance().GetFactory(TypeOf<BoxedValue>());
        }

        if (!factory)
        {
            HYP_LOG(Editor, Warning, "No factory registered for type: {}", typeInfo.name);

            return nullptr;
        }
    }

    return factory;
}

} // namespace Hyperion
