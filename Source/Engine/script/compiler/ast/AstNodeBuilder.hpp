#pragma once

#include <script/compiler/ast/AstArgument.hpp>
#include <script/compiler/ast/AstModuleAccess.hpp>
#include <script/compiler/type-system/SymbolType.hpp>

#include <Core/containers/String.hpp>

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

    RC<AstModuleAccess> Build(const RC<AstExpression>& expr);

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

    RC<AstExpression> Call(const Array<RC<AstArgument>>& args);

    const String& GetName() const
    {
        return m_name;
    }

private:
    String m_name;
    ModuleBuilder* m_parent;
};

} // namespace Hyperion
