/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_EDITOR_UI_HPP
#define HYPERION_EDITOR_UI_HPP

#include <core/utilities/TypeID.hpp>
#include <core/utilities/Optional.hpp>

#include <core/containers/String.hpp>

#include <core/memory/RefCountedPtr.hpp>

#include <core/Defines.hpp>

namespace hyperion {

class Node;
class HypProperty;

struct EditorNodePropertyRef
{
    String              title;
    Optional<String>    description;
    Weak<Node>          node;
    HypProperty         *property = nullptr;
};

class IUIDataSourceElementFactory;

HYP_API IUIDataSourceElementFactory *GetEditorUIDataSourceElementFactory(TypeID type_id);

template <class T>
static IUIDataSourceElementFactory *GetEditorUIDataSourceElementFactory()
{
    return GetEditorUIDataSourceElementFactory(TypeID::ForType<T>());
}

} // namespace hyperion

#endif