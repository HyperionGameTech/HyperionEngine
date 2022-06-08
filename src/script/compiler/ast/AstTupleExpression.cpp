#include <script/compiler/ast/AstTupleExpression.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Module.hpp>
#include <script/compiler/Configuration.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <system/debug.h>

namespace hyperion {
namespace compiler {

AstTupleExpression::AstTupleExpression(const std::vector<std::shared_ptr<AstArgument>> &members,
    const SourceLocation &location)
    : AstExpression(location, ACCESS_MODE_LOAD),
      m_members(members)
{
}

void AstTupleExpression::Visit(AstVisitor *visitor, Module *mod)
{
    std::vector<SymbolMember_t> member_types;
    std::vector<GenericInstanceTypeInfo::Arg> generic_param_types;

    for (size_t i = 0; i < m_members.size(); i++) {
        const auto &member = m_members[i];
        AssertThrow(member != nullptr);

        member->Visit(visitor, mod);

        std::string mem_name;
        
        if (member->IsNamed()) {
            mem_name = member->GetName();
        } else {
            mem_name = std::to_string(i);
        }

        // TODO find a better way to set up default assignment for members!
        // we can't  modify default values of types.
        //mem_type.SetDefaultValue(mem->GetAssignment());

        member_types.push_back(std::make_tuple(
            mem_name,
            member->GetSymbolType(),
            member->GetExpr()
        ));

        generic_param_types.push_back(GenericInstanceTypeInfo::Arg {
            mem_name,
            member->GetSymbolType(),
            nullptr
        });
    }

    m_symbol_type = SymbolType::Extend(SymbolType::GenericInstance(
        BuiltinTypes::TUPLE, 
        GenericInstanceTypeInfo {
            generic_param_types
        }
    ), member_types);
    
    // register the tuple type
    visitor->GetCompilationUnit()->RegisterType(m_symbol_type);
}

std::unique_ptr<Buildable> AstTupleExpression::Build(AstVisitor *visitor, Module *mod)
{
    AssertThrow(visitor != nullptr);
    AssertThrow(mod != nullptr);

    AssertThrow(m_symbol_type != nullptr);
    AssertThrow(m_symbol_type->GetDefaultValue() != nullptr);

    return m_symbol_type->GetDefaultValue()->Build(visitor, mod);
}

void AstTupleExpression::Optimize(AstVisitor *visitor, Module *mod)
{
    for (auto &member : m_members) {
        AssertThrow(member != nullptr);

        member->Optimize(visitor, mod);
    }
}

std::shared_ptr<AstStatement> AstTupleExpression::Clone() const
{
    return CloneImpl();
}

Tribool AstTupleExpression::IsTrue() const
{
    return Tribool::True();
}

bool AstTupleExpression::MayHaveSideEffects() const
{
    bool side_effects = false;
    for (const auto &member : m_members) {
        AssertThrow(member != nullptr);

        if (member->MayHaveSideEffects()) {
            side_effects = true;
            break;
        }
    }

    return side_effects;
}

SymbolTypePtr_t AstTupleExpression::GetSymbolType() const
{
    AssertThrow(m_symbol_type != nullptr);
    return m_symbol_type;
}

} // namespace compiler
} // namespace hyperion
