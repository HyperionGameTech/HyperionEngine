#include <script/compiler/ast/AstFunctionExpression.hpp>
#include <script/compiler/ast/AstArrayExpression.hpp>
#include <script/compiler/ast/AstReturnStatement.hpp>
#include <script/compiler/ast/AstVariable.hpp>
#include <script/compiler/ast/AstTypeObject.hpp>
#include <script/compiler/ast/AstUndefined.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Compiler.hpp>
#include <script/compiler/Keywords.hpp>
#include <script/compiler/Module.hpp>
#include <script/compiler/Scope.hpp>
#include <script/compiler/Configuration.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <math/MathUtil.hpp>

#include <system/Debug.hpp>
#include <util/UTF8.hpp>
#include <script/Hasher.hpp>

#include <vector>
#include <iostream>

namespace hyperion::compiler {

AstFunctionExpression::AstFunctionExpression(
    const Array<RC<AstParameter>> &parameters,
    const RC<AstPrototypeSpecification> &return_type_specification,
    const RC<AstBlock> &block,
    const SourceLocation &location
) : AstExpression(location, ACCESS_MODE_LOAD),
    m_parameters(parameters),
    m_return_type_specification(return_type_specification),
    m_block(block),
    m_is_closure(false),
    m_is_constructor_definition(false),
    m_return_type(BuiltinTypes::ANY),
    m_static_id(0)
{
}

void AstFunctionExpression::Visit(AstVisitor *visitor, Module *mod)
{
    AssertThrow(visitor != nullptr);
    AssertThrow(mod != nullptr);
    AssertThrow(m_block != nullptr);

    m_block_with_parameters = CloneAstNode(m_block);

    m_is_closure = true;
    m_is_constructor_definition = GetExpressionFlags() & EXPR_FLAGS_CONSTRUCTOR_DEFINITION;

    int scope_flags = 0;

    if (m_is_closure) {
        scope_flags |= ScopeFunctionFlags::CLOSURE_FUNCTION_FLAG;

        // closures are objects with a method named '$invoke',
        // so we pass the '$functor' argument when it is called.
        m_closure_self_param.Reset(new AstParameter(
            "$functor",
            nullptr,
            nullptr,
            false,
            false,
            false,
            m_location
        ));
    }

    if (m_is_constructor_definition) {
        scope_flags |= CONSTRUCTOR_DEFINITION_FLAG;
    }
    
    // // open the new scope for parameters
    mod->m_scopes.Open(Scope(
        SCOPE_TYPE_FUNCTION,
        scope_flags
    ));

    if (m_is_closure) {
        // m_block_with_parameters->PrependChild(m_closure_self_param);
        m_closure_self_param->Visit(visitor, mod);
    }

    for (SizeType index = 0; index < m_parameters.Size(); index++) {
        AssertThrow(m_parameters[index] != nullptr);
        // m_block_with_parameters->PrependChild(m_parameters[index - 1]);
        m_parameters[index]->Visit(visitor, mod);
    }

    if (m_return_type_specification != nullptr) {
        if (m_is_constructor_definition) {
            visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_return_type_specification_invalid_on_constructor,
                m_return_type_specification->GetLocation()
            ));
        } else {
            m_block_with_parameters->PrependChild(m_return_type_specification);
        }
    }

    if (m_is_constructor_definition) {
        // m_return_type = mod->LookupSymbolType("Self");
        // AssertThrowMsg(m_return_type != nullptr, "Self type alias not found in constructor definition");

        // m_return_type = m_return_type->GetUnaliased();
        
        // add implicit 'return self' at the end
        m_block_with_parameters->AddChild(RC<AstReturnStatement>(new AstReturnStatement(
            RC<AstVariable>(new AstVariable(
                Keyword::ToString(Keywords::Keyword_self).Get(),
                m_block_with_parameters->GetLocation()
            )),
            m_block_with_parameters->GetLocation()
        )));
    }

    // visit the function body
    m_block_with_parameters->Visit(visitor, mod);

    if (m_return_type_specification != nullptr) {
        if (m_return_type_specification->GetHeldType() != nullptr) {
            m_return_type = m_return_type_specification->GetHeldType();
        } else {
            m_return_type = BuiltinTypes::UNDEFINED;
        }
    }

    // first item will be set to return type
    Array<GenericInstanceTypeInfo::Arg> param_symbol_types;

    for (auto &param : m_parameters) {
        if (!param || !param->GetIdentifier()) {
            // skip, should have added an error
            continue;
        }

        // add to list of param types
        param_symbol_types.PushBack(GenericInstanceTypeInfo::Arg {
            .m_name             = param->GetName(),
            .m_type             = param->GetIdentifier()->GetSymbolType(),
            .m_default_value    = param->GetDefaultValue(),
            .m_is_ref           = param->IsRef(),
            .m_is_const         = param->IsConst()
        });
    }

    const Scope *function_scope = &mod->m_scopes.Top();//m_block_with_parameters->GetScope();
    AssertThrow(function_scope != nullptr);
    
    if (function_scope->GetReturnTypes().Any()) {
        // search through return types for ambiguities
        for (const auto &it : function_scope->GetReturnTypes()) {
            AssertThrow(it.first != nullptr);

            if (m_return_type_specification != nullptr) {
                // strict mode, because user specifically stated the intended return type
                if (!m_return_type->TypeCompatible(*it.first, true)) {
                    // error; does not match what user specified
                    visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                        LEVEL_ERROR,
                        Msg_mismatched_return_type,
                        it.second,
                        m_return_type->ToString(),
                        it.first->ToString()
                    ));
                }
            } else {
                // deduce return type
                if (m_return_type->IsAnyType() || m_return_type->IsPlaceholderType()) {
                    m_return_type = it.first;
                } else if (m_return_type->TypeCompatible(*it.first, false)) {
                    m_return_type = SymbolType::TypePromotion(m_return_type, it.first, true);
                } else {
                    // error; more than one possible deduced return type.
                    visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                        LEVEL_ERROR,
                        Msg_multiple_return_types,
                        it.second
                    ));

                    break;
                }
            }
        }
    } else {
        m_return_type = BuiltinTypes::VOID_TYPE;
    }
    
    // create data members to copy closure parameters
    Array<SymbolMember_t> closure_obj_members;

    for (const auto &it : function_scope->GetClosureCaptures()) {
        const String &name = it.first;
        const RC<Identifier> &identifier = it.second;

        AssertThrow(identifier != nullptr);
        AssertThrow(identifier->GetSymbolType() != nullptr);

        RC<AstExpression> current_value(new AstVariable(
            name,
            m_location
        ));
        
        closure_obj_members.PushBack(SymbolMember_t {
            identifier->GetName(),
            identifier->GetSymbolType(),
            current_value
        });
    }

    // close parameter scope
    mod->m_scopes.Close();

    // set object type to be an instance of function
    Array<GenericInstanceTypeInfo::Arg> generic_param_types;
    generic_param_types.Reserve(param_symbol_types.Size() + 1);
    generic_param_types.PushBack({
        "@return",
        m_return_type
    });

    // perform checking to see if it should still be considered a closure
    if (m_is_closure) {
        AssertThrow(m_closure_self_param != nullptr);
        AssertThrow(m_closure_self_param->GetIdentifier() != nullptr);

        if (closure_obj_members.Any() || m_closure_self_param->GetIdentifier()->GetUseCount() > 0) {
            generic_param_types.PushBack(GenericInstanceTypeInfo::Arg {
                m_closure_self_param->GetName(),
                BuiltinTypes::ANY,
                nullptr
            });
        } else {
            // unset m_is_closure, as closure 'self' param is unused.
            m_is_closure = false;
        }
    }
        
    for (auto &it : param_symbol_types) {
        generic_param_types.PushBack(it);
    }

    m_symbol_type = SymbolType::GenericInstance(
        BuiltinTypes::FUNCTION, 
        GenericInstanceTypeInfo {
            generic_param_types
        }
    );

    if (m_is_closure) {
        // add $invoke to call this object

        closure_obj_members.PushBack(SymbolMember_t {
            "$invoke",
            m_symbol_type,
            RC<AstUndefined>(new AstUndefined(m_location)) // should never actually be compiled -- just a placeholder
        });

        // visit each member
        for (SymbolMember_t &member : closure_obj_members) {
            if (std::get<2>(member) != nullptr) {
                std::get<2>(member)->Visit(visitor, mod);
            }
        }

        SymbolTypePtr_t prototype_type = SymbolType::Object(
            "ClosureInstance", // Prototype type
            closure_obj_members,
            BuiltinTypes::OBJECT
        );

        auto prototype_value = RC<AstTypeObject>(new AstTypeObject(
            prototype_type,
            nullptr,
            m_location
        ));

        m_closure_type = SymbolType::GenericInstance(
            BuiltinTypes::CLOSURE_TYPE,
            GenericInstanceTypeInfo {
                generic_param_types
            },
            {
                SymbolMember_t {
                    "$proto",
                    prototype_type,
                    prototype_value
                }
            }
        );

        // register type
        visitor->GetCompilationUnit()->RegisterType(m_closure_type);

        // allow generic instance to be reused
        visitor->GetCompilationUnit()->GetCurrentModule()->
            m_scopes.Root().GetIdentifierTable().AddSymbolType(m_closure_type);

        AssertThrow(prototype_value != nullptr);

        m_closure_object = prototype_value;
        m_closure_object->Visit(visitor, mod);
    }

    // we do +1 to account for closure self var.
    if (m_parameters.Size() + (m_is_closure ? 1 : 0) > MathUtil::MaxSafeValue<UInt8>()) {
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_maximum_number_of_arguments,
            m_location
        ));
    }
}

std::unique_ptr<Buildable> AstFunctionExpression::Build(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_block_with_parameters != nullptr);

    InstructionStreamContextGuard context_guard(
        &visitor->GetCompilationUnit()->GetInstructionStream().GetContextTree(),
        INSTRUCTION_STREAM_CONTEXT_DEFAULT
    );
    
    std::unique_ptr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    if (m_is_closure && m_closure_self_param != nullptr) {
        chunk->Append(m_closure_self_param->Build(visitor, mod));
    }

    for (SizeType index = 0; index < m_parameters.Size(); index++) {
        AssertThrow(m_parameters[index] != nullptr);
        chunk->Append(m_parameters[index]->Build(visitor, mod));
    }

    UInt8 rp;

    AssertThrow(m_parameters.Size() + (m_is_closure ? 1 : 0) <= MathUtil::MaxSafeValue<UInt8>());

    // the properties of this function
    UInt8 nargs = UInt8(m_parameters.Size());

    if (m_is_closure) {
        nargs++; // make room for the closure self object
    }

    UInt8 flags = FunctionFlags::NONE;

    if (m_parameters.Any()) {
        const RC<AstParameter> &last = m_parameters.Back();
        AssertThrow(last != nullptr);

        if (last->IsVariadic()) {
            flags |= FunctionFlags::VARIADIC;
        }
    }

    if (m_is_closure) {
        flags |= FunctionFlags::CLOSURE;
    }

    // the label to jump to the very end
    LabelId end_label = context_guard->NewLabel();
    chunk->TakeOwnershipOfLabel(end_label);

    LabelId func_addr = context_guard->NewLabel();
    chunk->TakeOwnershipOfLabel(func_addr);

    // jump to end as to not execute the function body
    chunk->Append(BytecodeUtil::Make<Jump>(Jump::JMP, end_label));

    // store the function address before the function body
    chunk->Append(BytecodeUtil::Make<LabelMarker>(func_addr));
    
    // TODO add optimization to avoid duplicating the function body
    // Build the function 
    chunk->Append(BuildFunctionBody(visitor, mod));

    // set the label's position to after the block
    chunk->Append(BytecodeUtil::Make<LabelMarker>(end_label));

    // store local variable
    // get register index
    rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    auto func = BytecodeUtil::Make<BuildableFunction>();
    func->label_id = func_addr;
    func->reg = rp;
    func->nargs = nargs;
    func->flags = flags;
    chunk->Append(std::move(func));

    if (m_is_closure) {
        AssertThrow(m_closure_object != nullptr);

        const UInt8 func_expr_reg = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

        // increase reg usage for closure object to hold it while we move this function expr as a member
        visitor->GetCompilationUnit()->GetInstructionStream().IncRegisterUsage();
        rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

        const UInt8 closure_obj_reg = rp;

        chunk->Append(m_closure_object->Build(visitor, mod));

        const UInt32 hash = hash_fnv_1("$invoke");
        chunk->Append(Compiler::StoreMemberFromHash(visitor, mod, hash));
        
        visitor->GetCompilationUnit()->GetInstructionStream().DecRegisterUsage();
        rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();
        
        //ASSERT_MSG(rp == 0, "Register position should be 0 to return closure object");

        // swap regs, so the closure object returned (put on register zero)
        auto instr_mov_reg = BytecodeUtil::Make<RawOperation<>>();
        instr_mov_reg->opcode = MOV_REG;
        instr_mov_reg->Accept<UInt8>(0); // dst
        instr_mov_reg->Accept<UInt8>(closure_obj_reg); // src
        chunk->Append(std::move(instr_mov_reg));
    }

    return chunk;
}

std::unique_ptr<Buildable> AstFunctionExpression::BuildFunctionBody(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_block_with_parameters != nullptr);

    std::unique_ptr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    // increase stack size by the number of parameters
    const SizeType param_stack_size = m_parameters.Size() + ((m_is_closure && m_closure_self_param != nullptr) ? 1 : 0);

    // increase stack size for call stack info
    visitor->GetCompilationUnit()->GetInstructionStream().IncStackSize();

    // build the function body
    chunk->Append(m_block_with_parameters->Build(visitor, mod));

    if (!m_block_with_parameters->IsLastStatementReturn()) {
        // add RET instruction
        chunk->Append(BytecodeUtil::Make<Return>());
    }

    for (SizeType i = 0; i < param_stack_size; i++) {
        visitor->GetCompilationUnit()->GetInstructionStream().DecStackSize();
    }

    // decrease stack size for call stack info
    visitor->GetCompilationUnit()->GetInstructionStream().DecStackSize();

    return chunk;
}

void AstFunctionExpression::Optimize(AstVisitor *visitor, Module *mod)
{
    for (auto &param : m_parameters) {
        if (param != nullptr) {
            param->Optimize(visitor, mod);
        }
    }

    if (m_block_with_parameters != nullptr) {
        m_block_with_parameters->Optimize(visitor, mod);
    }
}

RC<AstStatement> AstFunctionExpression::Clone() const
{
    return CloneImpl();
}

Tribool AstFunctionExpression::IsTrue() const
{
    return Tribool::True();
}

bool AstFunctionExpression::MayHaveSideEffects() const
{
    // changed to true because it affects registers
    return true;
}

SymbolTypePtr_t AstFunctionExpression::GetExprType() const
{
    if (m_is_closure && m_closure_type != nullptr) {
        return m_closure_type;
    }

    return m_symbol_type;
}

} // namespace hyperion::compiler
