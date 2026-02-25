/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

namespace Hyperion {

namespace functional {

class IScriptableDelegate;

/*! \brief A delegate that can be bound to a managed .NET object.
 *  \details This delegate can be bound to a managed .NET object, allowing the delegate have its behavior defined in script code.
 *  \tparam ReturnType The return type of the delegate.
 *  \tparam Args The argument types of the delegate. */
template <class ReturnType, class... Args>
class ScriptableDelegate;

} // namespace functional

using functional::IScriptableDelegate;
using functional::ScriptableDelegate;

} // namespace Hyperion
