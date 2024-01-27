#include <script/compiler/ast/AstNewExpression.hpp>
#include <script/compiler/ast/AstMember.hpp>
#include <script/compiler/ast/AstHasExpression.hpp>
#include <script/compiler/ast/AstTernaryExpression.hpp>
#include <script/compiler/ast/AstIdentifier.hpp>
#include <script/compiler/ast/AstTypeObject.hpp>
#include <script/compiler/ast/AstMember.hpp>
#include <script/compiler/ast/AstNil.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Module.hpp>
#include <script/compiler/Compiler.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <system/Debug.hpp>
#include <util/UTF8.hpp>
#include <script/Hasher.hpp>

namespace hyperion::compiler {

AstNewExpression::AstNewExpression(
    const RC<AstPrototypeSpecification> &proto,
    const RC<AstArgumentList> &arg_list,
    bool enable_constructor_call,
    const SourceLocation &location
) : AstExpression(location, ACCESS_MODE_LOAD),
    m_proto(proto),
    m_arg_list(arg_list),
    m_enable_constructor_call(enable_constructor_call)
{
}

void AstNewExpression::Visit(AstVisitor *visitor, Module *mod)
{
    AssertThrow(visitor != nullptr);
    AssertThrow(mod != nullptr);

    AssertThrow(m_proto != nullptr);
    m_proto->Visit(visitor, mod);

    if (m_arg_list != nullptr) {
        AssertThrowMsg(m_enable_constructor_call, "Args provided for non-constructor call new expr");
    }

    auto *value_of = m_proto->GetDeepValueOf();
    AssertThrow(value_of != nullptr);
    
    m_instance_type = BuiltinTypes::UNDEFINED;
    m_prototype_type = BuiltinTypes::UNDEFINED;

    SymbolTypePtr_t expr_type = value_of->GetExprType();
    AssertThrow(expr_type != nullptr);
    expr_type = expr_type->GetUnaliased();

    if (SymbolTypePtr_t held_type = value_of->GetHeldType()) {
        m_instance_type = held_type->GetUnaliased();
        m_object_value = m_proto->GetDefaultValue(); // may be nullptr
        m_prototype_type = m_proto->GetPrototypeType();
    } else {
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_not_a_type,
            m_location,
            expr_type->ToString()
        ));

        return;
    }

    if (m_prototype_type == nullptr) {
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_type_missing_prototype,
            m_location,
            expr_type->ToString()
        ));

        return;
    }

    // if (m_instance_type != nullptr && m_instance_type->IsProxyClass()) {
    //     visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
    //         LEVEL_ERROR,
    //         Msg_proxy_class_cannot_be_constructed,
    //         m_location
    //     ));

    //     return;
    // }

    if (m_enable_constructor_call) {
        static constexpr const char *construct_method_name = "$construct";
        static constexpr const char *temp_var_name = "__$temp_new_target";

        const bool is_any = m_prototype_type->IsAnyType();
        const bool is_placeholder = m_prototype_type->IsPlaceholderType();
        const bool has_construct_member = m_prototype_type->FindMember(construct_method_name) != nullptr;

        if (is_any || is_placeholder || has_construct_member) {
            m_constructor_block.Reset(new AstBlock(m_location));

            if (has_construct_member) {
                 m_constructor_call.Reset(new AstMemberCallExpression(
                    construct_method_name,
                    RC<AstNewExpression>(new AstNewExpression(
                        CloneAstNode(m_proto),
                        nullptr, // no args
                        false, // do not enable constructor call
                        m_location
                    )),
                    m_arg_list,
                    m_location
                ));
            } else {
                // conditionally lookup member with the name $construct and call if it exists.
                // to do this, we need to store a temporary variable holding the left hand side
                // expression

                RC<AstVariableDeclaration> lhs_decl(new AstVariableDeclaration(
                    temp_var_name,
                    nullptr,
                    CloneAstNode(m_proto),
                    IdentifierFlags::FLAG_CONST,
                    m_location
                ));

                m_constructor_block->AddChild(lhs_decl);

                m_constructor_call.Reset(new AstTernaryExpression(
                    RC<AstHasExpression>(new AstHasExpression(
                        RC<AstVariable>(new AstVariable(
                            temp_var_name,
                            m_location
                        )),
                        construct_method_name,
                        m_location
                    )),
                    RC<AstMemberCallExpression>(new AstMemberCallExpression(
                        construct_method_name,
                        RC<AstNewExpression>(new AstNewExpression(
                            RC<AstPrototypeSpecification>(new AstPrototypeSpecification(
                                RC<AstVariable>(new AstVariable(
                                    temp_var_name,
                                    m_location
                                )),
                                m_location
                            )),
                            nullptr, // no args
                            false, // do not enable constructor call
                            m_location
                        )),
                        m_arg_list,
                        m_location
                    )),
                    RC<AstVariable>(new AstVariable(
                        temp_var_name,
                        m_location
                    )),
                    m_location
                ));
            }

            m_constructor_block->AddChild(m_constructor_call);

            m_constructor_block->Visit(visitor, mod);

            // Do not continue analyzing from here, as m_constructor_call contains the new AstNewExpression.
            return;
        }
    }
}

std::unique_ptr<Buildable> AstNewExpression::Build(AstVisitor *visitor, Module *mod)
{
    if (m_constructor_block != nullptr) {
        return m_constructor_block->Build(visitor, mod);
    }

    std::unique_ptr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    AssertThrow(m_prototype_type != nullptr);

#if HYP_SCRIPT_ENABLE_BUILTIN_CONSTRUCTOR_OVERRIDE
    // does not currently work in templates
    // e.g `new X` where `X` is `String` as a template argument, attempts to
    // construct the object rather than baking in
    if (m_object_value != nullptr && m_prototype_type->GetTypeClass() == TYPE_BUILTIN)
    {
        chunk->Append(m_object_value->Build(visitor, mod));
    }
    else
#endif
    {
        AssertThrow(m_proto != nullptr);
        chunk->Append(m_proto->Build(visitor, mod));

        UInt8 rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

        auto instr_new = BytecodeUtil::Make<RawOperation<>>();
        instr_new->opcode = NEW;
        instr_new->Accept<UInt8>(rp); // dst (overwrite proto)
        instr_new->Accept<UInt8>(rp); // src (holds proto)
        chunk->Append(std::move(instr_new));
    }

    // if (m_constructor_call != nullptr) {
    //     chunk->Append(m_constructor_call->Build(visitor, mod));
    // }
    
    
    /*AssertThrow(m_proto != nullptr);
    chunk->Append(m_proto->Build(visitor, mod));

    if (m_is_dynamic_type) {
        // register holding the main object
        uint8_t rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

        auto instr_new = BytecodeUtil::Make<RawOperation<>>();
        instr_new->opcode = NEW;
        instr_new->Accept<uint8_t>(rp); // dst (overwrite proto)
        instr_new->Accept<uint8_t>(rp); // src (holds proto)
        chunk->Append(std::move(instr_new));
    } else {
        if (m_constructor_call != nullptr) {
            chunk->Append(m_constructor_call->Build(visitor, mod));
        } else {
            // build in the value
            AssertThrow(m_object_value != nullptr);
            chunk->Append(m_object_value->Build(visitor, mod));
        }
    }*/

    return chunk;
}

void AstNewExpression::Optimize(AstVisitor *visitor, Module *mod)
{
    if (m_constructor_block != nullptr) {
        m_constructor_block->Optimize(visitor, mod);

        return;
    }

    AssertThrow(m_proto != nullptr);
    m_proto->Optimize(visitor, mod);

    if (m_object_value != nullptr) {
        m_object_value->Optimize(visitor, mod);
    }
}

RC<AstStatement> AstNewExpression::Clone() const
{
    return CloneImpl();
}

Tribool AstNewExpression::IsTrue() const
{
    if (m_constructor_call != nullptr) {
        return m_constructor_call->IsTrue();
    }

    if (m_object_value != nullptr) {
        return m_object_value->IsTrue();
    }

    return Tribool::Indeterminate();
}

bool AstNewExpression::MayHaveSideEffects() const
{
    if (m_constructor_call != nullptr) {
        return m_constructor_call->MayHaveSideEffects();
    }

    return true;
}

SymbolTypePtr_t AstNewExpression::GetExprType() const
{
    if (m_constructor_call != nullptr) {
        return m_constructor_call->GetExprType();
    }

    AssertThrow(m_instance_type != nullptr);
    return m_instance_type;
    // AssertThrow(m_type_expr != nullptr);
    // AssertThrow(m_type_expr->GetSpecifiedType() != nullptr);

    // return m_type_expr->GetSpecifiedType();
}

AstExpression *AstNewExpression::GetTarget() const
{
    if (m_constructor_call != nullptr) {
        return m_constructor_call->GetTarget();
    }

    return m_object_value.Get();
}

} // namespace hyperion::compiler
