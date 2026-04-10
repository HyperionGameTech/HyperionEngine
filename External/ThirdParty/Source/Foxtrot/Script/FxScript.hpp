// #pragma once

// #include <Core/MemPool/FxMPPagedArray.hpp>
// #include <Util/FxTokenizer.hpp>
// #include <vector>

// #define FX_SCRIPT_VERSION_MAJOR 0
// #define FX_SCRIPT_VERSION_MINOR 3
// #define FX_SCRIPT_VERSION_PATCH 0

// #define FX_SCRIPT_USE_MEMPOOL 1

// #ifdef FX_SCRIPT_USE_MEMPOOL

// #include <Core/FxMemory.hpp>

// #define FX_SCRIPT_ALLOC_MEMORY(ptrtype_, size_) FxMemPool::Alloc<ptrtype_>(size_)
// #define FX_SCRIPT_ALLOC_NODE(nodetype_)         FxMemPool::Alloc<nodetype_>(sizeof(nodetype_))
// #define FX_SCRIPT_FREE(ptrtype_, ptr_)          FxMemPool::Free<ptrtype_>(ptr_)

// #else

// // #define FX_SCRIPT_ALLOC_MEMORY(ptrtype_, size_) new ptrtype_[size_]
// #define FX_SCRIPT_ALLOC_MEMORY(ptrtype_, size_) FxScriptAllocMemory<ptrtype_>(size_)
// #define FX_SCRIPT_ALLOC_NODE(nodetype_)         FxScriptAllocMemory<nodetype_>(sizeof(nodetype_))
// #define FX_SCRIPT_FREE(ptrtype_, ptr_)          FxScriptFreeMemory<ptrtype_>(ptr_)

// #include <cstdlib>

// #endif

// template <typename T>
// T* FxScriptAllocMemory(size_t size)
// {
//     T* ptr = reinterpret_cast<T*>(malloc(size));
//     if constexpr (std::is_constructible_v<T>) {
//         new (ptr) T;
//     }

//     return ptr;
// }

// template <typename T>
// void FxScriptFreeMemory(T* ptr)
// {
//     if constexpr (std::is_destructible_v<T>) {
//         ptr->~T();
//     }

//     free(ptr);
// }

// struct FxAstVarRef;
// struct FxScriptScope;
// struct FxScriptAction;

// struct FxScriptValue
// {
//     static FxScriptValue None;

//     enum ValueType : uint16
//     {
//         NONETYPE = 0x00,
//         INT = 0x01,
//         FLOAT = 0x02,
//         STRING = 0x04,
//         VEC3 = 0x08,
//         REF = 0x10
//     };

//     ValueType Type = NONETYPE;

//     union
//     {
//         int ValueInt = 0;
//         float ValueFloat;
//         float ValueVec3[3];
//         char* ValueString;

//         FxAstVarRef* ValueRef;
//     };

//     FxScriptValue() {}

//     explicit FxScriptValue(ValueType type, int value) : Type(type), ValueInt(value) {}

//     explicit FxScriptValue(ValueType type, float value) : Type(type), ValueFloat(value) {}

//     FxScriptValue(const FxScriptValue& other)
//     {
//         Type = other.Type;
//         if (other.Type == INT) {
//             ValueInt = other.ValueInt;
//         }
//         else if (other.Type == FLOAT) {
//             ValueFloat = other.ValueFloat;
//         }
//         else if (other.Type == STRING) {
//             ValueString = other.ValueString;
//         }
//         else if (other.Type == REF) {
//             ValueRef = other.ValueRef;
//         }
//     }

//     void Print() const
//     {
//         printf("[Value: ");
//         if (Type == NONETYPE) {
//             printf("Null]\n");
//         }
//         else if (Type == INT) {
//             printf("Int, %d]\n", ValueInt);
//         }
//         else if (Type == FLOAT) {
//             printf("Float, %f]\n", ValueFloat);
//         }
//         else if (Type == STRING) {
//             printf("String, %s]\n", ValueString);
//         }
//         else if (Type == REF) {
//             printf("Ref, %p]\n", ValueRef);
//         }
//     }

//     inline bool IsNumber() { return (Type == INT || Type == FLOAT); }

//     inline bool IsRef() { return (Type == REF); }
// };

// enum FxAstType
// {
//     FX_AST_LITERAL,
//     // FX_AST_NAME,

//     FX_AST_BINOP,
//     FX_AST_UNARYOP,
//     FX_AST_BLOCK,

//     // Variables
//     FX_AST_VARREF,
//     FX_AST_VARDECL,
//     FX_AST_ASSIGN,

//     // Actions
//     FX_AST_ACTIONDECL,
//     FX_AST_ACTIONCALL,
//     FX_AST_RETURN,

//     FX_AST_DOCCOMMENT,

//     FX_AST_COMMANDMODE,
// };

// struct FxAstNode
// {
//     FxAstType NodeType;
// };


// struct FxAstLiteral : public FxAstNode
// {
//     FxAstLiteral() { this->NodeType = FX_AST_LITERAL; }

//     // FxTokenizer::Token* Token = nullptr;
//     FxScriptValue Value;
// };

// struct FxAstBinop : public FxAstNode
// {
//     FxAstBinop() { this->NodeType = FX_AST_BINOP; }

//     FxTokenizer::Token* OpToken = nullptr;
//     FxAstNode* Left = nullptr;
//     FxAstNode* Right = nullptr;
// };

// struct FxAstBlock : public FxAstNode
// {
//     FxAstBlock() { this->NodeType = FX_AST_BLOCK; }

//     std::vector<FxAstNode*> Statements;
// };

// struct FxAstVarRef : public FxAstNode
// {
//     FxAstVarRef() { this->NodeType = FX_AST_VARREF; }

//     FxTokenizer::Token* Name = nullptr;
//     FxScriptScope* Scope = nullptr;
// };

// struct FxAstAssign : public FxAstNode
// {
//     FxAstAssign() { this->NodeType = FX_AST_ASSIGN; }

//     FxAstVarRef* Var = nullptr;
//     // FxScriptValue Value;
//     FxAstNode* Rhs = nullptr;
// };

// struct FxAstVarDecl : public FxAstNode
// {
//     FxAstVarDecl() { this->NodeType = FX_AST_VARDECL; }

//     FxTokenizer::Token* Name = nullptr;
//     FxTokenizer::Token* Type = nullptr;
//     FxAstAssign* Assignment = nullptr;

//     /// Ignore the scope that the variable is declared in, force it to be global.
//     bool DefineAsGlobal = false;
// };

// struct FxAstDocComment : public FxAstNode
// {
//     FxAstDocComment() { this->NodeType = FX_AST_DOCCOMMENT; }

//     FxTokenizer::Token* Comment;
// };

// struct FxAstActionDecl : public FxAstNode
// {
//     FxAstActionDecl() { this->NodeType = FX_AST_ACTIONDECL; }

//     FxTokenizer::Token* Name = nullptr;
//     FxAstVarDecl* ReturnVar = nullptr;
//     FxAstBlock* Params = nullptr;
//     FxAstBlock* Block = nullptr;

//     std::vector<FxAstDocComment*> DocComments;
// };

// struct FxAstCommandMode : public FxAstNode
// {
//     FxAstCommandMode() { this->NodeType = FX_AST_COMMANDMODE; }

//     FxAstNode* Node = nullptr;
// };

// struct FxAstActionCall : public FxAstNode
// {
//     FxAstActionCall() { this->NodeType = FX_AST_ACTIONCALL; }

//     FxScriptAction* Action = nullptr;
//     FxHash32 HashedName = 0;
//     std::vector<FxAstNode*> Params {}; // FxAstLiteral or FxAstVarRef
// };

// struct FxAstReturn : public FxAstNode
// {
//     FxAstReturn() { this->NodeType = FX_AST_RETURN; }
// };

// /**
//  * @brief Data is accessible from a label, such as a variable or an action.
//  */
// struct FxScriptLabelledData
// {
//     FxHash32 HashedName = 0;
//     FxTokenizer::Token* Name = nullptr;

//     FxScriptScope* Scope = nullptr;
// };

// struct FxScriptAction : public FxScriptLabelledData
// {
//     FxScriptAction(FxTokenizer::Token* name, FxScriptScope* scope, FxAstBlock* block, FxAstActionDecl* declaration)
//     {
//         HashedName = name->GetHash();
//         Name = name;
//         Scope = scope;
//         Block = block;
//         Declaration = declaration;
//     }

//     FxAstActionDecl* Declaration = nullptr;
//     FxAstBlock* Block = nullptr;
// };

// struct FxScriptVar : public FxScriptLabelledData
// {
//     FxTokenizer::Token* Type = nullptr;
//     FxScriptValue Value;

//     bool IsExternal = false;

//     void Print() const
//     {
//         printf("[Var] Type: %.*s, Name: %.*s (Hash:%u)", Type->Length, Type->Start, Name->Length, Name->Start,
//                Name->GetHash());
//         Value.Print();
//     }

//     FxScriptVar() {}

//     FxScriptVar(FxTokenizer::Token* type, FxTokenizer::Token* name, FxScriptScope* scope, bool is_external = false)
//         : Type(type)
//     {
//         this->HashedName = name->GetHash();
//         this->Name = name;
//         this->Scope = scope;
//         IsExternal = is_external;
//     }

//     FxScriptVar(const FxScriptVar& other)
//     {
//         HashedName = other.HashedName;
//         Type = other.Type;
//         Name = other.Name;
//         Value = other.Value;
//         IsExternal = other.IsExternal;
//     }

//     FxScriptVar& operator=(FxScriptVar&& other) noexcept
//     {
//         HashedName = other.HashedName;
//         Type = other.Type;
//         Name = other.Name;
//         Value = other.Value;
//         IsExternal = other.IsExternal;

//         Name = nullptr;
//         Type = nullptr;
//         HashedName = 0;

//         return *this;
//     }

//     ~FxScriptVar()
//     {
//         if (!IsExternal) {
//             return;
//         }

//         // Free tokens allocated by external variables
//         if (Type && Type->Start) {
//             FX_SCRIPT_FREE(char, Type->Start);
//         }

//         if (this->Name && this->Name->Start) {
//             FX_SCRIPT_FREE(char, this->Name->Start);
//         }
//     }
// };

// class FxScriptInterpreter;
// class FxScriptVM;


// struct FxScriptExternalFunc
// {
//     // using FuncType = void (*)(FxScriptInterpreter& interpreter, std::vector<FxScriptValue>& params, FxScriptValue*
//     // return_value);

//     using FuncType = void (*)(FxScriptVM* vm, std::vector<FxScriptValue>& params, FxScriptValue* return_value);

//     FxHash32 HashedName = 0;
//     FuncType Function = nullptr;

//     std::vector<FxScriptValue::ValueType> ParameterTypes;
//     bool IsVariadic = false;
// };

// struct FxScriptScope
// {
//     FxPagedArray<FxScriptVar> Vars;
//     FxPagedArray<FxScriptAction> Actions;

//     FxScriptScope* Parent = nullptr;

//     // This points to the return value for the current scope. If an action returns a value,
//     // this will be set to the variable that holds its value. This is interpreter only.
//     FxScriptVar* ReturnVar = nullptr;

//     void PrintAllVarsInScope()
//     {
//         puts("\n=== SCOPE ===");
//         for (FxScriptVar& var : Vars) {
//             var.Print();
//         }
//     }

//     FxScriptVar* FindVarInScope(FxHash32 hashed_name) { return FindInScope<FxScriptVar>(hashed_name, Vars); }

//     FxScriptAction* FindActionInScope(FxHash32 hashed_name)
//     {
//         return FindInScope<FxScriptAction>(hashed_name, Actions);
//     }

//     template <typename T>
//         requires std::is_base_of_v<FxScriptLabelledData, T>
//     T* FindInScope(FxHash32 hashed_name, const FxPagedArray<T>& buffer)
//     {
//         for (T& var : buffer) {
//             if (var.HashedName == hashed_name) {
//                 return &var;
//             }
//         }

//         return nullptr;
//     }
// };

// class FxScriptInterpreter;

// class FxConfigScript
// {
//     using Token = FxTokenizer::Token;
//     using TT = FxTokenizer::TokenType;

// public:
//     FxConfigScript() = default;

//     void LoadFile(const char* path);

//     void PushScope();
//     void PopScope();

//     FxScriptVar* FindVar(FxHash32 hashed_name);

//     FxScriptAction* FindAction(FxHash32 hashed_name);
//     FxScriptExternalFunc* FindExternalAction(FxHash32 hashed_name);

//     FxAstNode* TryParseKeyword();

//     FxAstAssign* TryParseAssignment(FxTokenizer::Token* var_name);

//     FxScriptValue ParseValue();

//     FxAstActionDecl* ParseActionDeclare();

//     FxAstNode* ParseRhs();
//     FxAstActionCall* ParseActionCall();

//     FxAstVarDecl* ParseVarDeclare(FxScriptScope* scope = nullptr);

//     FxAstBlock* ParseBlock();
//     FxAstNode* ParseStatement();

//     FxAstNode* ParseStatementAsCommand();

//     FxAstBlock* Parse();

//     /**
//      * @brief Parses and executes a script.
//      * @param interpreter The interpreter to execute with
//      */
//     void Execute(FxScriptVM& vm);

//     /**
//      * @brief Executes a command on a script. Defaults to parsing with command style syntax.
//      * @param command The command to execute on the script.
//      * @return If the command has been executed
//      */
//     bool ExecuteUserCommand(const char* command, FxScriptInterpreter& interpreter);

//     Token& GetToken(int offset = 0);
//     Token& EatToken(TT token_type);

//     void RegisterExternalFunc(FxHash32 func_name, std::vector<FxScriptValue::ValueType> param_types,
//                               FxScriptExternalFunc::FuncType func, bool is_variadic);

//     void DefineExternalVar(const char* type, const char* name, const FxScriptValue& value);

// private:
//     template <typename T>
//         requires std::is_base_of_v<FxScriptLabelledData, T>
//     T* FindLabelledData(FxHash32 hashed_name, FxPagedArray<T>& buffer)
//     {
//         FxScriptScope* scope = mCurrentScope;

//         while (scope) {
//             T* var = scope->FindInScope<T>(hashed_name, buffer);
//             if (var) {
//                 return var;
//             }

//             scope = scope->Parent;
//         }

//         return nullptr;
//     }

//     void DefineDefaultExternalFunctions();

// private:
//     FxPagedArray<FxScriptScope> mScopes;
//     FxScriptScope* mCurrentScope;

//     std::vector<FxScriptExternalFunc> mExternalFuncs;

//     std::vector<FxAstDocComment*> CurrentDocComments;

//     FxAstBlock* mRootBlock = nullptr;

//     bool mHasErrors = false;
//     bool mInCommandMode = false;

//     char* mFileData;
//     FxPagedArray<Token> mTokens = {};
//     uint32 mTokenIndex = 0;
// };

// //////////////////////////////////
// // Script AST Printer
// //////////////////////////////////

// class FxAstPrinter
// {
// public:
//     FxAstPrinter(FxAstBlock* root_block)
//     //: mRootBlock(root_block)
//     {
//     }

//     void Print(FxAstNode* node, int depth = 0)
//     {
//         if (node == nullptr) {
//             return;
//         }

//         for (int i = 0; i < depth; i++) {
//             putchar(' ');
//             putchar(' ');
//         }

//         if (node->NodeType == FX_AST_BLOCK) {
//             puts("[BLOCK]");

//             FxAstBlock* block = reinterpret_cast<FxAstBlock*>(node);
//             for (FxAstNode* child : block->Statements) {
//                 Print(child, depth + 1);
//             }
//             return;
//         }
//         else if (node->NodeType == FX_AST_ACTIONDECL) {
//             FxAstActionDecl* actiondecl = reinterpret_cast<FxAstActionDecl*>(node);
//             printf("[ACTIONDECL] ");
//             actiondecl->Name->Print();

//             for (FxAstNode* param : actiondecl->Params->Statements) {
//                 Print(param, depth + 1);
//             }

//             Print(actiondecl->Block, depth + 1);
//         }
//         else if (node->NodeType == FX_AST_VARDECL) {
//             FxAstVarDecl* vardecl = reinterpret_cast<FxAstVarDecl*>(node);

//             printf("[VARDECL] ");
//             vardecl->Name->Print();

//             Print(vardecl->Assignment, depth + 1);
//         }
//         else if (node->NodeType == FX_AST_ASSIGN) {
//             FxAstAssign* assign = reinterpret_cast<FxAstAssign*>(node);

//             printf("[ASSIGN] ");

//             assign->Var->Name->Print();
//             Print(assign->Rhs, depth + 1);
//         }
//         else if (node->NodeType == FX_AST_ACTIONCALL) {
//             FxAstActionCall* actioncall = reinterpret_cast<FxAstActionCall*>(node);

//             printf("[ACTIONCALL] ");
//             if (actioncall->Action == nullptr) {
//                 printf("{defined externally}");
//             }
//             else {
//                 actioncall->Action->Name->Print(true);
//             }

//             printf(" (%zu params)\n", actioncall->Params.size());
//         }
//         else if (node->NodeType == FX_AST_LITERAL) {
//             FxAstLiteral* literal = reinterpret_cast<FxAstLiteral*>(node);

//             printf("[LITERAL] ");
//             literal->Value.Print();
//         }
//         else if (node->NodeType == FX_AST_BINOP) {
//             FxAstBinop* binop = reinterpret_cast<FxAstBinop*>(node);

//             printf("[BINOP] ");
//             binop->OpToken->Print();

//             Print(binop->Left, depth + 1);
//             Print(binop->Right, depth + 1);
//         }
//         else if (node->NodeType == FX_AST_COMMANDMODE) {
//             FxAstCommandMode* command_mode = reinterpret_cast<FxAstCommandMode*>(node);
//             printf("[COMMANDMODE]\n");

//             Print(command_mode->Node, depth + 1);
//         }
//         else if (node->NodeType == FX_AST_RETURN) {
//             puts("[RETURN]");
//         }
//         else {
//             puts("[UNKNOWN]");
//         }
//         // else if (node->NodeType == FX_AST_)
//     }

// public:
//     // FxAstBlock* mRootBlock = nullptr;
// };

// /////////////////////////////////////////////
// // Script Bytecode Emitter
// /////////////////////////////////////////////

// enum FxScriptRegister : uint8
// {
//     FX_REG_NONE = 0x00,
//     FX_REG_X0,
//     FX_REG_X1,
//     FX_REG_X2,
//     FX_REG_X3,

//     /**
//      * @brief Return address register.
//      */
//     FX_REG_RA,

//     /**
//      * @brief Register that contains the result of an operation.
//      */
//     FX_REG_XR,

//     /**
//      * @brief Register that contains the stack pointer for the VM.
//      */
//     FX_REG_SP,

//     FX_REG_SIZE,
// };

// enum FxScriptRegisterFlag : uint16
// {
//     FX_REGFLAG_NONE = 0x00,
//     FX_REGFLAG_X0 = 0x01,
//     FX_REGFLAG_X1 = 0x02,
//     FX_REGFLAG_X2 = 0x04,
//     FX_REGFLAG_X3 = 0x08,
//     FX_REGFLAG_RA = 0x10,
//     FX_REGFLAG_XR = 0x20,
// };

// inline FxScriptRegisterFlag operator|(FxScriptRegisterFlag a, FxScriptRegisterFlag b)
// {
//     return static_cast<FxScriptRegisterFlag>(static_cast<uint16>(a) | static_cast<uint16>(b));
// }

// inline FxScriptRegisterFlag operator&(FxScriptRegisterFlag a, FxScriptRegisterFlag b)
// {
//     return static_cast<FxScriptRegisterFlag>(static_cast<uint16>(a) & static_cast<uint16>(b));
// }

// struct FxScriptBytecodeVarHandle
// {
//     FxHash32 HashedName = 0;
//     FxScriptValue::ValueType Type = FxScriptValue::INT;
//     int64 Offset = 0;

//     uint16 SizeOnStack = 4;
//     uint32 ScopeIndex = 0;
// };

// struct FxScriptBytecodeActionHandle
// {
//     FxHash32 HashedName = 0;
//     uint32 BytecodeIndex = 0;
// };

// class FxScriptBCEmitter
// {
// public:
//     FxScriptBCEmitter() = default;

//     void BeginEmitting(FxAstNode* node);
//     void Emit(FxAstNode* node);

//     enum RhsMode
//     {
//         RHS_FETCH_TO_REGISTER,

//         /**
//          * @brief Pushes the value to the stack, assuming that the value does not exist yet.
//          */
//         RHS_DEFINE_IN_MEMORY,

//         RHS_ASSIGN_TO_HANDLE,
//     };

//     static FxScriptRegister RegFlagToReg(FxScriptRegisterFlag reg_flag);
//     static FxScriptRegisterFlag RegToRegFlag(FxScriptRegister reg);

//     static const char* GetRegisterName(FxScriptRegister reg);

//     FxPagedArray<uint8> mBytecode {};

//     enum VarDeclareMode
//     {
//         DECLARE_DEFAULT,
//         DECLARE_NO_EMIT,
//     };


// private:
//     void EmitBlock(FxAstBlock* block);
//     void EmitAction(FxAstActionDecl* action);
//     void DoActionCall(FxAstActionCall* call);
//     FxScriptBytecodeVarHandle* DoVarDeclare(FxAstVarDecl* decl, VarDeclareMode mode = DECLARE_DEFAULT);
//     void EmitAssign(FxAstAssign* assign);
//     FxScriptBytecodeVarHandle* DefineAndFetchParam(FxAstNode* param_decl_node);
//     FxScriptBytecodeVarHandle* DefineReturnVar(FxAstVarDecl* decl);

//     FxScriptRegister EmitVarFetch(FxAstVarRef* ref, RhsMode mode);

//     void DoLoad(uint32 stack_offset, FxScriptRegister output_reg, bool force_absolute = false);
//     void DoSaveInt32(uint32 stack_offset, uint32 value, bool force_absolute = false);
//     void DoSaveReg32(uint32 stack_offset, FxScriptRegister reg, bool force_absolute = false);

//     void EmitPush32(uint32 value);
//     void EmitPush32r(FxScriptRegister reg);

//     void EmitPop32(FxScriptRegister output_reg);

//     void EmitLoad32(int offset, FxScriptRegister output_reg);
//     void EmitLoadAbsolute32(uint32 position, FxScriptRegister output_reg);

//     void EmitSave32(int16 offset, uint32 value);
//     void EmitSaveReg32(int16 offset, FxScriptRegister reg);

//     void EmitSaveAbsolute32(uint32 offset, uint32 value);
//     void EmitSaveAbsoluteReg32(uint32 offset, FxScriptRegister reg);

//     void EmitJumpRelative(uint16 offset);
//     void EmitJumpAbsolute(uint32 position);
//     void EmitJumpAbsoluteReg32(FxScriptRegister reg);
//     void EmitJumpCallAbsolute(uint32 position);
//     void EmitJumpReturnToCaller();
//     void EmitJumpCallExternal(FxHash32 hashed_name);

//     void EmitMoveInt32(FxScriptRegister reg, uint32 value);

//     void EmitParamsStart();
//     void EmitType(FxScriptValue::ValueType type);

//     uint32 EmitDataString(char* str, uint16 length);

//     FxScriptRegister EmitBinop(FxAstBinop* binop, FxScriptBytecodeVarHandle* handle);

//     FxScriptRegister EmitRhs(FxAstNode* rhs, RhsMode mode, FxScriptBytecodeVarHandle* handle);

//     FxScriptRegister EmitLiteralInt(FxAstLiteral* literal, RhsMode mode, FxScriptBytecodeVarHandle* handle);
//     FxScriptRegister EmitLiteralString(FxAstLiteral* literal, RhsMode mode, FxScriptBytecodeVarHandle* handle);


//     void WriteOp(uint8 base_op, uint8 spec_op);
//     void Write16(uint16 value);
//     void Write32(uint32 value);

//     FxScriptRegister FindFreeRegister();

//     FxScriptBytecodeVarHandle* FindVarHandle(FxHash32 hashed_name);
//     FxScriptBytecodeActionHandle* FindActionHandle(FxHash32 hashed_name);

//     void PrintBytecode();

//     void MarkRegisterUsed(FxScriptRegister reg);
//     void MarkRegisterFree(FxScriptRegister reg);

// public:
//     FxPagedArray<FxScriptBytecodeVarHandle> VarHandles;
//     std::vector<FxScriptBytecodeActionHandle> ActionHandles;

// private:
//     FxScriptRegisterFlag mRegsInUse = FX_REGFLAG_NONE;

//     int64 mStackOffset = 0;
//     uint32 mStackSize = 0;

//     uint16 mScopeIndex = 0;
// };

// class FxScriptBCPrinter
// {
// public:
//     FxScriptBCPrinter(FxPagedArray<uint8>& bytecode)
//     {
//         mBytecode = bytecode;
//         mBytecode.bDoNotDestroy = true;
//     }

//     void Print();
//     void PrintOp();


// private:
//     uint16 Read16();
//     uint32 Read32();

//     void DoPush(char* s, uint8 op_base, uint8 op_spec);
//     void DoPop(char* s, uint8 op_base, uint8 op_spec);
//     void DoLoad(char* s, uint8 op_base, uint8 op_spec);
//     void DoArith(char* s, uint8 op_base, uint8 op_spec);
//     void DoSave(char* s, uint8 op_base, uint8 op_spec);
//     void DoJump(char* s, uint8 op_base, uint8 op_spec);
//     void DoData(char* s, uint8 op_base, uint8 op_spec);
//     void DoType(char* s, uint8 op_base, uint8 op_spec);
//     void DoMove(char* s, uint8 op_base, uint8 op_spec);

// private:
//     uint32 mBytecodeIndex = 0;
//     FxPagedArray<uint8> mBytecode;
// };


// ///////////////////////////////////////////
// // Bytecode VM
// ///////////////////////////////////////////

// struct FxScriptVMCallFrame
// {
//     uint32 StartStackIndex = 0;
// };

// class FxScriptVM
// {
// public:
//     FxScriptVM() = default;

//     void Start(FxPagedArray<uint8>&& bytecode)
//     {
//         mBytecode = std::move(bytecode);
//         mPushedTypes.Create(64);

//         Stack = FX_SCRIPT_ALLOC_MEMORY(uint8, 1024);
//         // mStackOffset = 0;
//         Registers[FX_REG_SP] = 0;
//         memset(Registers, 0, sizeof(Registers));

//         while (mPC < mBytecode.Size()) {
//             ExecuteOp();
//         }

//         PrintRegisters();
//     }

//     void PrintRegisters();

//     void Push16(uint16 value);
//     void Push32(uint32 value);

//     uint32 Pop32();

// private:
//     void ExecuteOp();

//     void DoPush(uint8 op_base, uint8 op_spec);
//     void DoPop(uint8 op_base, uint8 op_spec);
//     void DoLoad(uint8 op_base, uint8 op_spec);
//     void DoArith(uint8 op_base, uint8 op_spec);
//     void DoSave(uint8 op_base, uint8 op_spec);
//     void DoJump(uint8 op_base, uint8 op_spec);
//     void DoData(uint8 op_base, uint8 op_spec);
//     void DoType(uint8 op_base, uint8 op_spec);
//     void DoMove(uint8 op_base, uint8 op_spec);

//     uint16 Read16();
//     uint32 Read32();

//     FxScriptVMCallFrame& PushCallFrame();
//     FxScriptVMCallFrame* GetCurrentCallFrame();
//     void PopCallFrame();

//     FxScriptExternalFunc* FindExternalAction(FxHash32 hashed_name);

// public:
//     // NONE, X0, X1, X2, X3, RA, XR, SP
//     int32 Registers[FX_REG_SIZE];

//     uint8* Stack = nullptr;

//     std::vector<FxScriptExternalFunc> mExternalFuncs;

//     FxPagedArray<uint8> mBytecode;

// private:
//     uint32 mPC = 0;


//     bool mIsInCallFrame = false;

//     FxScriptVMCallFrame mCallFrames[8];
//     int mCallFrameIndex = 0;

//     bool mIsInParams = false;
//     FxPagedArray<FxScriptValue::ValueType> mPushedTypes;

//     FxScriptValue::ValueType mCurrentType = FxScriptValue::NONETYPE;
// };

// ////////////////////////////////////////////////
// // Script Interpreter
// ////////////////////////////////////////////////

// class FxScriptInterpreter
// {
// public:
//     FxScriptInterpreter() = default;

//     void PushScope();
//     void PopScope();

//     FxScriptVar* FindVar(FxHash32 hashed_name);
//     FxScriptAction* FindAction(FxHash32 hashed_name);
//     FxScriptExternalFunc* FindExternalAction(FxHash32 hashed_name);

//     /**
//      * @brief Evaluates and gets the immediate value if `value` is a reference, or returns the value if it is already
//      * immediate.
//      * @param value The value to query from
//      * @return the immediate(literal) value
//      */
//     const FxScriptValue& GetImmediateValue(const FxScriptValue& value);

//     void DefineExternalVar(const char* type, const char* name, const FxScriptValue& value);

// private:
//     friend class FxConfigScript;
//     void Create(FxAstBlock* root_block);

//     void Visit(FxAstNode* node);

//     void Interpret();

//     FxScriptValue VisitExternalCall(FxAstActionCall* call, FxScriptExternalFunc& func);
//     FxScriptValue VisitActionCall(FxAstActionCall* call);
//     void VisitAssignment(FxAstAssign* assign);
//     FxScriptValue VisitRhs(FxAstNode* node);

//     bool CheckExternalCallArgs(FxAstActionCall* call, FxScriptExternalFunc& func);


// private:
//     FxAstNode* mRootBlock = nullptr;

//     bool mInCommandMode = false;

//     std::vector<FxScriptExternalFunc> mExternalFuncs;

//     FxPagedArray<FxScriptScope> mScopes;
//     FxScriptScope* mCurrentScope = nullptr;
// };
