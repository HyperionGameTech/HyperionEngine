#include <script/ScriptApi.hpp>
#include <script/ScriptApi2.hpp>

#include <script/compiler/Lexer.hpp>
#include <script/compiler/Parser.hpp>
#include <script/compiler/TokenStream.hpp>
#include <script/compiler/ast/AstAsExpression.hpp>
#include <script/compiler/ast/AstNil.hpp>
#include <script/compiler/ast/AstPrototypeSpecification.hpp>
#include <script/compiler/ast/AstTemplateExpression.hpp>
#include <script/compiler/ast/AstTypeRef.hpp>
#include <script/compiler/ast/AstVariable.hpp>
#include <script/compiler/ast/AstVariableDeclaration.hpp>

#include <script/vm/VM.hpp>

#include <core/lib/ByteBuffer.hpp>

namespace hyperion {
namespace scriptapi2 {

// ClassBuilder

ClassBuilder::ClassBuilder(Context *context, ClassDefinition class_definition)
    : m_context(context), m_class_definition(std::move(class_definition)) {
  AssertThrow(m_context != nullptr);
}

ClassBuilder &ClassBuilder::Member(String name, String type_string,
                                   Value value) {
  m_class_definition.members.PushBack(Symbol{name, type_string, value});

  return *this;
}

ClassBuilder &ClassBuilder::Method(String name, String type_string,
                                   NativeFunctionPtr_t fn) {
  m_class_definition.members.PushBack(Symbol{name, type_string, fn});

  return *this;
}

ClassBuilder &ClassBuilder::StaticMember(String name, String type_string,
                                         Value value) {
  m_class_definition.static_members.PushBack(Symbol{name, type_string, value});

  return *this;
}

ClassBuilder &ClassBuilder::StaticMethod(String name, String type_string,
                                         NativeFunctionPtr_t fn) {
  m_class_definition.static_members.PushBack(Symbol{name, type_string, fn});

  return *this;
}

void ClassBuilder::Build() {
  Mutex::Guard guard(m_context->m_mutex);

  // Add `native_type_id` member to class
  m_class_definition.static_members.PushBack(
      {"native_type_id",
       {"uint"},
       vm::Value{vm::Value::U32,
                 {.u32 = m_class_definition.native_type_id.Value()}}});

  m_context->m_class_definitions.PushBack(std::move(m_class_definition));
}

// Context

Context &Context::Global(String name, String type_string, Value value) {
  Mutex::Guard guard(m_mutex);

  m_globals.PushBack(GlobalDefinition{Symbol{name, type_string, value}});

  return *this;
}

Context &Context::Global(String name, String generic_params_string,
                         String type_string, Value value) {
  Mutex::Guard guard(m_mutex);

  m_globals.PushBack(GlobalDefinition{Symbol{name, type_string, value},
                                      std::move(generic_params_string)});

  return *this;
}

Context &Context::Global(String name, String type_string,
                         NativeFunctionPtr_t fn) {
  Mutex::Guard guard(m_mutex);

  m_globals.PushBack(GlobalDefinition{Symbol{name, type_string, fn}});

  return *this;
}

Context &Context::Global(String name, String generic_params_string,
                         String type_string, NativeFunctionPtr_t fn) {
  Mutex::Guard guard(m_mutex);

  m_globals.PushBack(GlobalDefinition{Symbol{name, type_string, fn},
                                      std::move(generic_params_string)});

  return *this;
}

RC<AstExpression> Context::ParseTypeExpression(const String &type_string) {
  AstIterator ast_iterator;

  SourceFile source_file(SourceLocation::eof.GetFileName(),
                         type_string.Size() + 1);

  ByteBuffer temp(type_string.Size() + 1, type_string.Data());
  source_file.ReadIntoBuffer(temp);

  // use the lexer and parser on this file buffer
  TokenStream token_stream(TokenStreamInfo{SourceLocation::eof.GetFileName()});

  CompilationUnit compilation_unit;

  Lexer lexer(SourceStream(&source_file), &token_stream, &compilation_unit);
  lexer.Analyze();

  Parser parser(&ast_iterator, &token_stream, &compilation_unit);

  RC<AstPrototypeSpecification> type_spec =
      parser.ParsePrototypeSpecification();

  AssertThrowMsg(!compilation_unit.GetErrorList().HasFatalErrors(),
                 "Failed to parse type expression: %s", type_string.Data());

  return type_spec;
}

Array<RC<AstParameter>>
Context::ParseGenericParams(const String &generic_params_string) {
  AstIterator ast_iterator;

  SourceFile source_file(SourceLocation::eof.GetFileName(),
                         generic_params_string.Size() + 1);

  ByteBuffer temp(generic_params_string.Size() + 1,
                  generic_params_string.Data());
  source_file.ReadIntoBuffer(temp);

  // use the lexer and parser on this file buffer
  TokenStream token_stream(TokenStreamInfo{SourceLocation::eof.GetFileName()});

  CompilationUnit compilation_unit;

  Lexer lexer(SourceStream(&source_file), &token_stream, &compilation_unit);
  lexer.Analyze();

  Parser parser(&ast_iterator, &token_stream, &compilation_unit);

  Array<RC<AstParameter>> generic_params = parser.ParseGenericParameters();

  AssertThrowMsg(!compilation_unit.GetErrorList().HasFatalErrors(),
                 "Failed to parse generic parameters: %s",
                 generic_params_string.Data());

  return generic_params;
}

void Context::Visit(AstVisitor *visitor, CompilationUnit *compilation_unit) {
  Mutex::Guard guard(m_mutex);

  for (GlobalDefinition &global : m_globals) {
    IdentifierFlagBits identifier_flags =
        IdentifierFlags::FLAG_CONST | IdentifierFlags::FLAG_NATIVE;

    RC<AstPrototypeSpecification> type_spec =
        ParseTypeExpression(global.symbol.type.type_string)
            .Cast<AstPrototypeSpecification>();
    AssertThrow(type_spec != nullptr);

    RC<AstExpression> expr(new AstAsExpression(
        RC<AstNil>(new AstNil(SourceLocation::eof)),
        RC<AstPrototypeSpecification>(new AstPrototypeSpecification(
            RC<AstTypeRef>(
                new AstTypeRef(BuiltinTypes::ANY, SourceLocation::eof)),
            SourceLocation::eof)),
        SourceLocation::eof));

    if (global.generic_params_string.HasValue()) {
      const Array<RC<AstParameter>> generic_params =
          ParseGenericParams(*global.generic_params_string);

      if (generic_params.Any()) {
        expr.Reset(new AstTemplateExpression(
            expr, generic_params, type_spec,
            AST_TEMPLATE_EXPRESSION_FLAG_NATIVE, SourceLocation::eof));

        identifier_flags |= IdentifierFlags::FLAG_GENERIC;

        type_spec.Reset(); // reset type_spec so we don't double-visit it
      }
    }

    global.var_decl.Reset(
        new AstVariableDeclaration(global.symbol.name, type_spec, expr,
                                   identifier_flags, SourceLocation::eof));

    visitor->GetAstIterator()->Push(global.var_decl);
  }

  for (ClassDefinition &class_definition : m_class_definitions) {
    Array<RC<AstVariableDeclaration>> members;
    members.Resize(class_definition.members.Size());

    Array<RC<AstVariableDeclaration>> static_members;
    static_members.Resize(class_definition.static_members.Size());

    FixedArray<Pair<Array<RC<AstVariableDeclaration>> *, Array<Symbol> *>, 2>
        member_arrays{
            Pair<Array<RC<AstVariableDeclaration>> *, Array<Symbol> *>{
                &members, &class_definition.members},
            Pair<Array<RC<AstVariableDeclaration>> *, Array<Symbol> *>{
                &static_members, &class_definition.static_members}};

    for (Pair<Array<RC<AstVariableDeclaration>> *, Array<Symbol> *>
             &member_array : member_arrays) {
      for (SizeType i = 0; i < member_array.second->Size(); ++i) {
        const Symbol &symbol = (*member_array.second)[i];

        RC<AstPrototypeSpecification> type_spec =
            ParseTypeExpression(symbol.type.type_string)
                .Cast<AstPrototypeSpecification>();
        AssertThrow(type_spec != nullptr);

        (*member_array.first)[i].Reset(new AstVariableDeclaration(
            symbol.name, type_spec,
            RC<AstAsExpression>(new AstAsExpression(
                RC<AstNil>(new AstNil(SourceLocation::eof)),
                RC<AstPrototypeSpecification>(new AstPrototypeSpecification(
                    RC<AstTypeRef>(
                        new AstTypeRef(BuiltinTypes::ANY, SourceLocation::eof)),
                    SourceLocation::eof)),
                SourceLocation::eof)),
            IdentifierFlags::FLAG_NATIVE, SourceLocation::eof));
      }
    }

    class_definition.expr.Reset(
        new AstTypeExpression(class_definition.name, nullptr, members, {},
                              static_members, false, SourceLocation::eof));

    IdentifierFlagBits identifier_flags =
        IdentifierFlags::FLAG_CONST | IdentifierFlags::FLAG_NATIVE;

    if (class_definition.generic_params_string.HasValue()) {
      const Array<RC<AstParameter>> generic_params =
          ParseGenericParams(*class_definition.generic_params_string);

      if (generic_params.Any()) {
        class_definition.expr.Reset(new AstTemplateExpression(
            class_definition.expr, generic_params, nullptr,
            AST_TEMPLATE_EXPRESSION_FLAG_NATIVE, SourceLocation::eof));

        identifier_flags |= IdentifierFlags::FLAG_GENERIC;
      }
    }

    class_definition.var_decl.Reset(new AstVariableDeclaration(
        class_definition.name, nullptr, class_definition.expr, identifier_flags,
        SourceLocation::eof));

    visitor->GetAstIterator()->Push(class_definition.var_decl);
  }
}

void Context::BindAll(APIInstance &api_instance, VM *vm) {
  Mutex::Guard guard(m_mutex);

  for (const GlobalDefinition &global : m_globals) {
    AssertThrow(global.var_decl != nullptr);
    AssertThrow(global.var_decl->GetIdentifier() != nullptr);

    const int stack_location =
        global.var_decl->GetIdentifier()->GetStackLocation();
    AssertThrowMsg(stack_location != -1, "Global %s has no stack location",
                   global.symbol.name.Data());

    Value value{Value::NONE, {}};

    if (global.symbol.value.Is<Value>()) {
      value = global.symbol.value.Get<Value>();
    } else if (global.symbol.value.Is<NativeFunctionPtr_t>()) {
      value = {Value::NATIVE_FUNCTION,
               {.native_func = global.symbol.value.Get<NativeFunctionPtr_t>()}};
    } else {
      AssertThrow(false);
    }

    VMState &vm_state = vm->GetState();

    AssertThrow(vm_state.GetMainThread()->GetStack().STACK_SIZE >
                stack_location);
    vm_state.GetMainThread()->GetStack().GetData()[stack_location] = value;
  }

  for (const ClassDefinition &class_definition : m_class_definitions) {
    AssertThrow(class_definition.expr != nullptr);

    AssertThrow(class_definition.var_decl != nullptr);
    AssertThrow(class_definition.var_decl->GetIdentifier() != nullptr);

    const int stack_location =
        class_definition.var_decl->GetIdentifier()->GetStackLocation();
    AssertThrowMsg(stack_location != -1, "Class %s has no stack location",
                   class_definition.name.Data());

    // Ensure class SymbolType is registered
    SymbolTypePtr_t held_type = class_definition.expr->GetHeldType();
    AssertThrow(held_type != nullptr);
    held_type = held_type->GetUnaliased();

    AssertThrowMsg(held_type->GetId() != -1, "Class %s has no ID",
                   class_definition.name.Data());

    // Load the class object from the VM - it is stored in StaticMemory
    // at the index

    VMState &vm_state = vm->GetState();

    const int index = held_type->GetId();
    AssertThrow(vm_state.m_static_memory.static_size > index);

    Array<Member> class_object_members;
    class_object_members.Resize(held_type->GetMembers().Size());

    for (SizeType i = 0; i < held_type->GetMembers().Size(); ++i) {
      const SymbolTypeMember &member = held_type->GetMembers()[i];

      auto symbol_it = class_definition.static_members.FindIf(
          [&member](const Symbol &symbol) {
            return symbol.name == member.name;
          });

      if (symbol_it == class_definition.static_members.End()) {
        continue;
      }

      const Symbol &symbol = *symbol_it;

      Value symbol_value{Value::NONE, {}};

      if (symbol.value.Is<Value>()) {
        symbol_value = symbol.value.Get<Value>();
      } else if (symbol.value.Is<NativeFunctionPtr_t>()) {
        symbol_value = {
            Value::NATIVE_FUNCTION,
            {.native_func = symbol.value.Get<NativeFunctionPtr_t>()}};
      } else {
        AssertThrow(false);
      }

      Memory::StrCpy(class_object_members[i].name, symbol.name.Data(),
                     MathUtil::Min(symbol.name.Size(), 255));
      class_object_members[i].hash = hash_fnv_1(class_object_members[i].name);
      class_object_members[i].value = symbol_value;
    }

    HeapValue *class_object_heap_value =
        vm_state.HeapAlloc(vm_state.GetMainThread());
    class_object_heap_value->Assign(VMObject(
        class_object_members.Data(), class_object_members.Size(), nullptr));
    class_object_heap_value->Mark();

    VMObject *class_object_ptr =
        class_object_heap_value->GetPointer<VMObject>();
    AssertThrow(class_object_ptr != nullptr);

    Array<Member> proto_object_members;
    proto_object_members.Resize(class_definition.members.Size());

    for (SizeType i = 0; i < class_definition.members.Size(); ++i) {
      const Symbol &symbol = class_definition.members[i];

      Value symbol_value{Value::NONE, {}};

      if (symbol.value.Is<Value>()) {
        symbol_value = symbol.value.Get<Value>();
      } else if (symbol.value.Is<NativeFunctionPtr_t>()) {
        symbol_value = {
            Value::NATIVE_FUNCTION,
            {.native_func = symbol.value.Get<NativeFunctionPtr_t>()}};
      }

      Memory::StrCpy(proto_object_members[i].name, symbol.name.Data(),
                     MathUtil::Min(symbol.name.Size(), 255));
      proto_object_members[i].hash = hash_fnv_1(proto_object_members[i].name);
      proto_object_members[i].value = symbol_value;
    }

    // Add __intern member
    proto_object_members.PushBack(
        Member{"__intern", hash_fnv_1("__intern"), Value{Value::NONE, {}}});

    HeapValue *proto_object_heap_value =
        vm_state.HeapAlloc(vm_state.GetMainThread());
    proto_object_heap_value->Assign(VMObject(proto_object_members.Data(),
                                             proto_object_members.Size(),
                                             class_object_heap_value));
    proto_object_heap_value->Mark();

    VMObject *proto_object_ptr =
        proto_object_heap_value->GetPointer<VMObject>();
    AssertThrow(proto_object_ptr != nullptr);

    // Set $proto for class object
    class_object_ptr->SetMember(
        "$proto", Value{Value::HEAP_POINTER, {.ptr = proto_object_heap_value}});

    api_instance.class_bindings.class_prototypes.Set(class_definition.name,
                                                     proto_object_heap_value);
    api_instance.class_bindings.class_names.Set(class_definition.native_type_id,
                                                class_definition.name);

    Value value{Value::HEAP_POINTER, {.ptr = class_object_heap_value}};

    // Set class object in static memory
    vm_state.m_static_memory[index] = value;

    // Set class object in global scope
    AssertThrow(vm_state.GetMainThread()->GetStack().STACK_SIZE >
                stack_location);
    vm_state.GetMainThread()->GetStack().GetData()[stack_location] = value;

    DebugLog(LogType::Info, "Set class %s at index %d\n",
             class_definition.name.Data(), index);
  }
}

} // namespace scriptapi2
} // namespace hyperion
