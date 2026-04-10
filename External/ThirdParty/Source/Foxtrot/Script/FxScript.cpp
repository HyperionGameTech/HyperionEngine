// #include "FxScript.hpp"

// #include <stdio.h>

// #include <Core/FxMemory.hpp>
// #include <Core/FxTypes.hpp>
// #include <Core/FxUtil.hpp>

// #define FX_SCRIPT_SCOPE_GLOBAL_VARS_START_SIZE 32
// #define FX_SCRIPT_SCOPE_LOCAL_VARS_START_SIZE  16

// #define FX_SCRIPT_SCOPE_GLOBAL_ACTIONS_START_SIZE 32
// #define FX_SCRIPT_SCOPE_LOCAL_ACTIONS_START_SIZE  32

// using Token = FxTokenizer::Token;
// using TT = FxTokenizer::TokenType;

// FxScriptValue FxScriptValue::None {};

// void FxConfigScript::LoadFile(const char* path)
// {
//     FILE* fp = FxUtil::FileOpen(path, "rb");
//     if (fp == nullptr) {
//         printf("[ERROR] Could not open config file at '%s'\n", path);
//         return;
//     }

//     std::fseek(fp, 0, SEEK_END);
//     size_t file_size = std::ftell(fp);
//     std::rewind(fp);

//     mFileData = FX_SCRIPT_ALLOC_MEMORY(char, file_size);

//     size_t read_size = std::fread(mFileData, 1, file_size, fp);
//     if (read_size != file_size) {
//         printf("[WARNING] Error reading all data from config file at '%s' "
//                "(read=%zu, size=%zu)\n",
//                path, read_size, file_size);
//     }

//     FxTokenizer tokenizer(mFileData, read_size);
//     tokenizer.Tokenize();

//     fclose(fp);

//     mTokens = std::move(tokenizer.GetTokens());

//     /*for (const auto& token : mTokens) {
//         token.Print();
//     }*/

//     mScopes.Create(8);
//     mCurrentScope = mScopes.Insert();
//     mCurrentScope->Vars.Create(FX_SCRIPT_SCOPE_GLOBAL_VARS_START_SIZE);
//     mCurrentScope->Actions.Create(FX_SCRIPT_SCOPE_GLOBAL_ACTIONS_START_SIZE);
// }

// Token& FxConfigScript::GetToken(int offset)
// {
//     const uint32 idx = mTokenIndex + offset;
//     if (idx < 0 || idx >= mTokens.Size()) {
//         printf("SOMETHING IS MISSING\n");
//     }
//     assert(idx >= 0 && idx <= mTokens.Size());
//     return mTokens[idx];
// }

// Token& FxConfigScript::EatToken(TT token_type)
// {
//     Token& token = GetToken();
//     if (token.Type != token_type) {
//         printf("[ERROR] %u:%u: Unexpected token type %s when expecting %s!\n", token.FileLine, token.FileColumn,
//                FxTokenizer::GetTypeName(token.Type), FxTokenizer::GetTypeName(token_type));
//         mHasErrors = true;
//     }
//     ++mTokenIndex;
//     return token;
// }

// void PrintDocCommentExample(FxTokenizer::Token* comment, int example_tag_length, bool is_command_mode)
// {
//     char* start = comment->Start + example_tag_length;

//     for (int i = 0; i < comment->Length - example_tag_length; i++) {
//         char ch = start[i];

//         if (!is_command_mode) {
//             putchar(ch);
//             continue;
//         }

//         // If we are in command mode, print the example using the command syntax
//         if (ch == '(' || ch == ')' || ch == ',') {
//             putchar(' ');
//             continue;
//         }

//         if (ch == ' ' || ch == '\t' || ch == ';') {
//             continue;
//         }

//         putchar(ch);
//     }

//     puts("");
// }

// void PrintDocComment(FxTokenizer::Token* comment, bool is_command_mode)
// {
//     const char* example_tag = "EX: ";
//     const int example_tag_length = 4;

//     char* start = comment->Start;

//     if (comment->Length > example_tag_length && !strncmp(start, example_tag, example_tag_length)) {
//         printf("\n\t-> Script : ");
//         PrintDocCommentExample(comment, example_tag_length, false);

//         printf("\t-> Command: ");
//         PrintDocCommentExample(comment, example_tag_length, true);

//         return;
//     }

//     printf("%.*s\n", comment->Length, start);
// }

// FxAstNode* FxConfigScript::TryParseKeyword()
// {
//     if (mTokenIndex >= mTokens.Size()) {
//         return nullptr;
//     }

//     Token& tk = GetToken();
//     FxHash32 hash = tk.GetHash();

//     // action [name] ( < [arg type] [arg name] ...> ) { <statements...> }
//     constexpr FxHash32 kw_action = FxHashStr32("action");

//     // local [type] [name] <?assignment> ;
//     constexpr FxHash32 kw_local = FxHashStr32("local");

//     // global [type] [name] <?assignment> ;
//     constexpr FxHash32 kw_global = FxHashStr32("global");

//     // return ;
//     constexpr FxHash32 kw_return = FxHashStr32("return");

//     // help [name of action] ;
//     constexpr FxHash32 kw_help = FxHashStr32("help");

//     if (hash == kw_action) {
//         EatToken(TT::Identifier);
//         // ParseActionDeclare();
//         return ParseActionDeclare();
//     }
//     if (hash == kw_local) {
//         EatToken(TT::Identifier);
//         return ParseVarDeclare();
//     }
//     if (hash == kw_global) {
//         EatToken(TT::Identifier);
//         return ParseVarDeclare(&mScopes[0]);
//     }
//     if (hash == kw_return) {
//         EatToken(TT::Identifier);
//         FxAstReturn* ret = FX_SCRIPT_ALLOC_NODE(FxAstReturn);
//         return ret;
//     }
//     if (hash == kw_help) {
//         EatToken(TT::Identifier);

//         FxTokenizer::Token& func_ref = EatToken(TT::Identifier);

//         FxScriptAction* action = FindAction(func_ref.GetHash());

//         if (action) {
//             for (FxAstDocComment* comment : action->Declaration->DocComments) {
//                 printf("[DOC] %.*s: ", action->Name->Length, action->Name->Start);
//                 PrintDocComment(comment->Comment, mInCommandMode);
//             }
//         }

//         return nullptr;
//     }

//     return nullptr;
// }

// void FxConfigScript::PushScope()
// {
//     FxScriptScope* current = mCurrentScope;

//     FxScriptScope* new_scope = mScopes.Insert();
//     new_scope->Parent = current;
//     new_scope->Vars.Create(FX_SCRIPT_SCOPE_LOCAL_VARS_START_SIZE);
//     new_scope->Actions.Create(FX_SCRIPT_SCOPE_LOCAL_ACTIONS_START_SIZE);

//     mCurrentScope = new_scope;
// }

// void FxConfigScript::PopScope()
// {
//     FxScriptScope* new_scope = mCurrentScope->Parent;
//     mScopes.RemoveLast();

//     assert(new_scope == &mScopes.GetLast());

//     mCurrentScope = new_scope;
// }

// FxAstVarDecl* FxConfigScript::ParseVarDeclare(FxScriptScope* scope)
// {
//     if (scope == nullptr) {
//         scope = mCurrentScope;
//     }

//     Token& type = EatToken(TT::Identifier);
//     Token& name = EatToken(TT::Identifier);

//     FxAstVarDecl* node = FX_SCRIPT_ALLOC_NODE(FxAstVarDecl);

//     node->Name = &name;
//     node->Type = &type;
//     node->DefineAsGlobal = (scope == &mScopes[0]);

//     FxScriptVar var { &type, &name, scope };

//     node->Assignment = TryParseAssignment(node->Name);
//     /*if (node->Assignment) {
//         var.Value = node->Assignment->Value;
//     }*/
//     mCurrentScope->Vars.Insert(var);

//     return node;
// }

// // FxScriptVar& FxConfigScript::ParseVarDeclare()
// //{
// //     Token& type = EatToken(TT::Identifier);
// //     Token& name = EatToken(TT::Identifier);
// //
// //     FxScriptVar var{ name.GetHash(), &type, &name };
// //
// //     TryParseAssignment(var);
// //
// //     mCurrentScope->Vars.Insert(var);
// //
// //     return mCurrentScope->Vars.GetLast();
// // }

// FxScriptVar* FxConfigScript::FindVar(FxHash32 hashed_name)
// {
//     FxScriptScope* scope = mCurrentScope;

//     while (scope) {
//         FxScriptVar* var = scope->FindVarInScope(hashed_name);
//         if (var) {
//             return var;
//         }

//         scope = scope->Parent;
//     }

//     return nullptr;
// }

// FxScriptExternalFunc* FxConfigScript::FindExternalAction(FxHash32 hashed_name)
// {
//     for (FxScriptExternalFunc& func : mExternalFuncs) {
//         if (func.HashedName == hashed_name) {
//             return &func;
//         }
//     }

//     return nullptr;
// }

// FxScriptAction* FxConfigScript::FindAction(FxHash32 hashed_name)
// {
//     FxScriptScope* scope = mCurrentScope;

//     while (scope) {
//         FxScriptAction* var = scope->FindActionInScope(hashed_name);
//         if (var) {
//             return var;
//         }

//         scope = scope->Parent;
//     }

//     return nullptr;
// }

// void FxConfigScript::Execute(FxScriptVM& vm)
// {
//     DefineDefaultExternalFunctions();

//     mRootBlock = Parse();

//     // If there are errors, exit early
//     if (mHasErrors || mRootBlock == nullptr) {
//         return;
//     }
//     printf("\n=====\n");
//     FxScriptBCEmitter emitter;
//     emitter.BeginEmitting(mRootBlock);

//     printf("\n=====\n");

//     FxScriptBCPrinter printer(emitter.mBytecode);
//     printer.Print();

//     printf("\n=====\n");

//     for (FxScriptBytecodeVarHandle& handle : emitter.VarHandles) {
//         printf("Var(%u) AT %lld\n", handle.HashedName, handle.Offset);
//     }

//     // return;

//     vm.mExternalFuncs = mExternalFuncs;
//     vm.Start(std::move(emitter.mBytecode));

//     for (FxScriptBytecodeVarHandle& handle : emitter.VarHandles) {
//         printf("Var(%u) AT %lld -> %u\n", handle.HashedName, handle.Offset, vm.Stack[handle.Offset]);
//     }

//     return;
//     /*
//     interpreter.Create(mRootBlock);

//     // Copy any external variable declarations from the parser to the interpreter
//     FxScriptScope& global_scope = mScopes[0];
//     FxScriptScope& interpreter_global_scope = interpreter.mScopes[0];

//     for (FxScriptVar& var : global_scope.Vars) {
//         if (var.IsExternal) {
//             interpreter_global_scope.Vars.Insert(var);
//         }
//     }

//     interpreter.mExternalFuncs = mExternalFuncs;

//     interpreter.Interpret();*/
// }

// bool FxConfigScript::ExecuteUserCommand(const char* command, FxScriptInterpreter& interpreter)
// {
//     mHasErrors = false;

//     // If there are errors, exit early
//     if (mRootBlock == nullptr) {
//         return false;
//     }

//     uint32 new_token_index = mTokens.Size();

//     uint32 old_token_index = mTokenIndex;

//     {
//         uint32 length_of_command = strlen(command);

//         char* data_buffer = FX_SCRIPT_ALLOC_MEMORY(char, length_of_command + 1);
//         memcpy(data_buffer, command, length_of_command);

//         FxTokenizer tokenizer(data_buffer, length_of_command);
//         tokenizer.Tokenize();

//         for (FxTokenizer::Token& token : tokenizer.GetTokens()) {
//             // token.Print();
//             mTokens.Insert(token);
//         }

//         /*for (FxTokenizer::Token& token : mTokens) {
//             token.Print();
//         }*/
//     }

//     mTokenIndex = new_token_index;
//     mInCommandMode = true;

//     FxAstBlock* root_block = FX_SCRIPT_ALLOC_NODE(FxAstBlock);

//     FxAstNode* statement;
//     while ((statement = ParseStatementAsCommand())) {
//         root_block->Statements.push_back(statement);
//     }

//     // FxAstNode* node = ParseStatementAsCommand();

//     // Revert the current state if there have been errors
//     if (root_block == nullptr) {
//         printf("Ignoring state...\n");
//         // Get the size of the tokens
//         // int current_token_index = mTokens.Size();
//         for (int i = new_token_index; i < new_token_index; i++) {
//             mTokens.RemoveLast();
//         }

//         mTokenIndex = old_token_index;

//         mInCommandMode = false;

//         // Since we have reverted state, clear errors flag
//         mHasErrors = false;

//         return false;
//     }

// #if 0

//     // Run the new block
//     {
//         interpreter.mInCommandMode = true;
//         interpreter.Visit(root_block);
//         interpreter.mInCommandMode = false;
//     }
// #endif
//     mInCommandMode = false;

//     return true;
// }

// FxScriptValue FxConfigScript::ParseValue()
// {
//     Token& token = GetToken();
//     TT token_type = token.Type;
//     FxScriptValue value;

//     if (token_type == TT::Identifier) {
//         FxScriptVar* var = FindVar(token.GetHash());

//         if (var) {
//             value.Type = FxScriptValue::REF;

//             FxAstVarRef* var_ref = FX_SCRIPT_ALLOC_NODE(FxAstVarRef);
//             var_ref->Name = var->Name;
//             var_ref->Scope = var->Scope;

//             value.ValueRef = var_ref;

//             EatToken(TT::Identifier);

//             return value;
//         }
//         else {
//             // We cannot find the definition for the variable, assume that it is an
//             // external variable that will be defined during the interpret stage.

//             printf("ERROR: Undefined reference to variable ");
//             token.Print();
//             printf("\n");

//             // printf("Undefined reference to variable \"%.*s\"! (Hash:%u)\n",
//             // token.Length, token.Start, token.GetHash());
//             EatToken(TT::Identifier);
//         }
//     }

//     switch (token_type) {
//     case TT::Integer:
//         EatToken(TT::Integer);
//         value.Type = FxScriptValue::INT;
//         value.ValueInt = token.ToInt();
//         break;
//     case TT::Float:
//         EatToken(TT::Float);
//         value.Type = FxScriptValue::FLOAT;
//         value.ValueFloat = token.ToFloat();
//         break;
//     case TT::String:
//         EatToken(TT::String);
//         value.Type = FxScriptValue::STRING;
//         value.ValueString = token.GetHeapStr();
//         break;
//     default:;
//     }

//     return value;
// }

// #define RETURN_IF_NO_TOKENS(rval_)                                                                                     \
//     {                                                                                                                  \
//         if (mTokenIndex >= mTokens.Size())                                                                             \
//             return (rval_);                                                                                            \
//     }

// static bool IsTokenTypeLiteral(FxTokenizer::TokenType type)
// {
//     return (type == TT::Integer || type == TT::Float || type == TT::String);
// }

// FxAstNode* FxConfigScript::ParseRhs()
// {
//     RETURN_IF_NO_TOKENS(nullptr);

//     bool has_parameters = false;

//     if (mTokenIndex + 1 < mTokens.Size()) {
//         TT next_token_type = GetToken(1).Type;
//         has_parameters = next_token_type == TT::LParen;

//         if (mInCommandMode) {
//             has_parameters = next_token_type == TT::Identifier || next_token_type == TT::Integer ||
//                              next_token_type == TT::Float || next_token_type == TT::String;
//         }
//     }

//     FxTokenizer::Token& token = GetToken();

//     FxAstNode* lhs = nullptr;

//     if (token.Type == TT::Identifier) {
//         if (has_parameters) {
//             lhs = ParseActionCall();
//         }
//         else {
//             FxScriptExternalFunc* external_action = FindExternalAction(token.GetHash());
//             if (external_action != nullptr) {
//                 lhs = ParseActionCall();
//             }

//             FxScriptAction* action = FindAction(token.GetHash());
//             if (action != nullptr) {
//                 lhs = ParseActionCall();
//             }
//         }
//     }
//     if (!lhs) {
//         if (IsTokenTypeLiteral(token.Type) || token.Type == TT::Identifier) {
//             FxAstLiteral* literal = FX_SCRIPT_ALLOC_NODE(FxAstLiteral);

//             FxScriptValue value = ParseValue();
//             literal->Value = value;

//             lhs = literal;
//         }
//         else {
//             lhs = ParseRhs();
//         }
//     }

//     // FxAstLiteral* literal = FX_SCRIPT_ALLOC_NODE(FxAstLiteral);
//     // literal->Value = value;

//     TT op_type = GetToken(0).Type;
//     if (op_type == TT::Plus || op_type == TT::Minus) {
//         FxAstBinop* binop = FX_SCRIPT_ALLOC_NODE(FxAstBinop);

//         binop->Left = lhs;
//         binop->OpToken = &EatToken(op_type);
//         binop->Right = ParseRhs();

//         return binop;
//     }

//     return lhs;

//     /*
//     RETURN_IF_NO_TOKENS(nullptr);

//     bool has_parameters = false;

//     if (mTokenIndex + 1 < mTokens.Size()) {
//         TT next_token_type = GetToken(1).Type;
//         has_parameters = next_token_type == TT::LParen;

//         if (mInCommandMode) {
//             has_parameters = next_token_type == TT::Identifier || next_token_type
//     == TT::Integer || next_token_type == TT::Float || next_token_type ==
//     TT::String;
//         }
//     }

//     FxTokenizer::Token& token = GetToken();

//     FxAstNode* lhs = nullptr;

//     if (token.Type == TT::Identifier) {
//         if (has_parameters) {
//             lhs = ParseActionCall();
//         }
//         else {
//             FxScriptExternalFunc* external_action =
//     FindExternalAction(token.GetHash()); if (external_action != nullptr) { return
//     ParseActionCall();
//             }

//             FxScriptAction* action = FindAction(token.GetHash());
//             if (action != nullptr) {
//                 return ParseActionCall();
//             }

//         }
//     }

//     else if (IsTokenTypeLiteral(token.Type) || token.Type == TT::Identifier) {
//         FxAstLiteral* literal = FX_SCRIPT_ALLOC_NODE(FxAstLiteral);
//         literal->Value = ParseValue();
//         lhs = literal;
//     }

//     TT op_type = GetToken(0).Type;
//     if (op_type == TT::Plus || op_type == TT::Minus) {
//         FxAstBinop* binop = FX_SCRIPT_ALLOC_NODE(FxAstBinop);

//         binop->Left = lhs;
//         binop->OpToken = &EatToken(op_type);
//         binop->Right = ParseRhs();

//         return binop;
//     }

//     return lhs;
//     */
// }

// FxAstAssign* FxConfigScript::TryParseAssignment(FxTokenizer::Token* var_name)
// {
//     if (GetToken().Type != TT::Equals) {
//         return nullptr;
//     }

//     EatToken(TT::Equals);

//     FxAstAssign* node = FX_SCRIPT_ALLOC_NODE(FxAstAssign);

//     FxAstVarRef* var_ref = FX_SCRIPT_ALLOC_NODE(FxAstVarRef);
//     var_ref->Name = var_name;
//     var_ref->Scope = mCurrentScope;
//     node->Var = var_ref;

//     // node->Value = ParseValue();
//     node->Rhs = ParseRhs();

//     return node;
// }

// void FxConfigScript::DefineExternalVar(const char* type, const char* name, const FxScriptValue& value)
// {
//     Token* name_token = FX_SCRIPT_ALLOC_MEMORY(Token, sizeof(Token));
//     Token* type_token = FX_SCRIPT_ALLOC_MEMORY(Token, sizeof(Token));

//     {
//         const uint32 type_len = strlen(type);

//         char* type_buffer = FX_SCRIPT_ALLOC_MEMORY(char, (type_len + 1));
//         strcpy(type_buffer, type);

//         type_token->Start = type_buffer;
//         type_token->Type = TT::Identifier;
//         type_token->Start[type_len] = 0;
//         type_token->Length = type_len + 1;
//     }

//     {
//         const uint32 name_len = strlen(name);

//         char* name_buffer = FX_SCRIPT_ALLOC_MEMORY(char, (name_len + 1));
//         strcpy(name_buffer, name);

//         name_token->Start = name_buffer;
//         name_token->Type = TT::Identifier;
//         name_token->Start[name_len] = 0;
//         name_token->Length = name_len + 1;
//     }

//     FxScriptScope* definition_scope = &mScopes[0];

//     FxScriptVar var(type_token, name_token, definition_scope, true);
//     var.Value = value;

//     definition_scope->Vars.Insert(var);

//     // To prevent the variable data from being deleted here.
//     var.Name = nullptr;
//     var.Type = nullptr;
// }

// FxAstNode* FxConfigScript::ParseStatementAsCommand()
// {
//     if (mHasErrors) {
//         return nullptr;
//     }

//     RETURN_IF_NO_TOKENS(nullptr);

//     // Eat any extraneous semicolons
//     while (GetToken().Type == TT::Semicolon) {
//         EatToken(TT::Semicolon);
//         if (mTokenIndex >= mTokens.Size()) {
//             return nullptr;
//         }
//     }

//     // Try to parse as a keyword first
//     FxAstNode* node = TryParseKeyword();

//     if (node) {
//         RETURN_IF_NO_TOKENS(node);

//         EatToken(TT::Semicolon);
//         return node;
//     }

//     // Check identifier
//     if (mTokenIndex < mTokens.Size() && GetToken().Type == TT::Identifier) {
//         TT next_token_type = TT::Unknown;

//         if (mTokenIndex + 1 < mTokens.Size()) {
//             next_token_type = GetToken(1).Type;
//         }

//         if (mTokenIndex + 1 < mTokens.Size() && GetToken(1).Type == TT::Equals) {
//             Token& assign_name = EatToken(TT::Identifier);
//             node = TryParseAssignment(&assign_name);
//         }
//         // If there is what looks to be a function call, try it
//         else {
//             FxScriptExternalFunc* external_action = FindExternalAction(GetToken().GetHash());
//             if (external_action != nullptr) {
//                 node = ParseActionCall();
//             }
//             else {
//                 FxScriptAction* action = FindAction(GetToken().GetHash());
//                 if (action != nullptr) {
//                     node = ParseActionCall();
//                 }
//             }
//         }
//         // If there is just a semicolon after an identifier, parse as an rhs to
//         // print out later
//         if (!node && next_token_type == TT::Semicolon) {
//             node = ParseRhs();
//         }
//     }

//     RETURN_IF_NO_TOKENS(node);

//     EatToken(TT::Semicolon);

//     return node;
// }

// FxAstNode* FxConfigScript::ParseStatement()
// {
//     if (mHasErrors) {
//         return nullptr;
//     }

//     RETURN_IF_NO_TOKENS(nullptr);

//     if (GetToken().Type == TT::Dollar) {
//         EatToken(TT::Dollar);

//         mInCommandMode = true;

//         FxAstCommandMode* cmd_mode = FX_SCRIPT_ALLOC_NODE(FxAstCommandMode);
//         cmd_mode->Node = ParseStatementAsCommand();

//         mInCommandMode = false;

//         return cmd_mode;
//     }

//     while (GetToken().Type == TT::DocComment) {
//         FxAstDocComment* comment = FX_SCRIPT_ALLOC_NODE(FxAstDocComment);
//         comment->Comment = &EatToken(TT::DocComment);
//         CurrentDocComments.push_back(comment);

//         if (mTokenIndex >= mTokens.Size()) {
//             return nullptr;
//         }
//     }

//     // Eat any extraneous semicolons
//     while (GetToken().Type == TT::Semicolon) {
//         EatToken(TT::Semicolon);

//         if (mTokenIndex >= mTokens.Size()) {
//             return nullptr;
//         }
//     }

//     FxAstNode* node = TryParseKeyword();

//     if (!node && (mTokenIndex < mTokens.Size() && GetToken().Type == TT::Identifier)) {
//         if (mTokenIndex + 1 < mTokens.Size() && GetToken(1).Type == TT::LParen) {
//             node = ParseActionCall();
//         }
//         else if (mTokenIndex + 1 < mTokens.Size() && GetToken(1).Type == TT::Equals) {
//             Token& assign_name = EatToken(TT::Identifier);
//             node = TryParseAssignment(&assign_name);
//         }
//         else {
//             GetToken().Print();
//         }
//     }

//     if (!node) {
//         return nullptr;
//     }

//     // Blocks do not require semicolons
//     if (node->NodeType == FX_AST_BLOCK || node->NodeType == FX_AST_ACTIONDECL) {
//         return node;
//     }

//     EatToken(TT::Semicolon);

//     return node;
// }

// FxAstBlock* FxConfigScript::ParseBlock()
// {
//     bool in_command = mInCommandMode;
//     mInCommandMode = false;

//     FxAstBlock* block = FX_SCRIPT_ALLOC_NODE(FxAstBlock);

//     EatToken(TT::LBrace);

//     while (GetToken().Type != TT::RBrace) {
//         FxAstNode* command = ParseStatement();
//         if (command == nullptr) {
//             break;
//         }
//         block->Statements.push_back(command);
//     }

//     EatToken(TT::RBrace);

//     mInCommandMode = in_command;
//     return block;
// }

// FxAstActionDecl* FxConfigScript::ParseActionDeclare()
// {
//     FxAstActionDecl* node = FX_SCRIPT_ALLOC_NODE(FxAstActionDecl);

//     if (!CurrentDocComments.empty()) {
//         node->DocComments = CurrentDocComments;
//         CurrentDocComments.clear();
//     }

//     // Name of the action
//     Token& name = EatToken(TT::Identifier);

//     node->Name = &name;

//     PushScope();
//     EatToken(TT::LParen);

//     FxAstBlock* params = FX_SCRIPT_ALLOC_NODE(FxAstBlock);

//     while (GetToken().Type != TT::RParen) {
//         params->Statements.push_back(ParseVarDeclare());

//         if (GetToken().Type == TT::Comma) {
//             EatToken(TT::Comma);
//             continue;
//         }

//         break;
//     }

//     EatToken(TT::RParen);

//     if (GetToken().Type != TT::LBrace) {
//         FxAstVarDecl* return_decl = ParseVarDeclare();
//         node->ReturnVar = return_decl;
//     }

//     node->Block = ParseBlock();
//     PopScope();

//     node->Params = params;

//     FxScriptAction action(&name, mCurrentScope, node->Block, node);
//     mCurrentScope->Actions.Insert(action);

//     return node;
// }

// // FxScriptValue FxConfigScript::TryCallInternalFunc(FxHash32 func_name,
// // std::vector<FxScriptValue>& params)
// //{
// //     FxScriptValue return_value;
// //
// //     for (const FxScriptInternalFunc& func : mInternalFuncs) {
// //         if (func.HashedName == func_name) {
// //             func.Func(params, &return_value);
// //             return return_value;
// //         }
// //     }
// //
// //     return return_value;
// // }

// FxAstActionCall* FxConfigScript::ParseActionCall()
// {
//     FxAstActionCall* node = FX_SCRIPT_ALLOC_NODE(FxAstActionCall);

//     Token& name = EatToken(TT::Identifier);

//     node->HashedName = name.GetHash();
//     node->Action = FindAction(node->HashedName);

//     TT end_token_type = TT::Semicolon;

//     if (!mInCommandMode) {
//         end_token_type = TT::RParen;
//     }

//     if (!mInCommandMode || GetToken().Type == TT::LParen) {
//         EatToken(TT::LParen);
//     }

//     while (GetToken().Type != end_token_type) {
//         FxAstNode* param = ParseRhs();

//         if (param == nullptr) {
//             break;
//         }

//         node->Params.push_back(param);

//         TT next_tt = GetToken().Type;

//         if (GetToken().Type == TT::Comma) {
//             EatToken(TT::Comma);
//             continue;
//         }

//         if (mInCommandMode && (next_tt != TT::RParen && next_tt != TT::Semicolon)) {
//             continue;
//         }

//         break;
//     }

//     if (!mInCommandMode || GetToken().Type == TT::RParen) {
//         EatToken(TT::RParen);
//     }

//     return node;
// }

// // void FxConfigScript::ParseDoCall()
// //{
// //     Token& call_name = EatToken(TT::Identifier);
// //
// //     EatToken(TT::LParen);
// //
// //     std::vector<FxScriptValue> params;
// //
// //     while (true) {
// //         params.push_back(ParseValue());
// //
// //         if (GetToken().Type == TT::Comma) {
// //             EatToken(TT::Comma);
// //             continue;
// //         }
// //
// //         break;
// //     }
// //
// //     EatToken(TT::RParen);
// //
// //     TryCallInternalFunc(call_name.GetHash(), params);
// //
// //     printf("Calling ");
// //     call_name.Print();
// //
// //     for (const auto& param : params) {
// //         printf("param : ");
// //         param.Print();
// //     }
// // }

// FxAstBlock* FxConfigScript::Parse()
// {
//     FxAstBlock* root_block = FX_SCRIPT_ALLOC_NODE(FxAstBlock);

//     FxAstNode* keyword;
//     while ((keyword = ParseStatement())) {
//         root_block->Statements.push_back(keyword);
//     }

//     /*for (const auto& var : mCurrentScope->Vars) {
//         var.Print();
//     }*/

//     if (mHasErrors) {
//         return nullptr;
//     }

//     FxAstPrinter printer(root_block);
//     printer.Print(root_block);

//     return root_block;
// }

// //////////////////////////////////////////
// // Script Interpreter
// //////////////////////////////////////////
// // #if 0
// void FxScriptInterpreter::Create(FxAstBlock* root_block)
// {
//     mRootBlock = root_block;

//     mScopes.Create(8);
//     mCurrentScope = mScopes.Insert();
//     mCurrentScope->Parent = nullptr;
//     mCurrentScope->Vars.Create(FX_SCRIPT_SCOPE_LOCAL_VARS_START_SIZE);
//     mCurrentScope->Actions.Create(FX_SCRIPT_SCOPE_LOCAL_ACTIONS_START_SIZE);
// }

// void FxScriptInterpreter::Interpret()
// {
//     Visit(mRootBlock);

//     mScopes[0].PrintAllVarsInScope();
// }

// void FxScriptInterpreter::PushScope()
// {
//     FxScriptScope* current = mCurrentScope;

//     FxScriptScope* new_scope = mScopes.Insert();
//     new_scope->Parent = current;
//     new_scope->Vars.Create(FX_SCRIPT_SCOPE_LOCAL_VARS_START_SIZE);
//     new_scope->Actions.Create(FX_SCRIPT_SCOPE_LOCAL_ACTIONS_START_SIZE);

//     mCurrentScope = new_scope;
// }

// void FxScriptInterpreter::PopScope()
// {
//     FxScriptScope* new_scope = mCurrentScope->Parent;
//     mScopes.RemoveLast();

//     assert(new_scope == &mScopes.GetLast());

//     mCurrentScope = new_scope;
// }

// FxScriptVar* FxScriptInterpreter::FindVar(FxHash32 hashed_name)
// {
//     FxScriptScope* scope = mCurrentScope;

//     while (scope) {
//         FxScriptVar* var = scope->FindVarInScope(hashed_name);
//         if (var) {
//             return var;
//         }

//         scope = scope->Parent;
//     }

//     return nullptr;
// }

// FxScriptAction* FxScriptInterpreter::FindAction(FxHash32 hashed_name)
// {
//     FxScriptScope* scope = mCurrentScope;

//     while (scope) {
//         FxScriptAction* var = scope->FindActionInScope(hashed_name);
//         if (var) {
//             return var;
//         }

//         scope = scope->Parent;
//     }

//     return nullptr;
// }

// FxScriptExternalFunc* FxScriptInterpreter::FindExternalAction(FxHash32 hashed_name)
// {
//     for (FxScriptExternalFunc& func : mExternalFuncs) {
//         if (func.HashedName == hashed_name) {
//             return &func;
//         }
//     }
//     return nullptr;
// }

// bool FxScriptInterpreter::CheckExternalCallArgs(FxAstActionCall* call, FxScriptExternalFunc& func)
// {
//     if (func.IsVariadic) {
//         return true;
//     }

//     if (call->Params.size() != func.ParameterTypes.size()) {
//         return false;
//     }

//     for (int i = 0; i < call->Params.size(); i++) {
//         FxScriptValue val = VisitRhs(call->Params[i]);

//         if (!(val.Type & func.ParameterTypes[i])) {
//             return false;
//         }
//     }

//     return true;
// }

// FxScriptValue FxScriptInterpreter::VisitExternalCall(FxAstActionCall* call, FxScriptExternalFunc& func)
// {
//     FxScriptValue return_value;

//     // PushScope();

//     if (!CheckExternalCallArgs(call, func)) {
//         printf("!!! Parameters do not match for function call!\n");
//         PopScope();
//         return return_value;
//     }

//     std::vector<FxScriptValue> params;
//     params.reserve(call->Params.size());

//     for (FxAstNode* param_node : call->Params) {
//         params.push_back(VisitRhs(param_node));
//     }

//     // func.Function(*this, params, &return_value);

//     // PopScope();

//     return return_value;
// }

// FxScriptValue FxScriptInterpreter::VisitActionCall(FxAstActionCall* call)
// {
//     FxScriptValue return_value;

//     // This is not a local call, check for an internal function
//     if (call->Action == nullptr) {
//         for (FxScriptExternalFunc& func : mExternalFuncs) {
//             if (func.HashedName != call->HashedName) {
//                 continue;
//             }

//             return VisitExternalCall(call, func);
//         }
//     }

//     if (call->Action == nullptr) {
//         puts("!!! Could not find action!");
//         return return_value;
//     }

//     PushScope();

//     std::vector<FxAstNode*> param_decls = call->Action->Declaration->Params->Statements;

//     if (call->Params.size() != param_decls.size()) {
//         printf("!!! MISMATCHED PARAM COUNTS\n");
//         PopScope();

//         return return_value;
//     }

//     // Assign each passed in value to the each parameter declaration
//     for (int i = 0; i < param_decls.size(); i++) {
//         FxAstVarDecl* decl = reinterpret_cast<FxAstVarDecl*>(param_decls[i]);

//         FxScriptVar param(decl->Type, decl->Name, mCurrentScope);
//         param.Value = GetImmediateValue(VisitRhs(call->Params[i]));

//         mCurrentScope->Vars.Insert(param);
//     }

//     if (call->Action->Declaration->ReturnVar) {
//         FxAstVarDecl* decl = call->Action->Declaration->ReturnVar;

//         FxScriptVar return_var(decl->Type, decl->Name, mCurrentScope);
//         mCurrentScope->Vars.Insert(return_var);

//         mCurrentScope->ReturnVar = &mCurrentScope->Vars.GetLast();
//     }

//     Visit(call->Action->Block);

//     // Acquire the return value from the scope
//     if (mCurrentScope->ReturnVar) {
//         return_value = GetImmediateValue(mCurrentScope->ReturnVar->Value);
//     }

//     // mCurrentScope->PrintAllVarsInScope();

//     PopScope();

//     return return_value;
// }

// FxScriptValue FxScriptInterpreter::VisitRhs(FxAstNode* node)
// {
//     if (node->NodeType == FX_AST_LITERAL) {
//         FxAstLiteral* literal = reinterpret_cast<FxAstLiteral*>(node);
//         return literal->Value;
//     }
//     else if (node->NodeType == FX_AST_ACTIONCALL) {
//         FxAstActionCall* call = reinterpret_cast<FxAstActionCall*>(node);
//         return VisitActionCall(call);
//     }
//     else if (node->NodeType == FX_AST_BINOP) {
//         FxAstBinop* binop = reinterpret_cast<FxAstBinop*>(node);

//         FxScriptValue lhs_pre_val = VisitRhs(binop->Left);
//         FxScriptValue rhs_pre_val = VisitRhs(binop->Right);

//         FxScriptValue lhs = GetImmediateValue(lhs_pre_val);
//         FxScriptValue rhs = GetImmediateValue(rhs_pre_val);

//         float sign = (binop->OpToken->Type == TT::Plus) ? 1.0f : -1.0f;

//         FxScriptValue result;

//         if (lhs.Type == FxScriptValue::INT) {
//             result.ValueInt = lhs.ValueInt;
//             result.Type = FxScriptValue::INT;

//             if (rhs.Type == FxScriptValue::INT) {
//                 result.ValueInt += rhs.ValueInt * sign;
//             }
//             else if (rhs.Type == FxScriptValue::FLOAT) {
//                 result.ValueInt += rhs.ValueFloat * sign;
//             }
//         }
//         else if (lhs.Type == FxScriptValue::FLOAT) {
//             result.ValueFloat = lhs.ValueFloat;
//             result.Type = FxScriptValue::FLOAT;

//             if (rhs.Type == FxScriptValue::INT) {
//                 result.ValueFloat += rhs.ValueInt * sign;
//             }
//             else if (rhs.Type == FxScriptValue::FLOAT) {
//                 result.ValueFloat += rhs.ValueFloat * sign;
//             }
//         }

//         return result;
//     }

//     FxScriptValue value {};
//     return value;
// }

// void FxConfigScript::DefineDefaultExternalFunctions()
// {
//     // log([int | float | string | ref] args...)
//     RegisterExternalFunc(
//         FxHashStr32("log"), {}, // Do not check argument types as we handle it here
//         [](FxScriptVM* vm, std::vector<FxScriptValue>& args, FxScriptValue* return_value)
//         {
//             printf("[SCRIPT]: ");

//             for (int i = args.size() - 1; i >= 0; i--) {
//                 FxScriptValue& arg = args[i];

//                 // const FxScriptValue& value = interpreter.GetImmediateValue(arg);
//                 const FxScriptValue& value = arg;

//                 switch (value.Type) {
//                 case FxScriptValue::NONETYPE:
//                     printf("[none]");
//                     break;
//                 case FxScriptValue::INT:
//                     printf("%d", value.ValueInt);
//                     break;
//                 case FxScriptValue::FLOAT:
//                     printf("%f", value.ValueFloat);
//                     break;
//                 case FxScriptValue::STRING:
//                     printf("%s", value.ValueString);
//                     break;
//                 default:
//                     printf("Unknown type\n");
//                     break;
//                 }

//                 putchar(' ');
//             }

//             putchar('\n');
//         },
//         true // Is variadic?
//     );

//     // listvars()
//     // RegisterExternalFunc(
//     //    FxHashStr32("__listvars__"),
//     //    {},
//     //    [](FxScriptVM* vm, std::vector<FxScriptValue>& args, FxScriptValue*
//     //    return_value)
//     //    {
//     //        FxScriptScope* scope = interpreter.mCurrentScope;
//     //        // Since there is a new scope created on function call, we need to
//     //        start from the parent scope if (scope && scope->Parent) {
//     //            scope = scope->Parent;
//     //        }

//     //        while (scope != nullptr) {
//     //            scope->PrintAllVarsInScope();
//     //            scope = scope->Parent;
//     //        }
//     //    },
//     //    false
//     //);

//     // listactions()
//     // RegisterExternalFunc(
//     //    FxHashStr32("__listactions__"),
//     //    {},
//     //    [](FxScriptVM* vm, std::vector<FxScriptValue>& args, FxScriptValue*
//     //    return_value)
//     //    {
//     //        FxScriptScope* scope = interpreter.mCurrentScope;
//     //        // Since there is a new scope created on function call, we need to
//     //        start from the parent scope if (scope && scope->Parent) {
//     //            scope = scope->Parent;
//     //        }

//     //        while (scope != nullptr) {

//     //            for (const FxScriptAction& action : scope->Actions) {
//     //                // Print out the action name
//     //                printf("action %.*s", action.Declaration->Name->Length,
//     //                action.Declaration->Name->Start);

//     //                // Retrieve the declarations for all parameters
//     //                std::vector<FxAstNode*>& param_decls =
//     //                action.Declaration->Params->Statements; const size_t
//     //                param_count = param_decls.size();

//     //                // Print out the parameter list
//     //                putchar('(');

//     //                for (int i = 0; i < param_count; i++) {
//     //                    FxAstNode* param_node = param_decls[i];
//     //                    if (param_node->NodeType != FX_AST_VARDECL) {
//     //                        continue;
//     //                    }

//     //                    FxAstVarDecl* param_decl =
//     //                    reinterpret_cast<FxAstVarDecl*>(param_node);

//     //                    // Print the type of the parameter
//     //                    printf("%.*s ", param_decl->Type->Length,
//     //                    param_decl->Type->Start);

//     //                    // Print the name of the parameter
//     //                    printf("%.*s", param_decl->Name->Length,
//     //                    param_decl->Name->Start);

//     //                    // If there are more parameters following, output a
//     //                    comma if (i < param_count - 1) {
//     //                        printf(", ");
//     //                    }
//     //                }

//     //                putchar(')');

//     //                // Print out the return type at the end of the declaration
//     //                if it exists if (action.Declaration->ReturnVar) {
//     //                    printf(" %.*s",
//     //                    action.Declaration->ReturnVar->Type->Length,
//     //                    action.Declaration->ReturnVar->Type->Start);
//     //                }

//     //                putchar('\n');
//     //            }

//     //            scope = scope->Parent;
//     //        }
//     //    },
//     //    false
//     //);
// }

// void FxScriptInterpreter::VisitAssignment(FxAstAssign* assign)
// {
//     FxScriptVar* var = FindVar(assign->Var->Name->GetHash());

//     if (!var) {
//         printf("!!! Could not find variable!\n");
//         return;
//     }

//     constexpr FxHash32 builtin_int = FxHashStr32("int");
//     constexpr FxHash32 builtin_playerid = FxHashStr32("playerid");
//     constexpr FxHash32 builtin_float = FxHashStr32("float");
//     constexpr FxHash32 builtin_string = FxHashStr32("string");

//     FxScriptValue rhs_value = VisitRhs(assign->Rhs);
//     const FxScriptValue& new_value = GetImmediateValue(rhs_value);

//     FxScriptValue::ValueType var_type = FxScriptValue::NONETYPE;

//     FxHash32 type_hash = var->Type->GetHash();

//     switch (type_hash) {
//     case builtin_playerid:
//         [[fallthrough]];
//     case builtin_int:
//         var_type = FxScriptValue::INT;
//         break;
//     case builtin_float:
//         var_type = FxScriptValue::FLOAT;
//         break;
//     case builtin_string:
//         var_type = FxScriptValue::STRING;
//         break;
//     default:
//         printf("!!! Unknown type for variable %.*s!\n", var->Type->Length, var->Type->Start);
//         return;
//     }

//     if (var_type != new_value.Type) {
//         printf("!!! Assignment value type does not match variable type!\n");
//         return;
//     }

//     var->Value = new_value;

//     // puts("Visit Assign");
// }

// void FxScriptInterpreter::Visit(FxAstNode* node)
// {
//     if (node == nullptr) {
//         return;
//     }

//     if (node->NodeType == FX_AST_BLOCK) {
//         // puts("Visit Block");

//         FxAstBlock* block = reinterpret_cast<FxAstBlock*>(node);
//         for (FxAstNode* child : block->Statements) {
//             if (child->NodeType == FX_AST_RETURN) {
//                 break;
//             }

//             Visit(child);
//         }
//     }
//     else if (node->NodeType == FX_AST_ACTIONDECL) {
//         FxAstActionDecl* actiondecl = reinterpret_cast<FxAstActionDecl*>(node);
//         // puts("Visit ActionDecl");

//         FxScriptAction action(actiondecl->Name, mCurrentScope, actiondecl->Block, actiondecl);
//         mCurrentScope->Actions.Insert(action);

//         // Visit(actiondecl->Block);
//     }
//     else if (node->NodeType == FX_AST_VARDECL) {
//         FxAstVarDecl* vardecl = reinterpret_cast<FxAstVarDecl*>(node);
//         // puts("Visit VarDecl");

//         FxScriptScope* scope = mCurrentScope;
//         if (vardecl->DefineAsGlobal) {
//             scope = &mScopes[0];
//         }

//         FxScriptVar var(vardecl->Type, vardecl->Name, scope);
//         scope->Vars.Insert(var);

//         Visit(vardecl->Assignment);
//     }
//     else if (node->NodeType == FX_AST_ASSIGN) {
//         FxAstAssign* assign = reinterpret_cast<FxAstAssign*>(node);
//         VisitAssignment(assign);
//     }
//     else if (node->NodeType == FX_AST_ACTIONCALL) {
//         FxAstActionCall* actioncall = reinterpret_cast<FxAstActionCall*>(node);
//         FxScriptValue return_value = VisitActionCall(actioncall);

//         // If we are in command mode, print the result to the user
//         if (mInCommandMode && return_value.Type != FxScriptValue::NONETYPE) {
//             return_value.Print();
//         }
//     }
//     // If we are in command mode, print the variable to the user
//     else if (node->NodeType == FX_AST_VARREF && mInCommandMode) {
//         FxAstVarRef* ref = reinterpret_cast<FxAstVarRef*>(node);
//         FxScriptVar* var = FindVar(ref->Name->GetHash());

//         if (var) {
//             var->Print();
//         }
//     }
//     // If we are in command mode, print the literal(probably result) to the user
//     else if (node->NodeType == FX_AST_LITERAL && mInCommandMode) {
//         FxAstLiteral* literal = reinterpret_cast<FxAstLiteral*>(node);
//         GetImmediateValue(literal->Value).Print();
//     }
//     // Executes a statement in command mode
//     else if (node->NodeType == FX_AST_COMMANDMODE) {
//         mInCommandMode = true;
//         Visit(reinterpret_cast<FxAstCommandMode*>(node)->Node);
//         mInCommandMode = false;
//     }
//     else {
//         puts("[UNKNOWN]");
//     }
// }

// void FxScriptInterpreter::DefineExternalVar(const char* type, const char* name, const FxScriptValue& value)
// {
//     Token* name_token = FX_SCRIPT_ALLOC_MEMORY(Token, sizeof(Token));
//     Token* type_token = FX_SCRIPT_ALLOC_MEMORY(Token, sizeof(Token));

//     {
//         const uint32 type_len = strlen(type);
//         char* type_buffer = FX_SCRIPT_ALLOC_MEMORY(char, (type_len + 1));
//         strcpy(type_buffer, type);
//         // type_buffer[type_len + 1] = 0;

//         type_token->Start = type_buffer;
//         type_token->Type = TT::Identifier;
//         type_token->Start[type_len] = 0;
//         type_token->Length = type_len + 1;
//     }

//     {
//         const uint32 name_len = strlen(name);

//         char* name_buffer = FX_SCRIPT_ALLOC_MEMORY(char, (name_len + 1));
//         strcpy(name_buffer, name);

//         name_token->Start = name_buffer;
//         name_token->Type = TT::Identifier;
//         name_token->Start[name_len] = 0;
//         name_token->Length = name_len + 1;
//     }

//     FxScriptScope* definition_scope = &mScopes[0];

//     FxScriptVar var(type_token, name_token, definition_scope, true);
//     var.Value = value;

//     definition_scope->Vars.Insert(var);
// }

// const FxScriptValue& FxScriptInterpreter::GetImmediateValue(const FxScriptValue& value)
// {
//     // If the value is a reference, get the value of that reference
//     if (value.Type == FxScriptValue::REF) {
//         FxScriptVar* var = FindVar(value.ValueRef->Name->GetHash());

//         if (!var) {
//             printf("!!! Undefined reference to variable\n");
//             // value.ValueRef->Name->Print();
//             putchar('\n');

//             return FxScriptValue::None;
//         }

//         return var->Value;
//     }

//     return value;
// }

// // #endif

// void FxConfigScript::RegisterExternalFunc(FxHash32 func_name, std::vector<FxScriptValue::ValueType> param_types,
//                                           FxScriptExternalFunc::FuncType callback, bool is_variadic)
// {
//     FxScriptExternalFunc func {
//         .HashedName = func_name,
//         .Function = callback,
//         .ParameterTypes = param_types,
//         .IsVariadic = is_variadic,
//     };

//     mExternalFuncs.push_back(func);
// }

// /////////////////////////////////////////
// // Script Bytecode Emitter
// /////////////////////////////////////////

// #include "FxScriptBytecode.hpp"

// void FxScriptBCEmitter::BeginEmitting(FxAstNode* node)
// {
//     mStackSize = 1024;

//     /*mStack = new uint8[1024];
//     mStackStart = mStack;*/

//     mBytecode.Create(4096);
//     VarHandles.Create(64);

//     Emit(node);

//     printf("\n");

//     PrintBytecode();
// }

// #define RETURN_IF_NO_NODE(node_)                                                                                       \
//     if ((node_) == nullptr) {                                                                                          \
//         return;                                                                                                        \
//     }

// #define RETURN_VALUE_IF_NO_NODE(node_, value_)                                                                         \
//     if ((node_) == nullptr) {                                                                                          \
//         return (value_);                                                                                               \
//     }

// void FxScriptBCEmitter::Emit(FxAstNode* node)
// {
//     RETURN_IF_NO_NODE(node);

//     if (node->NodeType == FX_AST_BLOCK) {
//         return EmitBlock(reinterpret_cast<FxAstBlock*>(node));
//     }
//     else if (node->NodeType == FX_AST_ACTIONDECL) {
//         return EmitAction(reinterpret_cast<FxAstActionDecl*>(node));
//     }
//     else if (node->NodeType == FX_AST_ACTIONCALL) {
//         return DoActionCall(reinterpret_cast<FxAstActionCall*>(node));
//     }
//     else if (node->NodeType == FX_AST_ASSIGN) {
//         return EmitAssign(reinterpret_cast<FxAstAssign*>(node));
//     }
//     else if (node->NodeType == FX_AST_VARDECL) {
//         DoVarDeclare(reinterpret_cast<FxAstVarDecl*>(node));
//         return;
//     }
// }

// FxScriptBytecodeVarHandle* FxScriptBCEmitter::FindVarHandle(FxHash32 hashed_name)
// {
//     for (FxScriptBytecodeVarHandle& handle : VarHandles) {
//         if (handle.HashedName == hashed_name) {
//             return &handle;
//         }
//     }
//     return nullptr;
// }

// FxScriptBytecodeActionHandle* FxScriptBCEmitter::FindActionHandle(FxHash32 hashed_name)
// {
//     for (FxScriptBytecodeActionHandle& handle : ActionHandles) {
//         if (handle.HashedName == hashed_name) {
//             return &handle;
//         }
//     }
//     return nullptr;
// }

// FxScriptRegister FxScriptBCEmitter::FindFreeRegister()
// {
//     uint16 gp_r = 0x01;

//     const uint16 num_gp_regs = FX_REG_X3;

//     for (int i = 0; i < num_gp_regs; i++) {
//         if (!(mRegsInUse & gp_r)) {
//             // We are starting on 0x01, so register index should be N + 1
//             const int register_index = i + 1;

//             return static_cast<FxScriptRegister>(register_index);
//         }

//         gp_r <<= 1;
//     }

//     return FxScriptRegister::FX_REG_NONE;
// }

// const char* FxScriptBCEmitter::GetRegisterName(FxScriptRegister reg)
// {
//     switch (reg) {
//     case FX_REG_NONE:
//         return "NONE";
//     case FX_REG_X0:
//         return "X0";
//     case FX_REG_X1:
//         return "X1";
//     case FX_REG_X2:
//         return "X2";
//     case FX_REG_X3:
//         return "X3";
//     case FX_REG_RA:
//         return "RA";
//     case FX_REG_XR:
//         return "XR";
//     case FX_REG_SP:
//         return "SP";
//     default:;
//     };

//     return "NONE";
// }

// FxScriptRegister FxScriptBCEmitter::RegFlagToReg(FxScriptRegisterFlag reg_flag)
// {
//     switch (reg_flag) {
//     case FX_REGFLAG_NONE:
//         return FX_REG_NONE;
//     case FX_REGFLAG_X0:
//         return FX_REG_X0;
//     case FX_REGFLAG_X1:
//         return FX_REG_X1;
//     case FX_REGFLAG_X2:
//         return FX_REG_X2;
//     case FX_REGFLAG_X3:
//         return FX_REG_X3;
//     case FX_REGFLAG_RA:
//         return FX_REG_RA;
//     case FX_REGFLAG_XR:
//         return FX_REG_XR;
//     }

//     return FX_REG_NONE;
// }

// FxScriptRegisterFlag FxScriptBCEmitter::RegToRegFlag(FxScriptRegister reg)
// {
//     switch (reg) {
//     case FX_REG_NONE:
//         return FX_REGFLAG_NONE;
//     case FX_REG_X0:
//         return FX_REGFLAG_X0;
//     case FX_REG_X1:
//         return FX_REGFLAG_X1;
//     case FX_REG_X2:
//         return FX_REGFLAG_X2;
//     case FX_REG_X3:
//         return FX_REGFLAG_X3;
//     case FX_REG_RA:
//         return FX_REGFLAG_RA;
//     case FX_REG_XR:
//         return FX_REGFLAG_XR;
//     case FX_REG_SP:
//         return FX_REGFLAG_NONE;
//     default:;
//     }

//     return FX_REGFLAG_NONE;
// }

// void FxScriptBCEmitter::Write16(uint16 value)
// {
//     mBytecode.Insert(static_cast<uint8>(value >> 8));
//     mBytecode.Insert(static_cast<uint8>(value));
// }

// void FxScriptBCEmitter::Write32(uint32 value)
// {
//     Write16(static_cast<uint16>(value >> 16));
//     Write16(static_cast<uint16>(value));
// }

// void FxScriptBCEmitter::WriteOp(uint8 op_base, uint8 op_spec)
// {
//     mBytecode.Insert(op_base);
//     mBytecode.Insert(op_spec);
// }

// using RhsMode = FxScriptBCEmitter::RhsMode;

// #define MARK_REGISTER_USED(regn_)                                                                                      \
//     {                                                                                                                  \
//         MarkRegisterUsed(regn_);                                                                                       \
//     }
// #define MARK_REGISTER_FREE(regn_)                                                                                      \
//     {                                                                                                                  \
//         MarkRegisterFree(regn_);                                                                                       \
//     }

// void FxScriptBCEmitter::MarkRegisterUsed(FxScriptRegister reg)
// {
//     FxScriptRegisterFlag rflag = RegToRegFlag(reg);
//     mRegsInUse = static_cast<FxScriptRegisterFlag>(uint16(mRegsInUse) | uint16(rflag));
// }

// void FxScriptBCEmitter::MarkRegisterFree(FxScriptRegister reg)
// {
//     FxScriptRegisterFlag rflag = RegToRegFlag(reg);

//     mRegsInUse = static_cast<FxScriptRegisterFlag>(uint16(mRegsInUse) & (~uint16(rflag)));
// }

// void FxScriptBCEmitter::EmitSave32(int16 offset, uint32 value)
// {
//     // SAVE32 [i16 offset] [i32]

//     printf("save32 %d, %u\n", static_cast<int16>(offset), value);

//     WriteOp(OpBase_Save, OpSpecSave_Int32);

//     Write16(offset);
//     Write32(value);
// }

// void FxScriptBCEmitter::EmitSaveReg32(int16 offset, FxScriptRegister reg)
// {
//     // SAVE32r [i16 offset] [%r32]

//     printf("save32r %d, %s\n", static_cast<int16>(offset), GetRegisterName(reg));

//     WriteOp(OpBase_Save, OpSpecSave_Reg32);

//     Write16(offset);
//     Write16(reg);
// }

// void FxScriptBCEmitter::EmitSaveAbsolute32(uint32 position, uint32 value)
// {
//     // SAVE32a [i32 offset] [i32]

//     printf("save32a %u, %u\n", position, value);

//     WriteOp(OpBase_Save, OpSpecSave_AbsoluteInt32);

//     Write32(position);
//     Write32(value);
// }

// void FxScriptBCEmitter::EmitSaveAbsoluteReg32(uint32 position, FxScriptRegister reg)
// {
//     // SAVE32r [i32 offset] [%r32]

//     printf("save32ar %u, %s\n", position, GetRegisterName(reg));

//     WriteOp(OpBase_Save, OpSpecSave_AbsoluteReg32);

//     Write32(position);
//     Write16(reg);
// }

// void FxScriptBCEmitter::EmitPush32(uint32 value)
// {
//     // PUSH32 [i32]

//     printf("push32 %d \t# --- offset %lld\n", value, mStackOffset);

//     WriteOp(OpBase_Push, OpSpecPush_Int32);
//     Write32(value);

//     mStackOffset += 4;
// }

// void FxScriptBCEmitter::EmitPush32r(FxScriptRegister reg)
// {
//     // PUSH32r [%r32]
//     printf("push32r %s \t# --- offset %lld\n", GetRegisterName(reg), mStackOffset);

//     WriteOp(OpBase_Push, OpSpecPush_Reg32);
//     Write16(reg);

//     mStackOffset += 4;
// }

// void FxScriptBCEmitter::EmitPop32(FxScriptRegister output_reg)
// {
//     // POP32 [%r32]
//     printf("pop32 %s\n", GetRegisterName(output_reg));

//     WriteOp(OpBase_Pop, (OpSpecPop_Int32 << 4) | (output_reg & 0x0F));

//     mStackOffset -= 4;
// }

// void FxScriptBCEmitter::EmitLoad32(int offset, FxScriptRegister output_reg)
// {
//     // LOAD [i16] [%r32]

//     printf("load32  %d, %s\n", offset, GetRegisterName(output_reg));

//     WriteOp(OpBase_Load, (OpSpecLoad_Int32 << 4) | (output_reg & 0x0F));
//     Write16(static_cast<uint16>(offset));
// }

// void FxScriptBCEmitter::EmitLoadAbsolute32(uint32 position, FxScriptRegister output_reg)
// {
//     // LOADA [i32] [%r32]
//     printf("load32a %u, %s\n", position, GetRegisterName(output_reg));

//     WriteOp(OpBase_Load, (OpSpecLoad_AbsoluteInt32 << 4) | (output_reg & 0x0F));
//     Write32(position);
// }

// void FxScriptBCEmitter::EmitJumpRelative(uint16 offset)
// {
//     printf("jmpr %d\n", offset);

//     WriteOp(OpBase_Jump, OpSpecJump_Relative);
//     Write16(offset);
// }

// void FxScriptBCEmitter::EmitJumpAbsolute(uint32 position)
// {
//     printf("jmpa %d\n", position);

//     WriteOp(OpBase_Jump, OpSpecJump_Absolute);
//     Write32(position);
// }

// void FxScriptBCEmitter::EmitJumpAbsoluteReg32(FxScriptRegister reg)
// {
//     printf("jmpar %s\n", FxScriptBCEmitter::GetRegisterName(reg));

//     WriteOp(OpBase_Jump, OpSpecJump_AbsoluteReg32);
//     Write16(reg);
// }

// void FxScriptBCEmitter::EmitJumpCallAbsolute(uint32 position)
// {
//     printf("calla %u\n", position);

//     WriteOp(OpBase_Jump, OpSpecJump_CallAbsolute);
//     Write32(position);
// }

// void FxScriptBCEmitter::EmitJumpCallExternal(FxHash32 hashed_name)
// {
//     printf("callext %u\n", hashed_name);

//     WriteOp(OpBase_Jump, OpSpecJump_CallExternal);
//     Write32(hashed_name);
// }

// void FxScriptBCEmitter::EmitJumpReturnToCaller()
// {
//     printf("ret\n");

//     WriteOp(OpBase_Jump, OpSpecJump_ReturnToCaller);
// }

// void FxScriptBCEmitter::EmitMoveInt32(FxScriptRegister reg, uint32 value)
// {
//     printf("move32 %s, %u\n", GetRegisterName(reg), value);

//     WriteOp(OpBase_Move, (OpSpecMove_Int32 << 4) | (reg & 0x0F));
//     Write32(value);
// }

// void FxScriptBCEmitter::EmitParamsStart() { WriteOp(OpBase_Data, OpSpecData_ParamsStart); }

// void FxScriptBCEmitter::EmitType(FxScriptValue::ValueType type)
// {
//     OpSpecType op_type = OpSpecType_Int;

//     if (type == FxScriptValue::STRING) {
//         op_type = OpSpecType_String;
//     }

//     WriteOp(OpBase_Type, op_type);
// }

// uint32 FxScriptBCEmitter::EmitDataString(char* str, uint16 length)
// {
//     WriteOp(OpBase_Data, OpSpecData_String);

//     uint32 start_index = mBytecode.Size();

//     uint16 final_length = length + 1;

//     // If the length is not a factor of 2 (sizeof uint16) then add a byte of
//     // padding
//     if ((final_length & 0x01)) {
//         ++final_length;
//     }

//     Write16(final_length);

//     for (int i = 0; i < final_length; i += 2) {
//         mBytecode.Insert(str[i]);

//         if (i >= length) {
//             mBytecode.Insert(0);
//             break;
//         }

//         mBytecode.Insert(str[i + 1]);
//     }

//     return start_index;
// }

// FxScriptRegister FxScriptBCEmitter::EmitBinop(FxAstBinop* binop, FxScriptBytecodeVarHandle* handle)
// {
//     bool will_preserve_lhs = false;
//     // Load the A and B values into the registers
//     FxScriptRegister a_reg = EmitRhs(binop->Left, RhsMode::RHS_FETCH_TO_REGISTER, handle);

//     // Since there is a chance that this register will be clobbered (by binop,
//     // action call, etc), we will push the value of the register here and return
//     // it after processing the RHS
//     if (binop->Right->NodeType != FX_AST_LITERAL) {
//         will_preserve_lhs = true;
//         EmitPush32r(a_reg);
//     }

//     FxScriptRegister b_reg = EmitRhs(binop->Right, RhsMode::RHS_FETCH_TO_REGISTER, handle);

//     // Retrieve the previous LHS
//     if (will_preserve_lhs) {
//         EmitPop32(a_reg);
//     }

//     if (binop->OpToken->Type == TT::Plus) {
//         printf("add %s, %s\n", GetRegisterName(a_reg), GetRegisterName(b_reg));

//         WriteOp(OpBase_Arith, OpSpecArith_Add);

//         mBytecode.Insert(a_reg);
//         mBytecode.Insert(b_reg);
//     }

//     // We no longer need the lhs or rhs registers, free em
//     MARK_REGISTER_FREE(a_reg);
//     MARK_REGISTER_FREE(b_reg);

//     return FX_REG_XR;
// }

// FxScriptRegister FxScriptBCEmitter::EmitVarFetch(FxAstVarRef* ref, RhsMode mode)
// {
//     FxScriptBytecodeVarHandle* var_handle = FindVarHandle(ref->Name->GetHash());

//     bool force_absolute_load = false;

//     // If the variable is from a previous scope, load it from an absolute address.
//     // local offsets will change depending on where they are called.
//     if (var_handle->ScopeIndex < mScopeIndex) {
//         force_absolute_load = true;
//     }

//     if (!var_handle) {
//         printf("Could not find var handle!");
//         return FX_REG_NONE;
//     }

//     FxScriptRegister reg = FindFreeRegister();

//     MARK_REGISTER_USED(reg);

//     DoLoad(var_handle->Offset, reg, force_absolute_load);

//     if (mode == RhsMode::RHS_FETCH_TO_REGISTER) {
//         return reg;
//     }

//     // If we are just copying the variable to this new variable, we can free the
//     // register after we push to the stack.
//     if (mode == RhsMode::RHS_DEFINE_IN_MEMORY) {
//         if (var_handle->Type == FxScriptValue::STRING) {
//             EmitType(var_handle->Type);
//         }

//         EmitPush32r(reg);
//         MARK_REGISTER_FREE(reg);

//         return FX_REG_NONE;
//     }

//     // This is a reference to a variable, return the register we loaded it into
//     return reg;
// }

// void FxScriptBCEmitter::DoLoad(uint32 stack_offset, FxScriptRegister output_reg, bool force_absolute)
// {
//     if (stack_offset < 0xFFFE && !force_absolute) {
//         // Relative load

//         // Calculate the relative index to the current stack offset
//         int input_offset = -(static_cast<int>(mStackOffset) - static_cast<int>(stack_offset));

//         EmitLoad32(input_offset, output_reg);
//     }
//     else {
//         // Absolute load
//         EmitLoadAbsolute32(stack_offset, output_reg);
//     }
// }

// void FxScriptBCEmitter::DoSaveInt32(uint32 stack_offset, uint32 value, bool force_absolute)
// {
//     if (stack_offset < 0xFFFE && !force_absolute) {
//         // Relative save

//         // Calculate the relative index to the current stack offset
//         int input_offset = -(static_cast<int>(mStackOffset) - static_cast<int>(stack_offset));

//         EmitSave32(input_offset, value);
//     }
//     else {
//         // Absolute save
//         EmitSaveAbsolute32(stack_offset, value);
//     }
// }

// void FxScriptBCEmitter::DoSaveReg32(uint32 stack_offset, FxScriptRegister reg, bool force_absolute)
// {
//     if (stack_offset < 0xFFFE && !force_absolute) {
//         // Relative save

//         // Calculate the relative index to the current stack offset
//         int input_offset = -(static_cast<int>(mStackOffset) - static_cast<int>(stack_offset));

//         EmitSaveReg32(input_offset, reg);
//     }
//     else {
//         // Absolute save
//         EmitSaveAbsoluteReg32(stack_offset, reg);
//     }
// }

// void FxScriptBCEmitter::EmitAssign(FxAstAssign* assign)
// {
//     FxScriptBytecodeVarHandle* var_handle = FindVarHandle(assign->Var->Name->GetHash());
//     if (var_handle == nullptr) {
//         printf("!!! Var '%.*s' does not exist!\n", assign->Var->Name->Length, assign->Var->Name->Start);
//         return;
//     }

//     // bool force_absolute_save = false;

//     // if (var_handle->ScopeIndex < mScopeIndex) {
//     //     force_absolute_save = true;
//     // }

//     // int output_offset = -(static_cast<int>(mStackOffset) -
//     // static_cast<int>(var_handle->Offset));

//     if (!var_handle) {
//         printf("Could not find var handle to assign to!");
//         return;
//     }

//     EmitRhs(assign->Rhs, RhsMode::RHS_ASSIGN_TO_HANDLE, var_handle);
// }

// FxScriptRegister FxScriptBCEmitter::EmitLiteralInt(FxAstLiteral* literal, RhsMode mode,
//                                                    FxScriptBytecodeVarHandle* handle)
// {
//     // If this is on variable definition, push the value to the stack.
//     if (mode == RhsMode::RHS_DEFINE_IN_MEMORY) {
//         EmitPush32(literal->Value.ValueInt);

//         return FX_REG_NONE;
//     }

//     // If this is as a literal, push the value to the stack and pop onto the
//     // target register.
//     else if (mode == RhsMode::RHS_FETCH_TO_REGISTER) {
//         // EmitPush32(literal->Value.ValueInt);

//         FxScriptRegister output_reg = FindFreeRegister();
//         // EmitPop32(output_reg);

//         EmitMoveInt32(output_reg, literal->Value.ValueInt);

//         // Mark the output register as used to store it
//         MARK_REGISTER_USED(output_reg);

//         return output_reg;
//     }

//     else if (mode == RhsMode::RHS_ASSIGN_TO_HANDLE) {
//         const bool force_absolute_save = (handle->ScopeIndex < mScopeIndex);
//         DoSaveInt32(handle->Offset, literal->Value.ValueInt, force_absolute_save);

//         return FX_REG_NONE;
//     }

//     return FX_REG_NONE;
// }

// FxScriptRegister FxScriptBCEmitter::EmitLiteralString(FxAstLiteral* literal, RhsMode mode,
//                                                       FxScriptBytecodeVarHandle* handle)
// {
//     const uint32 string_length = strlen(literal->Value.ValueString);

//     // Emit the length and string data
//     const uint32 string_position = EmitDataString(literal->Value.ValueString, string_length);

//     // local string some_value = "Some String";
//     if (mode == RhsMode::RHS_DEFINE_IN_MEMORY) {
//         // Push the location and mark it as a pointer to a string
//         EmitType(FxScriptValue::STRING);
//         EmitPush32(string_position);

//         return FX_REG_NONE;
//     }

//     // some_function("Some String")
//     else if (mode == RhsMode::RHS_FETCH_TO_REGISTER) {
//         // Push the location for the string and pop it back to a register.
//         EmitType(FxScriptValue::STRING);

//         // Push the string position
//         // EmitPush32(string_position);

//         // Find a register to output to and write the index
//         FxScriptRegister output_reg = FindFreeRegister();
//         // EmitPop32(output_reg);

//         EmitMoveInt32(output_reg, string_position);

//         // Mark the output register as used to store it
//         MARK_REGISTER_USED(output_reg);

//         return output_reg;
//     }

//     // some_previous_value = "Some String";
//     else if (mode == RhsMode::RHS_ASSIGN_TO_HANDLE) {
//         const bool force_absolute_save = (handle->ScopeIndex < mScopeIndex);

//         DoSaveInt32(handle->Offset, string_position, force_absolute_save);
//         handle->Type = FxScriptValue::STRING;

//         return FX_REG_NONE;
//     }

//     return FX_REG_NONE;
// }

// FxScriptRegister FxScriptBCEmitter::EmitRhs(FxAstNode* rhs, FxScriptBCEmitter::RhsMode mode,
//                                             FxScriptBytecodeVarHandle* handle)
// {
//     if (rhs->NodeType == FX_AST_LITERAL) {
//         FxAstLiteral* literal = reinterpret_cast<FxAstLiteral*>(rhs);

//         if (literal->Value.Type == FxScriptValue::INT) {
//             return EmitLiteralInt(literal, mode, handle);
//         }
//         else if (literal->Value.Type == FxScriptValue::STRING) {
//             return EmitLiteralString(literal, mode, handle);
//         }
//         else if (literal->Value.Type == FxScriptValue::REF) {
//             // Reference another value, load from memory into register
//             FxScriptRegister output_register = EmitVarFetch(literal->Value.ValueRef, mode);
//             if (mode == RhsMode::RHS_ASSIGN_TO_HANDLE) {
//                 DoSaveReg32(handle->Offset, output_register);
//             }

//             return output_register;
//         }

//         return FX_REG_NONE;
//     }

//     else if (rhs->NodeType == FX_AST_ACTIONCALL || rhs->NodeType == FX_AST_BINOP) {
//         FxScriptRegister result_register = FX_REG_XR;

//         if (rhs->NodeType == FX_AST_BINOP) {
//             result_register = EmitBinop(reinterpret_cast<FxAstBinop*>(rhs), handle);
//         }

//         else if (rhs->NodeType == FX_AST_ACTIONCALL) {
//             DoActionCall(reinterpret_cast<FxAstActionCall*>(rhs));
//             // Action results are stored in XR
//             result_register = FX_REG_XR;
//         }

//         if (mode == RhsMode::RHS_DEFINE_IN_MEMORY) {
//             uint32 offset = mStackOffset;
//             EmitPush32r(result_register);

//             if (handle) {
//                 handle->Offset = offset;
//             }

//             return FX_REG_NONE;
//         }

//         else if (mode == RhsMode::RHS_FETCH_TO_REGISTER) {
//             // Push the result to a register
//             EmitPush32r(result_register);

//             // Find a register to output to, and pop the value to there.
//             FxScriptRegister output_reg = FindFreeRegister();
//             EmitPop32(output_reg);

//             // Mark the output register as used to store it
//             MARK_REGISTER_USED(output_reg);

//             return output_reg;
//         }
//         else if (mode == RhsMode::RHS_ASSIGN_TO_HANDLE) {
//             const bool force_absolute_save = (handle->ScopeIndex < mScopeIndex);

//             // Save the value back to the variable
//             DoSaveReg32(handle->Offset, result_register, force_absolute_save);

//             return FX_REG_NONE;
//         }
//     }

//     return FX_REG_NONE;
// }

// FxScriptBytecodeVarHandle* FxScriptBCEmitter::DoVarDeclare(FxAstVarDecl* decl, VarDeclareMode mode)
// {
//     RETURN_VALUE_IF_NO_NODE(decl, nullptr);

//     const uint16 size_of_type = static_cast<uint16>(sizeof(int32));

//     const FxHash32 type_int = FxHashStr32("int");
//     const FxHash32 type_string = FxHashStr32("string");

//     FxHash32 decl_hash = decl->Name->GetHash();

//     FxScriptValue::ValueType value_type = FxScriptValue::INT;

//     switch (decl_hash) {
//     case type_int:
//         value_type = FxScriptValue::INT;
//         break;
//     case type_string:
//         printf("USING STRING\n");
//         value_type = FxScriptValue::STRING;
//         break;
//     };

//     FxScriptBytecodeVarHandle handle {
//         .HashedName = decl->Name->GetHash(),
//         .Type = value_type, // Just int for now
//         .Offset = (mStackOffset),
//         .SizeOnStack = size_of_type,
//         .ScopeIndex = mScopeIndex,
//     };

//     VarHandles.Insert(handle);

//     printf("\n# variable %.*s:\n", decl->Name->Length, decl->Name->Start);

//     FxScriptBytecodeVarHandle* inserted_handle = &VarHandles[VarHandles.Size() - 1];

//     if (mode == DECLARE_NO_EMIT) {
//         // Do not emit any values
//         return inserted_handle;
//     }

//     if (decl->Assignment) {
//         FxAstNode* rhs = decl->Assignment->Rhs;

//         EmitRhs(rhs, RhsMode::RHS_DEFINE_IN_MEMORY, inserted_handle);

//         // EmitPush32(0);

//         // EmitRhs(rhs, RhsMode::RHS_ASSIGN_TO_HANDLE, inserted_handle);
//     }
//     else {
//         // There is no assignment, push zero as the value for now and
//         // a later assignment can set it using save32.
//         EmitPush32(0);
//     }

//     return inserted_handle;
// }

// void FxScriptBCEmitter::DoActionCall(FxAstActionCall* call)
// {
//     RETURN_IF_NO_NODE(call);

//     FxScriptBytecodeActionHandle* handle = FindActionHandle(call->HashedName);

//     std::vector<uint32> call_locations;
//     call_locations.reserve(8);

//     // Push all params to stack
//     for (FxAstNode* param : call->Params) {
//         // FxScriptRegister reg =
//         if (param->NodeType == FX_AST_ACTIONCALL) {
//             call_locations.push_back(mStackOffset);
//             EmitRhs(param, RhsMode::RHS_DEFINE_IN_MEMORY, nullptr);
//         }
//         // MARK_REGISTER_FREE(reg);
//     }

//     EmitPush32r(FX_REG_RA);

//     EmitParamsStart();

//     int call_location_index = 0;

//     // Push all params to stack
//     for (FxAstNode* param : call->Params) {
//         if (param->NodeType == FX_AST_ACTIONCALL) {
//             FxScriptRegister temp_register = FindFreeRegister();

//             DoLoad(call_locations[call_location_index], temp_register);
//             call_location_index++;

//             EmitPush32r(temp_register);

//             continue;
//         }

//         EmitRhs(param, RhsMode::RHS_DEFINE_IN_MEMORY, nullptr);
//     }

//     // The handle could not be found, write it as a possible external symbol.
//     if (!handle) {
//         printf("Call name-> %u\n", call->HashedName);

//         // Since popping the parameters are handled internally in the VM,
//         // we need to decrement the stack offset here.
//         for (int i = 0; i < call->Params.size(); i++) {
//             mStackOffset -= 4;
//         }

//         EmitJumpCallExternal(call->HashedName);

//         EmitPop32(FX_REG_RA);
//         return;
//     }

//     EmitJumpCallAbsolute(handle->BytecodeIndex);

//     EmitPop32(FX_REG_RA);
// }

// FxScriptBytecodeVarHandle* FxScriptBCEmitter::DefineAndFetchParam(FxAstNode* param_decl_node)
// {
//     if (param_decl_node->NodeType != FX_AST_VARDECL) {
//         printf("!!! param node type is not vardecl!\n");
//         return nullptr;
//     }

//     // Emit variable without emitting pushes or pops
//     FxScriptBytecodeVarHandle* handle = DoVarDeclare(reinterpret_cast<FxAstVarDecl*>(param_decl_node),
//     DECLARE_NO_EMIT);

//     if (!handle) {
//         printf("!!! could not define and fetch param!\n");
//         return nullptr;
//     }

//     assert(handle->SizeOnStack == 4);

//     mStackOffset += handle->SizeOnStack;

//     return handle;
// }

// FxScriptBytecodeVarHandle* FxScriptBCEmitter::DefineReturnVar(FxAstVarDecl* decl)
// {
//     RETURN_VALUE_IF_NO_NODE(decl, nullptr);

//     return DoVarDeclare(decl);
// }

// void FxScriptBCEmitter::EmitAction(FxAstActionDecl* action)
// {
//     RETURN_IF_NO_NODE(action);

//     ++mScopeIndex;

//     // Store the bytecode offset before the action is emitted

//     const size_t start_of_action = mBytecode.Size();
//     printf("Start of action %zu\n", start_of_action);

//     // Emit the jump instruction, we will update the jump position after emitting
//     // all of the code inside the block
//     EmitJumpRelative(0);

//     // const uint32 initial_stack_offset = mStackOffset;

//     const size_t header_jump_start_index = start_of_action + sizeof(uint16);

//     printf("%.*s:\n", action->Name->Length, action->Name->Start);

//     size_t start_var_handle_count = VarHandles.Size();

//     // Offset for the pushed return address
//     mStackOffset += 4;

//     for (FxAstNode* param_decl_node : action->Params->Statements) {
//         DefineAndFetchParam(param_decl_node);
//     }

//     FxScriptBytecodeVarHandle* return_var = DefineReturnVar(action->ReturnVar);

//     EmitBlock(action->Block);

//     if (return_var) {
//         // const int offset = -(static_cast<int>(mStackOffset) -
//         // static_cast<int>(return_var->Offset)); EmitLoad32(offset, FX_REG_XR);

//         DoLoad(return_var->Offset, FX_REG_XR);
//     }

//     // Return offset back to pre-call
//     mStackOffset -= 4;

//     EmitJumpReturnToCaller();

//     const size_t end_of_action = mBytecode.Size();
//     const uint16 distance_to_action = static_cast<uint16>(end_of_action - (start_of_action)-4);

//     printf("End of action: %zu\n", end_of_action);
//     printf("Distance to action: %d\n", distance_to_action);

//     // Update the jump to the end of the action
//     mBytecode[header_jump_start_index] = static_cast<uint8>(distance_to_action >> 8);
//     mBytecode[header_jump_start_index + 1] = static_cast<uint8>((distance_to_action & 0xFF));

//     FxScriptBytecodeActionHandle action_handle { .HashedName = action->Name->GetHash(),
//                                                  .BytecodeIndex = static_cast<uint32>(start_of_action + 4) };

//     const size_t number_of_scope_var_handles = VarHandles.Size() - start_var_handle_count;
//     printf("Number of var handles to remove: %zu\n", number_of_scope_var_handles);

//     ActionHandles.push_back(action_handle);

//     --mScopeIndex;

//     for (int i = 0; i < number_of_scope_var_handles; i++) {
//         FxScriptBytecodeVarHandle* var = VarHandles.RemoveLast();
//         assert(var->SizeOnStack == 4);
//         mStackOffset -= var->SizeOnStack;
//     }
// }

// void FxScriptBCEmitter::EmitBlock(FxAstBlock* block)
// {
//     RETURN_IF_NO_NODE(block);

//     for (FxAstNode* node : block->Statements) {
//         Emit(node);
//     }
// }

// void FxScriptBCEmitter::PrintBytecode()
// {
//     const size_t size = mBytecode.Size();
//     for (int i = 0; i < 25; i++) {
//         printf("%02d ", i);
//     }
//     printf("\n");
//     for (int i = 0; i < 25; i++) {
//         printf("---");
//     }
//     printf("\n");

//     for (size_t i = 0; i < size; i++) {
//         printf("%02X ", mBytecode[i]);

//         if (i > 0 && ((i + 1) % 25) == 0) {
//             printf("\n");
//         }
//     }
//     printf("\n");
// }

// /////////////////////////////////////
// // Bytecode Printer
// /////////////////////////////////////

// uint16 FxScriptBCPrinter::Read16()
// {
//     uint8 lo = mBytecode[mBytecodeIndex++];
//     uint8 hi = mBytecode[mBytecodeIndex++];

//     return ((static_cast<uint16>(lo) << 8) | hi);
// }

// uint32 FxScriptBCPrinter::Read32()
// {
//     uint16 lo = Read16();
//     uint16 hi = Read16();

//     return ((static_cast<uint32>(lo) << 16) | hi);
// }

// #define BC_PRINT_OP(fmt_, ...) snprintf(s, 128, fmt_, ##__VA_ARGS__)

// void FxScriptBCPrinter::DoLoad(char* s, uint8 op_base, uint8 op_spec_raw)
// {
//     uint8 op_spec = ((op_spec_raw >> 4) & 0x0F);
//     uint8 op_reg = (op_spec_raw & 0x0F);

//     if (op_spec == OpSpecLoad_Int32) {
//         int16 offset = Read16();
//         BC_PRINT_OP("load32 %d, %s", offset,
//         FxScriptBCEmitter::GetRegisterName(static_cast<FxScriptRegister>(op_reg)));
//     }
//     else if (op_spec == OpSpecLoad_AbsoluteInt32) {
//         uint32 offset = Read32();
//         BC_PRINT_OP("load32a %u, %s", offset,
//                     FxScriptBCEmitter::GetRegisterName(static_cast<FxScriptRegister>(op_reg)));
//     }
// }

// void FxScriptBCPrinter::DoPush(char* s, uint8 op_base, uint8 op_spec)
// {
//     if (op_spec == OpSpecPush_Int32) {
//         uint32 value = Read32();
//         BC_PRINT_OP("push32 %u", value);
//     }
//     else if (op_spec == OpSpecPush_Reg32) {
//         uint16 reg = Read16();
//         BC_PRINT_OP("push32r %s", FxScriptBCEmitter::GetRegisterName(static_cast<FxScriptRegister>(reg)));
//     }
// }

// void FxScriptBCPrinter::DoPop(char* s, uint8 op_base, uint8 op_spec_raw)
// {
//     uint8 op_spec = ((op_spec_raw >> 4) & 0x0F);
//     uint8 op_reg = (op_spec_raw & 0x0F);

//     if (op_spec == OpSpecPush_Int32) {
//         BC_PRINT_OP("pop32 %s", FxScriptBCEmitter::GetRegisterName(static_cast<FxScriptRegister>(op_reg)));
//     }
// }

// void FxScriptBCPrinter::DoArith(char* s, uint8 op_base, uint8 op_spec)
// {
//     uint8 a_reg = mBytecode[mBytecodeIndex++];
//     uint8 b_reg = mBytecode[mBytecodeIndex++];

//     if (op_spec == OpSpecArith_Add) {
//         BC_PRINT_OP("add32 %s, %s", FxScriptBCEmitter::GetRegisterName(static_cast<FxScriptRegister>(a_reg)),
//                     FxScriptBCEmitter::GetRegisterName(static_cast<FxScriptRegister>(b_reg)));
//     }
// }

// void FxScriptBCPrinter::DoSave(char* s, uint8 op_base, uint8 op_spec)
// {
//     // Save a imm32 into an offset in the stack
//     if (op_spec == OpSpecSave_Int32) {
//         const int16 offset = Read16();
//         const uint32 value = Read32();

//         BC_PRINT_OP("save32 %d, %u", offset, value);
//     }

//     // Save a register into an offset in the stack
//     else if (op_spec == OpSpecSave_Reg32) {
//         const int16 offset = Read16();
//         uint16 reg = Read16();

//         BC_PRINT_OP("save32r %d, %s", offset,
//         FxScriptBCEmitter::GetRegisterName(static_cast<FxScriptRegister>(reg)));
//     }
//     else if (op_spec == OpSpecSave_AbsoluteInt32) {
//         const uint32 offset = Read32();
//         const uint32 value = Read32();

//         BC_PRINT_OP("save32a %u, %u", offset, value);
//     }
//     else if (op_spec == OpSpecSave_AbsoluteReg32) {
//         const uint32 offset = Read32();
//         uint16 reg = Read16();

//         BC_PRINT_OP("save32ar %u, %s", offset,
//         FxScriptBCEmitter::GetRegisterName(static_cast<FxScriptRegister>(reg)));
//     }
// }

// void FxScriptBCPrinter::DoJump(char* s, uint8 op_base, uint8 op_spec)
// {
//     if (op_spec == OpSpecJump_Relative) {
//         uint16 offset = Read16();
//         printf("# jump relative to (%u)\n", mBytecodeIndex + offset);
//         BC_PRINT_OP("jmpr %d", offset);
//     }
//     else if (op_spec == OpSpecJump_Absolute) {
//         uint32 position = Read32();
//         BC_PRINT_OP("jmpa %u", position);
//     }
//     else if (op_spec == OpSpecJump_AbsoluteReg32) {
//         uint16 reg = Read16();
//         BC_PRINT_OP("jmpar %s", FxScriptBCEmitter::GetRegisterName(static_cast<FxScriptRegister>(reg)));
//     }
//     else if (op_spec == OpSpecJump_CallAbsolute) {
//         uint32 position = Read32();
//         BC_PRINT_OP("calla %u", position);
//     }
//     else if (op_spec == OpSpecJump_ReturnToCaller) {
//         BC_PRINT_OP("ret");
//     }
//     else if (op_spec == OpSpecJump_CallExternal) {
//         uint32 hashed_name = Read32();
//         BC_PRINT_OP("callext %u", hashed_name);
//     }
// }

// void FxScriptBCPrinter::DoData(char* s, uint8 op_base, uint8 op_spec)
// {
//     if (op_spec == OpSpecData_String) {
//         uint16 length = Read16();
//         char* data_str = FX_SCRIPT_ALLOC_MEMORY(char, length);
//         uint16* data_str16 = reinterpret_cast<uint16*>(data_str);

//         uint32 bytecode_end = mBytecodeIndex + length;
//         int data_index = 0;
//         while (mBytecodeIndex < bytecode_end) {
//             uint16 value16 = Read16();
//             data_str16[data_index++] = ((value16 << 8) | (value16 >> 8));
//         }

//         BC_PRINT_OP("datastr %d, %.*s", length, length, data_str);

//         FX_SCRIPT_FREE(char, data_str);
//     }
//     else if (op_spec == OpSpecData_ParamsStart) {
//         BC_PRINT_OP("paramsstart");
//     }
// }

// void FxScriptBCPrinter::DoType(char* s, uint8 op_base, uint8 op_spec)
// {
//     if (op_spec == OpSpecType_Int) {
//         BC_PRINT_OP("typeint");
//     }
//     else if (op_spec == OpSpecType_String) {
//         BC_PRINT_OP("typestr");
//     }
// }

// void FxScriptBCPrinter::DoMove(char* s, uint8 op_base, uint8 op_spec_raw)
// {
//     uint8 op_spec = ((op_spec_raw >> 4) & 0x0F);
//     uint8 op_reg = (op_spec_raw & 0x0F);

//     if (op_spec == OpSpecMove_Int32) {
//         uint32 value = Read32();
//         BC_PRINT_OP("move32 %s, %u\t", FxScriptBCEmitter::GetRegisterName(static_cast<FxScriptRegister>(op_reg)),
//                     value);
//     }
// }

// void FxScriptBCPrinter::Print()
// {
//     while (mBytecodeIndex < mBytecode.Size()) {
//         PrintOp();
//     }
// }

// void FxScriptBCPrinter::PrintOp()
// {
//     uint32 bc_index = mBytecodeIndex;

//     uint16 op_full = Read16();

//     const uint8 op_base = static_cast<uint8>(op_full >> 8);
//     const uint8 op_spec = static_cast<uint8>(op_full & 0xFF);

//     char s[128];

//     switch (op_base) {
//     case OpBase_Push:
//         DoPush(s, op_base, op_spec);
//         break;
//     case OpBase_Pop:
//         DoPop(s, op_base, op_spec);
//         break;
//     case OpBase_Load:
//         DoLoad(s, op_base, op_spec);
//         break;
//     case OpBase_Arith:
//         DoArith(s, op_base, op_spec);
//         break;
//     case OpBase_Jump:
//         DoJump(s, op_base, op_spec);
//         break;
//     case OpBase_Save:
//         DoSave(s, op_base, op_spec);
//         break;
//     case OpBase_Data:
//         DoData(s, op_base, op_spec);
//         break;
//     case OpBase_Type:
//         DoType(s, op_base, op_spec);
//         break;
//     case OpBase_Move:
//         DoMove(s, op_base, op_spec);
//         break;
//     }

//     printf("%-25s", s);

//     printf("\t# Offset: %u\n", bc_index);
// }

// ///////////////////////////////////////////
// // Bytecode VM
// ///////////////////////////////////////////

// void FxScriptVM::PrintRegisters()
// {
//     printf("\n=== Register Dump ===\n\n");
//     printf("X0=%u\tX1=%u\tX2=%u\tX3=%u\n", Registers[FX_REG_X0], Registers[FX_REG_X1], Registers[FX_REG_X2],
//            Registers[FX_REG_X3]);

//     printf("XR=%u\tRA=%u\n", Registers[FX_REG_XR], Registers[FX_REG_RA]);

//     printf("\n=====================\n\n");
// }

// uint16 FxScriptVM::Read16()
// {
//     uint8 lo = mBytecode[mPC++];
//     uint8 hi = mBytecode[mPC++];

//     return ((static_cast<uint16>(lo) << 8) | hi);
// }

// uint32 FxScriptVM::Read32()
// {
//     uint16 lo = Read16();
//     uint16 hi = Read16();

//     return ((static_cast<uint32>(lo) << 16) | hi);
// }

// void FxScriptVM::Push16(uint16 value)
// {
//     uint16* dptr = reinterpret_cast<uint16*>(Stack + Registers[FX_REG_SP]);
//     (*dptr) = value;

//     Registers[FX_REG_SP] += sizeof(uint16);
// }

// void FxScriptVM::Push32(uint32 value)
// {
//     uint32* dptr = reinterpret_cast<uint32*>(Stack + Registers[FX_REG_SP]);
//     (*dptr) = value;

//     Registers[FX_REG_SP] += sizeof(uint32);
// }

// uint32 FxScriptVM::Pop32()
// {
//     if (Registers[FX_REG_SP] == 0) {
//         printf("ERR\n");
//     }

//     Registers[FX_REG_SP] -= sizeof(uint32);

//     uint32 value = *reinterpret_cast<uint32*>(Stack + Registers[FX_REG_SP]);
//     return value;
// }

// FxScriptVMCallFrame& FxScriptVM::PushCallFrame()
// {
//     mIsInCallFrame = true;

//     FxScriptVMCallFrame& start_frame = mCallFrames[mCallFrameIndex++];
//     start_frame.StartStackIndex = Registers[FX_REG_SP];

//     return start_frame;
// }

// void FxScriptVM::PopCallFrame()
// {
//     FxScriptVMCallFrame* frame = GetCurrentCallFrame();
//     if (!frame) {
//         FX_BREAKPOINT;
//     }

//     while (Registers[FX_REG_SP] > frame->StartStackIndex) {
//         Pop32();
//     }

//     --mCallFrameIndex;

//     if (mCallFrameIndex == 0) {
//         mIsInCallFrame = false;
//     }
// }

// FxScriptVMCallFrame* FxScriptVM::GetCurrentCallFrame()
// {
//     if (!mIsInCallFrame || mCallFrameIndex < 1) {
//         return nullptr;
//     }

//     return &mCallFrames[mCallFrameIndex - 1];
// }

// FxScriptExternalFunc* FxScriptVM::FindExternalAction(FxHash32 hashed_name)
// {
//     for (FxScriptExternalFunc& func : mExternalFuncs) {
//         if (func.HashedName == hashed_name) {
//             return &func;
//         }
//     }
//     return nullptr;
// }

// void FxScriptVM::ExecuteOp()
// {
//     uint16 op_full = Read16();

//     const uint8 op_base = static_cast<uint8>(op_full >> 8);
//     const uint8 op_spec = static_cast<uint8>(op_full & 0xFF);

//     switch (op_base) {
//     case OpBase_Push:
//         DoPush(op_base, op_spec);
//         break;
//     case OpBase_Pop:
//         DoPop(op_base, op_spec);
//         break;
//     case OpBase_Load:
//         DoLoad(op_base, op_spec);
//         break;
//     case OpBase_Arith:
//         DoArith(op_base, op_spec);
//         break;
//     case OpBase_Jump:
//         DoJump(op_base, op_spec);
//         break;
//     case OpBase_Save:
//         DoSave(op_base, op_spec);
//         break;
//     case OpBase_Data:
//         DoData(op_base, op_spec);
//         break;
//     case OpBase_Type:
//         DoType(op_base, op_spec);
//         break;
//     case OpBase_Move:
//         DoMove(op_base, op_spec);
//         break;
//     }
// }

// void FxScriptVM::DoLoad(uint8 op_base, uint8 op_spec_raw)
// {
//     uint8 op_spec = ((op_spec_raw >> 4) & 0x0F);
//     uint8 op_reg = (op_spec_raw & 0x0F);

//     if (op_spec == OpSpecLoad_Int32) {
//         int16 offset = Read16();

//         uint8* dataptr = &Stack[Registers[FX_REG_SP] + offset];
//         uint32 data32 = *reinterpret_cast<uint32*>(dataptr);

//         Registers[op_reg] = data32;
//     }
//     else if (op_spec == OpSpecLoad_AbsoluteInt32) {
//         uint32 offset = Read32();

//         uint8* dataptr = &Stack[offset];
//         uint32 data32 = *reinterpret_cast<uint32*>(dataptr);

//         Registers[op_reg] = data32;
//     }
// }

// void FxScriptVM::DoPush(uint8 op_base, uint8 op_spec)
// {
//     if (mIsInParams) {
//         if (mCurrentType != FxScriptValue::NONETYPE) {
//             mPushedTypes.Insert(mCurrentType);
//             mCurrentType = FxScriptValue::NONETYPE;
//         }
//         else {
//             mPushedTypes.Insert(FxScriptValue::INT);
//         }
//     }

//     if (op_spec == OpSpecPush_Int32) {
//         uint32 value = Read32();
//         Push32(value);
//     }
//     else if (op_spec == OpSpecPush_Reg32) {
//         uint16 reg = Read16();
//         Push32(Registers[reg]);
//     }
// }

// void FxScriptVM::DoPop(uint8 op_base, uint8 op_spec_raw)
// {
//     uint8 op_spec = ((op_spec_raw >> 4) & 0x0F);
//     uint8 op_reg = (op_spec_raw & 0x0F);

//     if (op_spec == OpSpecPush_Int32) {
//         uint32 value = Pop32();

//         Registers[op_reg] = value;
//     }

//     if (mIsInParams) {
//         mPushedTypes.RemoveLast();
//     }
// }

// void FxScriptVM::DoArith(uint8 op_base, uint8 op_spec)
// {
//     uint8 a_reg = mBytecode[mPC++];
//     uint8 b_reg = mBytecode[mPC++];

//     if (op_spec == OpSpecArith_Add) {
//         Registers[FX_REG_XR] = Registers[a_reg] + Registers[b_reg];
//     }
// }

// void FxScriptVM::DoSave(uint8 op_base, uint8 op_spec)
// {
//     uint32 offset;

//     if (op_spec == OpSpecSave_AbsoluteInt32 || op_spec == OpSpecSave_AbsoluteReg32) {
//         offset = Read32();
//     }
//     else {
//         // The offset is relative to the stack pointer, get the absolute value
//         const int16 relative_offset = static_cast<int16>(Read16());
//         offset = static_cast<uint32>(Registers[FX_REG_SP] + relative_offset);
//     }

//     uint32* dataptr = reinterpret_cast<uint32*>(&Stack[offset]);

//     // Save a imm32 into an offset in the stack
//     if (op_spec == OpSpecSave_Int32) {
//         (*dataptr) = Read32();
//     }

//     // Save a register into an offset in the stack
//     else if (op_spec == OpSpecSave_Reg32) {
//         uint16 reg = Read16();
//         (*dataptr) = Registers[reg];
//     }

//     else if (op_spec == OpSpecSave_AbsoluteInt32) {
//         (*dataptr) = Read32();
//     }

//     else if (op_spec == OpSpecSave_AbsoluteReg32) {
//         uint16 reg = Read16();
//         (*dataptr) = Registers[reg];
//     }
// }

// void FxScriptVM::DoJump(uint8 op_base, uint8 op_spec)
// {
//     if (op_spec == OpSpecJump_Relative) {
//         uint16 offset = Read16();
//         mPC += offset;
//     }
//     else if (op_spec == OpSpecJump_Absolute) {
//         uint32 position = Read32();
//         mPC = position;
//     }
//     else if (op_spec == OpSpecJump_AbsoluteReg32) {
//         uint16 reg = Read16();
//         mPC = Registers[reg];
//     }
//     else if (op_spec == OpSpecJump_CallAbsolute) {
//         uint32 call_address = Read32();
//         // printf("Call to address % 4u\n", call_address);

//         // Push32(mPC);

//         Registers[FX_REG_RA] = mPC;

//         mPushedTypes.Clear();
//         mIsInParams = false;

//         PushCallFrame();

//         // Jump to the action address
//         mPC = call_address;
//     }
//     else if (op_spec == OpSpecJump_ReturnToCaller) {
//         PopCallFrame();

//         // uint32 return_address = Pop32();

//         uint32 return_address = Registers[FX_REG_RA];
//         // printf("Return to caller (%04d)\n", return_address);
//         mPC = return_address;

//         // Restore the return address register to its previous value. This is pushed
//         // when `paramsstart` is encountered.
//         // Registers[FX_REG_RA] = Pop32();
//     }
//     else if (op_spec == OpSpecJump_CallExternal) {
//         uint32 hashed_name = Read32();

//         FxScriptExternalFunc* external_func = FindExternalAction(hashed_name);

//         if (!external_func) {
//             printf("!!! Could not find external function in VM!\n");
//             return;
//         }

//         std::vector<FxScriptValue> params;
//         params.reserve(external_func->ParameterTypes.size());

//         uint32 num_params = mPushedTypes.Size();

//         printf("Num Params: %u\n", num_params);

//         for (int i = 0; i < num_params; i++) {
//             FxScriptValue::ValueType param_type = mPushedTypes.GetLast();

//             FxScriptValue value;
//             value.Type = param_type;

//             if (param_type == FxScriptValue::INT) {
//                 value.ValueInt = Pop32();
//                 value.Type = param_type;
//             }
//             else if (param_type == FxScriptValue::STRING) {
//                 uint32 string_location = Pop32();
//                 uint8* str_base_ptr = &mBytecode[string_location];
//                 // uint16 str_length = *((uint16*)str_base_ptr);

//                 str_base_ptr += 2;

//                 value.ValueString = reinterpret_cast<char*>(str_base_ptr);
//                 value.Type = param_type;
//             }

//             mPushedTypes.RemoveLast();

//             params.push_back(value);
//         }

//         mPushedTypes.Clear();
//         mIsInParams = false;

//         FxScriptValue return_value {};
//         external_func->Function(this, params, &return_value);
//     }
// }

// void FxScriptVM::DoData(uint8 op_base, uint8 op_spec)
// {
//     if (op_spec == OpSpecData_String) {
//         uint16 length = Read16();
//         mPC += length;
//     }
//     else if (op_spec == OpSpecData_ParamsStart) {
//         mIsInParams = true;

//         // Push the current return address pointer. This is so nested action calls
//         // can correctly navigate back to the caller.
//         // Push32(Registers[FX_REG_RA]);
//     }
// }

// void FxScriptVM::DoType(uint8 op_base, uint8 op_spec)
// {
//     if (op_spec == OpSpecType_Int) {
//         mCurrentType = FxScriptValue::INT;
//     }
//     else if (op_spec == OpSpecType_String) {
//         mCurrentType = FxScriptValue::STRING;
//     }
// }

// void FxScriptVM::DoMove(uint8 op_base, uint8 op_spec_raw)
// {
//     uint8 op_spec = ((op_spec_raw >> 4) & 0x0F);
//     FxScriptRegister op_reg = static_cast<FxScriptRegister>(op_spec_raw & 0x0F);

//     if (op_spec == OpSpecMove_Int32) {
//         int32 value = Read32();
//         Registers[op_reg] = value;
//     }
// }
