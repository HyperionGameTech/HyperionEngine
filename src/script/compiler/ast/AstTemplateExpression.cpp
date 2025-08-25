#include <script/compiler/ast/AstTemplateExpression.hpp>
#include <script/compiler/ast/AstTemplateInstantiation.hpp>
#include <script/compiler/ast/AstPrototypeSpecification.hpp>
#include <script/compiler/ast/AstTypeObject.hpp>
#include <script/compiler/ast/AstTypeRef.hpp>
#include <script/compiler/ast/AstVariable.hpp>
#include <script/compiler/ast/AstNil.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Module.hpp>
#include <script/compiler/Compiler.hpp>
#include <script/compiler/SemanticAnalyzer.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>
#include <script/compiler/emit/StorageOperation.hpp>

#include <core/debug/Debug.hpp>
#include <util/UTF8.hpp>

namespace hyperion::compiler {

AstTemplateExpression::AstTemplateExpression(
    const RC<AstExpression>& expr,
    const Array<RC<AstParameter>>& genericParams,
    const RC<AstPrototypeSpecification>& returnTypeSpecification,
    AstTemplateExpressionFlags flags,
    const SourceLocation& location)
    : AstExpression(location, ACCESS_MODE_LOAD),
      m_expr(expr),
      m_genericParams(genericParams),
      m_returnTypeSpecification(returnTypeSpecification),
      m_flags(flags)
{
}

AstTemplateExpression::AstTemplateExpression(
    const RC<AstExpression>& expr,
    const Array<RC<AstParameter>>& genericParams,
    const RC<AstPrototypeSpecification>& returnTypeSpecification,
    const SourceLocation& location)
    : AstTemplateExpression(
          expr,
          genericParams,
          returnTypeSpecification,
          AST_TEMPLATE_EXPRESSION_FLAG_NONE,
          location)
{
}

void AstTemplateExpression::Visit(AstVisitor* visitor, Module* mod)
{
    Assert(!m_isVisited);
    m_isVisited = true;

    m_symbolType = BuiltinTypes::UNDEFINED;

    Assert(visitor != nullptr);
    Assert(mod != nullptr);

    // dirty hacky way to make uninstantiated generic types be treated similarly to c++ templates
    visitor->GetCompilationUnit()->GetErrorList().SuppressErrors(true);

    mod->m_scopes.Open(Scope(SCOPE_TYPE_NORMAL, UNINSTANTIATED_GENERIC_FLAG));

    m_block.Reset(new AstBlock(m_location));

    // visit params before expression to make declarations
    // for things that may be used in the expression

    Array<SymbolTypeRef> paramSymbolTypes;
    paramSymbolTypes.Reserve(m_genericParams.Size());

    for (RC<AstParameter>& genericParam : m_genericParams)
    {
        Assert(genericParam != nullptr);

        SymbolTypeRef genericParamType = SymbolType::GenericParameter(
            genericParam->GetName(),
            BuiltinTypes::CLASS_TYPE);

        RC<AstTypeObject> genericParamTypeObject(new AstTypeObject(
            genericParamType,
            BuiltinTypes::CLASS_TYPE,
            m_location));

        genericParamType->SetTypeObject(genericParamTypeObject);

        mod->m_scopes.Top().GetIdentifierTable().AddSymbolType(genericParamType);

        // when visited, will register the SymbolType
        genericParamTypeObject->Visit(visitor, mod);

        {
            auto* genericParamTypeObjectValueOf = genericParamTypeObject->GetDeepValueOf();
            Assert(genericParamTypeObjectValueOf != nullptr);

            SymbolTypeRef genericParamTypeObjectValueOfType = genericParamTypeObjectValueOf->GetHeldType();
            Assert(genericParamTypeObjectValueOfType != nullptr);
            genericParamType = genericParamTypeObjectValueOfType->GetUnaliased();
        }

        // Keep it around because SymbolType holds a weak reference to it
        m_genericParamTypeObjects.PushBack(std::move(genericParamTypeObject));

        RC<AstVariableDeclaration> varDecl;

        if (genericParam->IsVariadic())
        {
            // if variadic, create a variadic template instantiation
            // so T gets transformed into varargs<T>
            RC<AstTemplateInstantiation> variadicTemplateInstantiation(new AstTemplateInstantiation(
                RC<AstVariable>(new AstVariable(
                    "varargs",
                    m_location)),
                { RC<AstArgument>(new AstArgument(
                    RC<AstTypeRef>(new AstTypeRef(
                        genericParamType,
                        m_location)),
                    false,
                    false,
                    false,
                    false,
                    "T",
                    m_location)) },
                m_location));

            variadicTemplateInstantiation->Visit(visitor, mod);

            { // set genericParamType to be the variadic template instantiation type
                auto* variadicTemplateInstantiationValueOf = variadicTemplateInstantiation->GetDeepValueOf();
                Assert(variadicTemplateInstantiationValueOf != nullptr);

                SymbolTypeRef variadicTemplateInstantiationType = variadicTemplateInstantiationValueOf->GetHeldType();
                Assert(variadicTemplateInstantiationType != nullptr);
                variadicTemplateInstantiationType = variadicTemplateInstantiationType->GetUnaliased();

                // set the variadic template instantiation type as the generic param type
                genericParamType = std::move(variadicTemplateInstantiationType);
            }

            varDecl.Reset(new AstVariableDeclaration(
                genericParam->GetName(),
                nullptr,
                CloneAstNode(variadicTemplateInstantiation),
                IdentifierFlags::FLAG_CONST,
                m_location));
        }
        else
        {
            varDecl.Reset(new AstVariableDeclaration(
                genericParam->GetName(),
                nullptr,
                RC<AstTypeRef>(new AstTypeRef(
                    genericParamType,
                    SourceLocation::eof)),
                IdentifierFlags::FLAG_CONST,
                m_location));
        }

        m_block->AddChild(varDecl);

        paramSymbolTypes.PushBack(std::move(genericParamType));
    }

    if (m_returnTypeSpecification != nullptr)
    {
        m_block->AddChild(m_returnTypeSpecification);
    }

    Assert(m_expr != nullptr);

    m_block->AddChild(m_expr);
    m_block->Visit(visitor, mod);

    Array<GenericInstanceTypeInfo::Arg> genericParamTypes;
    genericParamTypes.Reserve(m_genericParams.Size() + 1); // anotha one for @return

    SymbolTypeRef exprReturnType;
    SymbolTypeRef explicitReturnType = m_returnTypeSpecification != nullptr
        ? m_returnTypeSpecification->GetHeldType()
        : nullptr;

    // if return type has been specified - visit it and check to see
    // if it's compatible with the expression
    if (m_returnTypeSpecification != nullptr)
    {
        Assert(explicitReturnType != nullptr);

        exprReturnType = explicitReturnType;
    }
    else
    {
        exprReturnType = BuiltinTypes::PLACEHOLDER;
    }

    genericParamTypes.PushBack({ "@return",
        exprReturnType });

    Assert(m_genericParams.Size() == paramSymbolTypes.Size());

    for (SizeType i = 0; i < m_genericParams.Size(); i++)
    {
        const RC<AstParameter>& param = m_genericParams[i];
        const SymbolTypeRef& paramSymbolType = paramSymbolTypes[i];

        RC<AstExpression> defaultValue = CloneAstNode(param->GetDefaultValue());

        if (defaultValue == nullptr)
        {
            defaultValue = RC<AstTypeRef>(new AstTypeRef(
                paramSymbolType,
                SourceLocation::eof));
        }

        genericParamTypes.PushBack(GenericInstanceTypeInfo::Arg {
            param->GetName(),
            std::move(paramSymbolType),
            std::move(defaultValue) });
    }

    m_symbolType = SymbolType::GenericInstance(
        BuiltinTypes::GENERIC_VARIABLE_TYPE,
        GenericInstanceTypeInfo {
            genericParamTypes });

    Assert(m_symbolType != nullptr);

    if (m_flags & AST_TEMPLATE_EXPRESSION_FLAG_NATIVE)
    {
        Assert(m_symbolType->GetId() == -1, "For native generic expressions, symbol type must not yet be registered");

        m_symbolType->SetFlags(m_symbolType->GetFlags() | SYMBOL_TYPE_FLAGS_NATIVE);

        // Create dummy type object
        m_nativeDummyTypeObject.Reset(new AstTypeObject(
            m_symbolType,
            BuiltinTypes::CLASS_TYPE,
            m_location));

        m_symbolType->SetTypeObject(m_nativeDummyTypeObject);

        // Register our type
        visitor->GetCompilationUnit()->RegisterType(m_symbolType);
    }

    mod->m_scopes.Close();

    // dirty hacky way to make uninstantiated generic types be treated similarly to c++ templates
    visitor->GetCompilationUnit()->GetErrorList().SuppressErrors(false);
}

UniquePtr<Buildable> AstTemplateExpression::Build(AstVisitor* visitor, Module* mod)
{
    Assert(m_isVisited);

    // // build the expression
    // if (m_flags & AST_TEMPLATE_EXPRESSION_FLAG_NATIVE) {
    //     auto chunk = BytecodeUtil::Make<BytecodeChunk>();

    //     Assert(m_symbolType->GetId() != -1, "For native generic expressions, symbol type must be registered");

    //     // Assert(m_nativeDummyTypeObject != nullptr);
    //     // chunk->Append(m_nativeDummyTypeObject->Build(visitor, mod));

    //     chunk->Append(m_block->Build(visitor, mod));

    //     uint8 rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    //     // store the result which is in rp into static memory
    //     chunk->Append(BytecodeUtil::Make<Comment>("Store native generic class " + m_symbolType->GetName() + " in static data at index " + String::ToString(m_symbolType->GetId())));

    //     auto instrStoreStatic = BytecodeUtil::Make<StorageOperation>();
    //     instrStoreStatic->GetBuilder().Store(rp).Static().ByIndex(m_symbolType->GetId());
    //     chunk->Append(std::move(instrStoreStatic));

    //     return chunk;
    // }

    return nullptr; // Uninstantiated generic types are not buildable
}

void AstTemplateExpression::Optimize(AstVisitor* visitor, Module* mod)
{
    // do nothing - instantiated generic expressions will be optimized
}

RC<AstStatement> AstTemplateExpression::Clone() const
{
    return CloneImpl();
}

Tribool AstTemplateExpression::IsTrue() const
{
    return Tribool::Indeterminate();
}

bool AstTemplateExpression::MayHaveSideEffects() const
{
    return true;
}

SymbolTypeRef AstTemplateExpression::GetExprType() const
{
    Assert(m_isVisited);
    Assert(m_symbolType != nullptr);

    return m_symbolType;
}

SymbolTypeRef AstTemplateExpression::GetHeldType() const
{
    Assert(m_isVisited);
    Assert(m_expr != nullptr);

    return m_expr->GetHeldType();
}

const AstExpression* AstTemplateExpression::GetValueOf() const
{
    return AstExpression::GetValueOf();
}

const AstExpression* AstTemplateExpression::GetDeepValueOf() const
{
    return AstExpression::GetDeepValueOf();
}

const AstExpression* AstTemplateExpression::GetHeldGenericExpr() const
{
    return m_expr.Get();
}

} // namespace hyperion::compiler
