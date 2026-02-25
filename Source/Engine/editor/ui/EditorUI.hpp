/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/utilities/Optional.hpp>

#include <Core/containers/String.hpp>

#include <Core/reflection/Handle.hpp>
#include <Core/reflection/TypeInfoFwd.hpp>

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

HYP_API Handle<UIElementFactoryBase> GetEditorUIElementFactory(const TypeInfo& typeInfo);

template <class T>
static Handle<UIElementFactoryBase> GetEditorUIElementFactory()
{
    return GetEditorUIElementFactory(TypeOf<T>());
}

} // namespace Hyperion
