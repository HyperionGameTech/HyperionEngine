/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/reflection/TypeId.hpp>
#include <core/utilities/Optional.hpp>

#include <core/containers/String.hpp>

#include <core/reflection/Handle.hpp>

#include <core/Defines.hpp>

namespace hyperion {

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

HYP_API Handle<UIElementFactoryBase> GetEditorUIElementFactory(TypeId typeId);

template <class T>
static Handle<UIElementFactoryBase> GetEditorUIElementFactory()
{
    return GetEditorUIElementFactory(TypeId::ForType<T>());
}

} // namespace hyperion
