#include <Lang/Compiler/Ast/AstNodeBuilder.hpp>
#include <Lang/Compiler/Ast/AstCallExpression.hpp>
#include <Lang/Compiler/Ast/AstVariable.hpp>

#include <Core/Debug/Debug.hpp>

namespace Hyperion {

ModuleBuilder AstNodeBuilder::Module(const String& name)
{
    return ModuleBuilder(name);
}

ModuleBuilder::ModuleBuilder(
    const String& name)
    : m_name(name),
      m_parent(nullptr)
{
}

ModuleBuilder::ModuleBuilder(
    const String& name,
    ModuleBuilder* parent)
    : m_name(name),
      m_parent(parent)
{
}

ModuleBuilder ModuleBuilder::Module(const String& name)
{
    return ModuleBuilder(name, this);
}

FunctionBuilder ModuleBuilder::Function(const String& name)
{
    return FunctionBuilder(name, this);
}

Handle<AstModuleAccess> ModuleBuilder::Build(const Handle<AstExpression>& expr)
{
    if (m_parent != nullptr)
    {
        return MakeHandle<AstModuleAccess>(
            m_name,
            m_parent->Build(expr),
            SourceLocation::Eof());
    }
    else
    {
        return MakeHandle<AstModuleAccess>(
            m_name,
            expr,
            SourceLocation::Eof());
    }
}

FunctionBuilder::FunctionBuilder(
    const String& name)
    : m_name(name),
      m_parent(nullptr)
{
}

FunctionBuilder::FunctionBuilder(
    const String& name,
    ModuleBuilder* parent)
    : m_name(name),
      m_parent(parent)
{
}

Handle<AstExpression> FunctionBuilder::Call(const Array<Handle<AstArgument>>& args)
{
    Handle<AstCallExpression> call = MakeHandle<AstCallExpression>(
        MakeHandle<AstVariable>(
            m_name,
            SourceLocation::Eof()),
        args,
        false,
        SourceLocation::Eof());

    if (m_parent != nullptr)
    {
        return m_parent->Build(call);
    }
    else
    {
        return call;
    }
}

} // namespace Hyperion
