#pragma once

#include <Lang/Compiler/AstVisitor.hpp>
#include <Lang/Compiler/Identifier.hpp>
#include <Lang/Compiler/Enums.hpp>
#include <Lang/Compiler/Ast/AstArgument.hpp>

#include <Core/Utilities/Optional.hpp>

#include <memory>
#include <utility>

namespace Hyperion {

// forward declaration
class Module;

struct SubstitutionResult
{
    Variant<const SymbolType*, RC<AstArgument>> value;
    size_t index = size_t(-1);
};

struct ArgInfo
{
    bool isNamed;
    String name;
    const SymbolType* type;
};

class SemanticAnalyzer : public AstVisitor
{
public:
    struct Helpers
    {
    private:
        static size_t FindFreeSlot(
            size_t currentIndex,
            const FlatSet<size_t>& usedIndices,
            const Array<GenericInstanceTypeInfo::Arg>& genericArgs,
            bool isVariadic,
            size_t numSuppliedArgs);

        static size_t ArgIndex(
            size_t currentIndex,
            const ArgInfo& argInfo,
            const FlatSet<size_t>& usedIndices,
            const Array<GenericInstanceTypeInfo::Arg>& genericArgs,
            bool isVariadic = false,
            size_t numSuppliedArgs = -1);

    public:
        static const SymbolType* GetVarArgType(
            const Array<GenericInstanceTypeInfo::Arg>& genericArgs);

        static HYP_NODISCARD SymbolType* SubstituteGenericParameters(
            AstVisitor* visitor,
            Module* mod,
            const SymbolType* inputType,
            const Array<GenericInstanceTypeInfo::Arg>& inArgs,
            const SourceLocation& location);

        static HYP_NODISCARD const SymbolType* GenericPromotion(
            AstVisitor* visitor,
            Module* mod,
            const SymbolType* lptr,
            const SymbolType* rptr,
            const SourceLocation& location);

        static HYP_NODISCARD const SymbolType* ResolvePlaceholderType(
            AstVisitor* visitor,
            Module* mod,
            const SymbolType* inputType,
            const SourceLocation& location);

        static void CheckArgTypeCompatible(
            AstVisitor* visitor,
            Module* mod,
            const SourceLocation& location,
            const SymbolType* argType,
            const SymbolType* paramType);

        static bool SubstituteFunctionArgs(
            AstVisitor* visitor,
            Module* mod,
            const SymbolType* symbolType,
            const Array<RC<AstArgument>>& args,
            const SourceLocation& location,
            const SymbolType*& outReturnType,
            Array<RC<AstArgument>>& outArgs);

        static void EnsureFunctionArgCompatibility(
            AstVisitor* visitor,
            Module* mod,
            const SymbolType* symbolType,
            const Array<RC<AstArgument>>& args,
            const SourceLocation& location);

        static void EnsureTypeAssignmentCompatibility(
            AstVisitor* visitor,
            Module* mod,
            const SymbolType* symbolType,
            const SymbolType* assignmentType,
            bool strictEnum,
            bool strictNull,
            const SourceLocation& location);
    };

public:
    SemanticAnalyzer(AstIterator* astIterator, CompilationUnit* compilationUnit);
    SemanticAnalyzer(const SemanticAnalyzer& other);

    /** Generates the compilation unit structure from the given statement iterator */
    void Analyze(bool expectModuleDecl = true);
};

} // namespace Hyperion
