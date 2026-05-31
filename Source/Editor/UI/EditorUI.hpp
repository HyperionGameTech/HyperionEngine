/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Utilities/Optional.hpp>

#include <Core/Containers/String.hpp>

#include <Core/Reflection/Handle.hpp>
#include <Core/Reflection/TypeInfoFwd.hpp>

#include <Core/Defines.hpp>

namespace Hyperion {

class Node;
class Property;

struct EditorNodePropertyRef
{
    String title;
    Optional<String> description;
    WeakHandle<Node> node;
    Property* property = nullptr;
};

class UIElementFactoryBase;

EDITOR_API Handle<UIElementFactoryBase> GetEditorUIElementFactory(const TypeInfo& typeInfo);

template <class T>
static Handle<UIElementFactoryBase> GetEditorUIElementFactory()
{
    return GetEditorUIElementFactory(TypeOf<T>());
}

} // namespace Hyperion
