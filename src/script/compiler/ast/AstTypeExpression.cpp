#include <script/compiler/ast/AstTypeExpression.hpp>
#include <script/compiler/ast/AstNil.hpp>
#include <script/compiler/ast/AstObject.hpp>
#include <script/compiler/ast/AstArrayExpression.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Keywords.hpp>
#include <script/compiler/Module.hpp>
#include <script/compiler/Configuration.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <script/Hasher.hpp>
#include <system/debug.h>

namespace hyperion::compiler {

AstTypeExpression::AstTypeExpression(
    const std::string &name,
    const std::shared_ptr<AstPrototypeSpecification> &base_specification,
    const std::vector<std::shared_ptr<AstVariableDeclaration>> &members,
    const std::vector<std::shared_ptr<AstVariableDeclaration>> &static_members,
    const SourceLocation &location)
    : AstExpression(location, ACCESS_MODE_LOAD),
      m_name(name),
      m_base_specification(base_specification),
      m_members(members),
      m_static_members(static_members),
      m_num_members(0)
{
}

void AstTypeExpression::Visit(AstVisitor *visitor, Module *mod)
{   
    AssertThrow(visitor != nullptr && mod != nullptr);

    auto prototype_type = SymbolType::Object(
        m_name + "Instance", // Prototype type
        {},
        BuiltinTypes::OBJECT
    );

    // TODO: allow custom bases (which would have to extend Type somewhere)
    auto base_type = BuiltinTypes::CLASS_TYPE;

    m_symbol_type = SymbolType::Extend(
        m_name,
        base_type,
        {}
    );

    m_expr.reset(new AstTypeObject(
        m_symbol_type,
        nullptr, // prototype - TODO
        m_location
    ));

    mod->m_scopes.Open(Scope(SCOPE_TYPE_NORMAL, 0));

    // add symbol type to be usable within members
    mod->m_scopes.Top().GetIdentifierTable().AddSymbolType(m_symbol_type);

    for (auto &outside_member : m_outside_members) {
        outside_member->Visit(visitor, mod);
    }

    // check if one with the name $proto already exists.
    bool proto_found = false;
    bool base_found = false;

    for (const auto &mem : m_static_members) {
        AssertThrow(mem != nullptr);

        if (mem->GetName() == "$proto") {
            proto_found = true;
        } else if (mem->GetName() == "base") {
            base_found = true;
        }
    }

    if (!proto_found) { // no custom '$proto' member, add default.
        m_symbol_type->AddMember(SymbolMember_t {
            "$proto",
            prototype_type,
            std::shared_ptr<AstTypeObject>(new AstTypeObject(
                prototype_type,
                nullptr,
                m_location
            ))
        });
    }

    if (!base_found) { // no custom 'base' member, add default
        m_symbol_type->AddMember(SymbolMember_t {
            "base",
            base_type,
            std::shared_ptr<AstTypeObject>(new AstTypeObject(
                base_type,
                nullptr,
                m_location
            ))
        });
    }

    // ===== STATIC DATA MEMBERS ======

    // open the scope for static data members
    mod->m_scopes.Open(Scope(SCOPE_TYPE_TYPE_DEFINITION, 0));

    for (const auto &mem : m_static_members) {
        AssertThrow(mem != nullptr);
        mem->Visit(visitor, mod);

        std::string mem_name = mem->GetName();

        AssertThrow(mem->GetIdentifier() != nullptr);
        SymbolTypePtr_t mem_type = mem->GetIdentifier()->GetSymbolType();
        
        m_symbol_type->AddMember(SymbolMember_t {
            mem_name,
            mem_type,
            mem->GetRealAssignment()
        });
    }

    // close the scope for static data members
    mod->m_scopes.Close();

    // ===== INSTANCE DATA MEMBERS =====

    // open the scope for data members
    mod->m_scopes.Open(Scope(SCOPE_TYPE_TYPE_DEFINITION, 0));

    for (const auto &mem : m_members) {
        if (mem != nullptr) {
            if (mem->GetName() == "construct") {//m_name) { // it is the constructor
                mem->SetName("$construct");
            }

            mem->Visit(visitor, mod);

            AssertThrow(mem->GetIdentifier() != nullptr);

            std::string mem_name = mem->GetName();
            SymbolTypePtr_t mem_type = mem->GetIdentifier()->GetSymbolType();
            
            prototype_type->AddMember(SymbolMember_t(
                mem_name,
                mem_type,
                mem->GetRealAssignment()
            ));
        }
    }

    // close the scope for data members
    mod->m_scopes.Close();

    // close scope for Self var
    mod->m_scopes.Close();

    m_expr->Visit(visitor, mod);
}

std::unique_ptr<Buildable> AstTypeExpression::Build(AstVisitor *visitor, Module *mod)
{
    std::unique_ptr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    // for (auto &outside_member : m_outside_members) {
    //     AssertThrow(outside_member != nullptr);

    //     chunk->Append(outside_member->Build(visitor, mod));
    // }

    AssertThrow(m_expr != nullptr);
    chunk->Append(m_expr->Build(visitor, mod));

    return chunk;
}

void AstTypeExpression::Optimize(AstVisitor *visitor, Module *mod)
{
    // for (auto &outside_member : m_outside_members) {
    //     AssertThrow(outside_member != nullptr);
    
    //     outside_member->Optimize(visitor, mod);
    // }

    AssertThrow(m_expr != nullptr);
    m_expr->Optimize(visitor, mod);
}

Pointer<AstStatement> AstTypeExpression::Clone() const
{
    return CloneImpl();
}

bool AstTypeExpression::IsLiteral() const
{
    return false;
}

Tribool AstTypeExpression::IsTrue() const
{
    return Tribool::True();
}

bool AstTypeExpression::MayHaveSideEffects() const
{
    return true;
}

SymbolTypePtr_t AstTypeExpression::GetExprType() const
{
    AssertThrow(m_expr != nullptr);
    return m_expr->GetExprType();
}

const AstExpression *AstTypeExpression::GetValueOf() const
{
    AssertThrow(m_expr != nullptr);
    return m_expr->GetValueOf();
}

const AstExpression *AstTypeExpression::GetDeepValueOf() const
{
    AssertThrow(m_expr != nullptr);
    return m_expr->GetDeepValueOf();
}

} // namespace hyperion::compiler
