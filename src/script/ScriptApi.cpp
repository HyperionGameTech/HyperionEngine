#include <script/ScriptApi.hpp>
#include <script/compiler/Parser.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Configuration.hpp>
#include <script/compiler/type-system/BuiltinTypes.hpp>
#include <script/compiler/ast/AstTypeObject.hpp>
#include <script/compiler/ast/AstTypeRef.hpp>
#include <script/vm/VM.hpp>
#include <script/Hasher.hpp>

#include <system/Debug.hpp>

namespace hyperion {

using namespace vm;
using namespace compiler;

ScriptBindingsHolder g_script_bindings { };

// ScriptBindingsBase

ScriptBindingsBase::ScriptBindingsBase(TypeID type_id)
{
    g_script_bindings.AddBinding(this);
}

// ScriptBindingsHolder

void ScriptBindingsHolder::AddBinding(ScriptBindingsBase *script_bindings)
{
    const UInt index = binding_index++;

    AssertThrowMsg(index < max_bindings, "Too many script bindings attached.");

    bindings[index] = script_bindings;
}

void ScriptBindingsHolder::GenerateAll(scriptapi2::Context &context)
{
    for (auto it = bindings.Begin(); it != bindings.End(); ++it) {
        if (*it == nullptr) {
            break;
        }

        (*it)->Generate(context);
    }
}

APIInstance::APIInstance(const SourceFile &source_file)
    : m_source_file(source_file),
      m_vm(nullptr)
{
}

} // namespace hyperion
