/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <EditorPch.hpp>

#include <editor/EditorCommand.hpp>

#include <Core/reflection/Class.hpp>

#include <EditorCommand.generated.inl>

namespace Hyperion {

String EditorCommandBase::GetText() const
{
    return InstanceClass()->GetName().LookupString();
}

} // namespace Hyperion
