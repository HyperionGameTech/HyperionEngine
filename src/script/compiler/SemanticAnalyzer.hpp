#pragma once

#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Identifier.hpp>
#include <script/compiler/Enums.hpp>
#include <script/compiler/ast/AstArgument.hpp>

#include <core/utilities/Optional.hpp>

#include <memory>
#include <utility>

namespace Hyperion {

// forward declaration
class Module;

struct SubstitutionResult
{
    Variant<const SymbolType*, RC<AstArgument>> value;
    SizeType index = SizeType(-1);
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
        static SizeType FindFreeSlot(
            SizeType currentIndex,
            const FlatSet<SizeType>& usedIndices,
            const Array<GenericInstanceTypeInfo::Arg>& genericArgs,
            bool isVariadic,
            SizeType numSuppliedArgs);

        static SizeType ArgIndex(
            SizeType currentIndex,
            const ArgInfo& argInfo,
            const FlatSet<SizeType>& usedIndices,
            const Array<GenericInstanceTypeInfo::Arg>& genericArgs,
            bool isVariadic = false,
            SizeType numSuppliedArgs = -1);

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
