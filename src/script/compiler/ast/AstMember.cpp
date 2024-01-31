#include <script/compiler/ast/AstMember.hpp>
#include <script/compiler/ast/AstVariable.hpp>
#include <script/compiler/ast/AstNil.hpp>
#include <script/compiler/ast/AstIdentifier.hpp>
#include <script/compiler/ast/AstCallExpression.hpp>
#include <script/compiler/ast/AstTypeObject.hpp>
#include <script/compiler/ast/AstTypeRef.hpp>
#include <script/compiler/ast/AstModuleAccess.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Compiler.hpp>
#include <script/compiler/SemanticAnalyzer.hpp>
#include <script/compiler/Module.hpp>
#include <script/compiler/Configuration.hpp>
#include <script/compiler/Keywords.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <script/Instructions.hpp>
#include <system/Debug.hpp>
#include <script/Hasher.hpp>

#include <iostream>

namespace hyperion::compiler {

AstMember::AstMember(
    const String &field_name,
    const RC<AstExpression> &target,
    const SourceLocation &location
) : AstExpression(location, ACCESS_MODE_LOAD | ACCESS_MODE_STORE),
    m_field_name(field_name),
    m_target(target),
    m_symbol_type(BuiltinTypes::UNDEFINED),
    m_found_index(~0u),
    m_enable_generic_member_substitution(true)
{
}

void AstMember::Visit(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_target != nullptr);
    m_target->Visit(visitor, mod);

    bool is_proxy_class = false;

    m_access_options = m_target->GetAccessOptions();

    m_target_type = m_target->GetExprType();
    AssertThrow(m_target_type != nullptr);
    m_target_type = m_target_type->GetUnaliased();

    if (mod->IsInScopeOfType(ScopeType::SCOPE_TYPE_NORMAL, ScopeFunctionFlags::REF_VARIABLE_FLAG)) {
        // TODO: implement
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_internal_error,
            m_location
        ));
    }

    const SymbolTypePtr_t original_type = m_target_type;

    // start looking at the target type,
    // iterate through base type
    SymbolTypePtr_t field_type = nullptr;
    SymbolTypeMember member;

    for (UInt depth = 0; field_type == nullptr && m_target_type != nullptr; depth++) {
        AssertThrow(m_target_type != nullptr);
        m_target_type = m_target_type->GetUnaliased();

        if (m_target_type->IsAnyType()) {
            field_type = BuiltinTypes::ANY;

            break;
        }

        if (m_target_type->IsPlaceholderType() || m_target_type->IsGenericParameter()) {
            field_type = BuiltinTypes::PLACEHOLDER;

            break;
        }

        is_proxy_class = m_target_type->IsProxyClass();

        if (is_proxy_class) {
            // load the type by name
            m_proxy_expr.Reset(new AstPrototypeSpecification(
                RC<AstTypeRef>(new AstTypeRef(
                    m_target_type,
                    m_location
                )),
                m_location
            ));

            m_proxy_expr->Visit(visitor, mod);

            // if it is a proxy class,
            // convert thing.DoThing()
            // to ThingProxy.DoThing(thing)
            if (m_target_type->FindMember(m_field_name, member, m_found_index)) {
                field_type = member.type;
            }

            break;
        }

        { // Check for members on the object's prototype
            UInt field_index = ~0u;

            if (m_target_type->FindPrototypeMember(m_field_name, member, field_index)) {
                // only set m_found_index if found in first level.
                // for members from base objects,
                // we load based on hash.
                if (depth == 0) {
                    m_found_index = field_index;
                }

                field_type = member.type;

                break;
            }
        }

        if (auto base = m_target_type->GetBaseType()) {
            m_target_type = base->GetUnaliased();
        } else {
            break;
        }
    }

    AssertThrow(m_target_type != nullptr);

    // Look for members on the object itself (static members)
    if (field_type == nullptr) {
        const AstExpression *value_of = m_target->GetDeepValueOf();
        AssertThrow(value_of != nullptr);

        if (SymbolTypePtr_t held_type = value_of->GetHeldType()) {
            if (held_type->IsAnyType()) {
                field_type = BuiltinTypes::ANY;
            } else if (held_type->IsPlaceholderType() || held_type->IsGenericParameter()) {
                field_type = BuiltinTypes::PLACEHOLDER;
            } else {
                UInt field_index = ~0u;
                UInt depth = 0;

                if (held_type->FindMemberDeep(m_field_name, member, field_index, depth)) {
                    // only set m_found_index if found in first level.
                    // for members from base objects,
                    // we load based on hash.
                    if (depth == 0) {
                        m_found_index = field_index;
                    }

                    field_type = member.type;
                }
            }
        }
    }

    if (field_type != nullptr) {
        field_type = field_type->GetUnaliased();
        AssertThrow(field_type != nullptr);

        if (m_enable_generic_member_substitution && field_type->IsGenericExpressionType()) {
            // @FIXME
            // Cloning the member will unfortunately will break closure captures used
            // in a member function, but it's the best we can do for now.
            // it also will cause too many clones to be made, making a larger bytecode chunk.
            m_override_expr = CloneAstNode(member.expr);
            AssertThrowMsg(m_override_expr != nullptr, "member %s is generic but has no value", m_field_name.Data());

            m_override_expr->Visit(visitor, mod);

            m_symbol_type = m_override_expr->GetExprType();
        } else {
            m_symbol_type = field_type;
        }

        AssertThrow(m_symbol_type != nullptr);

        m_symbol_type = m_symbol_type->GetUnaliased();
    } else {
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_not_a_data_member,
            m_location,
            m_field_name,
            original_type->ToString()
        ));
    }
}

std::unique_ptr<Buildable> AstMember::Build(AstVisitor *visitor, Module *mod)
{
    if (m_override_expr != nullptr) {
        m_override_expr->SetAccessMode(m_access_mode);
        return m_override_expr->Build(visitor, mod);
    }

    // if (m_proxy_expr != nullptr) {
    //     m_proxy_expr->SetAccessMode(m_access_mode);
    //     return m_proxy_expr->Build(visitor, mod);
    // }

    std::unique_ptr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    if (m_proxy_expr != nullptr) {
        chunk->Append(m_proxy_expr->Build(visitor, mod));
    } else {
        AssertThrow(m_target != nullptr);
        chunk->Append(m_target->Build(visitor, mod));
    }

    if (m_found_index == ~0u) {
        // no exact index of member found, have to load from hash.
        const UInt32 hash = hash_fnv_1(m_field_name.Data());

        switch (m_access_mode) {
        case ACCESS_MODE_LOAD:
            chunk->Append(Compiler::LoadMemberFromHash(visitor, mod, hash));
            break;
        case ACCESS_MODE_STORE:
            chunk->Append(Compiler::StoreMemberFromHash(visitor, mod, hash));
            break;
        default:
            AssertThrowMsg(false, "unknown access mode");
        }
    } else {
        switch (m_access_mode) {
        case ACCESS_MODE_LOAD:
            // just load the data member.
            chunk->Append(Compiler::LoadMemberAtIndex(
                visitor,
                mod,
                m_found_index
            ));
            break;
        case ACCESS_MODE_STORE:
            // we are in storing mode, so store to LAST item in the member expr.
            chunk->Append(Compiler::StoreMemberAtIndex(
                visitor,
                mod,
                m_found_index
            ));
            break;
        default:
            AssertThrowMsg(false, "unknown access mode");
        }
    }

    switch (m_access_mode) {
    case ACCESS_MODE_LOAD:
        chunk->Append(BytecodeUtil::Make<Comment>("Load member " + m_field_name));
        break;
    case ACCESS_MODE_STORE:
        chunk->Append(BytecodeUtil::Make<Comment>("Store member " + m_field_name));
        break;
    }

    return chunk;
}

void AstMember::Optimize(AstVisitor *visitor, Module *mod)
{
    if (m_override_expr != nullptr) {
        m_override_expr->Optimize(visitor, mod);

        return;
    }

    if (m_proxy_expr != nullptr) {
        m_proxy_expr->Optimize(visitor, mod);

        // return;
    }

    AssertThrow(m_target != nullptr);

    m_target->Optimize(visitor, mod);

    // TODO: check if the member being accessed is constant and can
    // be optimized
}

RC<AstStatement> AstMember::Clone() const
{
    return CloneImpl();
}

Tribool AstMember::IsTrue() const
{
    if (m_override_expr != nullptr) {
        return m_override_expr->IsTrue();
    }

    // if (m_proxy_expr != nullptr) {
    //     return m_proxy_expr->IsTrue();
    // }

    return Tribool::Indeterminate();
}

bool AstMember::MayHaveSideEffects() const
{
    if (m_override_expr != nullptr) {
        return m_override_expr->MayHaveSideEffects();
    }

    if (m_proxy_expr != nullptr && m_proxy_expr->MayHaveSideEffects()) {
        return true;
    }

    AssertThrow(m_target != nullptr);

    return m_target->MayHaveSideEffects() || m_access_mode == ACCESS_MODE_STORE;
}

SymbolTypePtr_t AstMember::GetExprType() const
{
    return m_symbol_type;
    // if (m_override_expr != nullptr) {
    //     return m_override_expr->GetExprType();
    // }
    // // if (m_proxy_expr != nullptr) {
    // //     return m_proxy_expr->GetExprType();
    // // }

    // AssertThrow(m_symbol_type != nullptr);

    // return m_symbol_type;
}

SymbolTypePtr_t AstMember::GetHeldType() const
{
    if (m_held_type != nullptr) {
        return m_held_type;
    }

    return AstExpression::GetHeldType();
}

const AstExpression *AstMember::GetValueOf() const
{
    if (m_override_expr != nullptr) {
        return m_override_expr->GetValueOf();
    }

    // if (m_proxy_expr != nullptr) {
    //     return m_proxy_expr->GetValueOf();
    // }

    return AstExpression::GetValueOf();
}

const AstExpression *AstMember::GetDeepValueOf() const
{
    if (m_override_expr != nullptr) {
        return m_override_expr->GetDeepValueOf();
    }

    // if (m_proxy_expr != nullptr) {
    //     return m_proxy_expr->GetDeepValueOf();
    // }

    return AstExpression::GetDeepValueOf();
}

AstExpression *AstMember::GetTarget() const
{
    // if (m_override_expr != nullptr) {
    //     return m_override_expr->GetTarget();
    // }

    // if (m_proxy_expr != nullptr) {
    //     return m_proxy_expr->GetTarget();
    // }

    if (m_target != nullptr) {
        return m_target.Get();
    }

    return AstExpression::GetTarget();
}

bool AstMember::IsMutable() const
{
    if (m_override_expr != nullptr) {
        return m_override_expr->IsMutable();
    }

    if (m_proxy_expr != nullptr && !m_proxy_expr->IsMutable()) {
        return false;
    }

    AssertThrow(m_target != nullptr);
    AssertThrow(m_symbol_type != nullptr);

    if (!m_target->IsMutable()) {
        return false;
    }

    return true;
}

} // namespace hyperion::compiler
