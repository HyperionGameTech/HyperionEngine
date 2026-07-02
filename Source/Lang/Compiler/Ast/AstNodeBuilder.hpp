#pragma once

#include <Lang/Compiler/Ast/AstArgument.hpp>
#include <Lang/Compiler/Ast/AstModuleAccess.hpp>
#include <Lang/Compiler/TypeSystem/SymbolType.hpp>

#include <Core/Containers/String.hpp>

namespace Hyperion {

class AstVisitor;
class Module;
class ModuleBuilder;
class FunctionBuilder;

class AstNodeBuilder
{
public:
    ModuleBuilder Module(const String& name);
};

class ModuleBuilder
{
public:
    ModuleBuilder(
        const String& name);

    ModuleBuilder(
        const String& name,
        ModuleBuilder* parent);

    const String& GetName() const
    {
        return m_name;
    }

    ModuleBuilder Module(const String& name);
    FunctionBuilder Function(const String& name);

    SharedPtr<AstModuleAccess> Build(const SharedPtr<AstExpression>& expr);

private:
    String m_name;
    ModuleBuilder* m_parent;
};

class FunctionBuilder
{
public:
    FunctionBuilder(
        const String& name);

    FunctionBuilder(
        const String& name,
        ModuleBuilder* parent);

    SharedPtr<AstExpression> Call(const Array<SharedPtr<AstArgument>>& args);

    const String& GetName() const
    {
        return m_name;
    }

private:
    String m_name;
    ModuleBuilder* m_parent;
};

} // namespace Hyperion
