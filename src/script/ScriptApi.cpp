#include <script/ScriptApi.hpp>
#include <script/compiler/Parser.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Configuration.hpp>
#include <script/compiler/type-system/BuiltinTypes.hpp>
#include <script/compiler/ast/AstTypeObject.hpp>
#include <script/compiler/ast/AstTypeRef.hpp>
#include <script/vm/VM.hpp>

#include <core/debug/Debug.hpp>

namespace hyperion {

using namespace vm;
using namespace compiler;

ScriptBindingsHolder g_scriptBindings {};

// ScriptBindingsBase

ScriptBindingsBase::ScriptBindingsBase(TypeId typeId)
{
    g_scriptBindings.AddBinding(this);
}

// ScriptBindingsHolder

void ScriptBindingsHolder::AddBinding(ScriptBindingsBase* scriptBindings)
{
    const uint32 index = bindingIndex++;

    Assert(index < maxBindings, "Too many script bindings attached.");

    bindings[index] = scriptBindings;
}

void ScriptBindingsHolder::GenerateAll(scriptapi2::Context& context)
{
    for (auto it = bindings.Begin(); it != bindings.End(); ++it)
    {
        if (*it == nullptr)
        {
            break;
        }

        (*it)->Generate(context);
    }
}

APIInstance::APIInstance()
    : m_vm(nullptr)
{
}

} // namespace hyperion
