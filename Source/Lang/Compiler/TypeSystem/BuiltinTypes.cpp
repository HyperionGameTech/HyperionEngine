#include <Lang/Compiler/TypeSystem/BuiltinTypes.hpp>

#include <Lang/Compiler/Ast/AstNil.hpp>
#include <Lang/Compiler/Ast/AstFunctionExpression.hpp>
#include <Lang/Compiler/Ast/AstString.hpp>
#include <Lang/Compiler/Ast/AstName.hpp>
#include <Lang/Compiler/Ast/AstArrayExpression.hpp>
#include <Lang/Compiler/Ast/AstHashMap.hpp>
#include <Lang/Compiler/Ast/AstInteger.hpp>
#include <Lang/Compiler/Ast/AstUnsignedInteger.hpp>
#include <Lang/Compiler/Ast/AstFloat.hpp>
#include <Lang/Compiler/Ast/AstFalse.hpp>
#include <Lang/Compiler/Ast/AstTypeRef.hpp>
#include <Lang/Compiler/Ast/AstUndefined.hpp>

#include <Lang/Compiler/CompilationUnit.hpp>
#include <Lang/Compiler/IdentifierTable.hpp>

namespace Hyperion {

// Class names to use for overrides for built in types.
// (see: GetNativeClassNameForType() below)
static const String s_stringClassName = "String";
static const String s_mapClassName = "ScriptMap";
static const String s_nameClassName = "Name";
static const String s_classRefClassName = "ClassRef";
static const String s_mathClassName = "Math";

const SymbolType* BuiltinTypes::s_primitiveType = nullptr;
const SymbolType* BuiltinTypes::s_errorType = nullptr;
const SymbolType* BuiltinTypes::s_anyType = nullptr;
const SymbolType* BuiltinTypes::s_classType = nullptr;
const SymbolType* BuiltinTypes::s_placeholderType = nullptr;
const SymbolType* BuiltinTypes::s_voidType = nullptr;
const SymbolType* BuiltinTypes::s_objectType = nullptr;
const SymbolType* BuiltinTypes::s_functionBaseType = nullptr;
const SymbolType* BuiltinTypes::s_arrayBaseType = nullptr;
const SymbolType* BuiltinTypes::s_varArgsBaseType = nullptr;
const SymbolType* BuiltinTypes::s_mapBaseType = nullptr;
const SymbolType* BuiltinTypes::s_int8Type = nullptr;
const SymbolType* BuiltinTypes::s_int16Type = nullptr;
const SymbolType* BuiltinTypes::s_int32Type = nullptr;
const SymbolType* BuiltinTypes::s_int64Type = nullptr;
const SymbolType* BuiltinTypes::s_uint8Type = nullptr;
const SymbolType* BuiltinTypes::s_uint16Type = nullptr;
const SymbolType* BuiltinTypes::s_uint32Type = nullptr;
const SymbolType* BuiltinTypes::s_uint64Type = nullptr;
const SymbolType* BuiltinTypes::s_floatType = nullptr;
const SymbolType* BuiltinTypes::s_doubleType = nullptr;
const SymbolType* BuiltinTypes::s_boolType = nullptr;
const SymbolType* BuiltinTypes::s_stringType = nullptr;
const SymbolType* BuiltinTypes::s_nullType = nullptr;
const SymbolType* BuiltinTypes::s_nameType = nullptr;
const SymbolType* BuiltinTypes::s_varArgsType = nullptr;
const SymbolType* BuiltinTypes::s_functionType = nullptr;
const SymbolType* BuiltinTypes::s_arrayType = nullptr;
const SymbolType* BuiltinTypes::s_mapType = nullptr;
const SymbolType* BuiltinTypes::s_mathType = nullptr;

void BuiltinTypes::Initialize(CompilationUnit* globalCompilationUnit)
{
    Assert(globalCompilationUnit != nullptr);

    s_primitiveType = new SymbolType(
        "<primitive>",
        TYPE_BUILTIN,
        nullptr,
        nullptr,
        {}, {});

    s_errorType = new SymbolType(
        "<error>",
        TYPE_BUILTIN,
        nullptr,
        nullptr,
        {}, {});

    s_anyType = new SymbolType(
        "any",
        TYPE_BUILTIN,
        nullptr,
        nullptr,
        {}, {});

    s_classType = new SymbolType(
        "Class",
        TYPE_BUILTIN,
        nullptr,
        nullptr,
        {}, {});

    s_placeholderType = new SymbolType(
        "<placeholder>",
        TYPE_BUILTIN,
        nullptr,
        nullptr,
        {}, {});

    s_voidType = SymbolType::Primitive(
        "void",
        MakeHandle<AstUndefined>(SourceLocation::Eof()));

    s_objectType = new SymbolType(
        "object",
        TYPE_BUILTIN,
        nullptr,
        nullptr,
        {}, {});

    s_functionBaseType = SymbolType::Primitive(
        "FunctionBase",
        nullptr);

    s_arrayBaseType = SymbolType::Primitive(
        "ArrayBase",
        nullptr);

    s_varArgsBaseType = SymbolType::Extend(
        "VarArgsBase",
        BuiltinTypes::s_arrayBaseType,
        {}, {});

    s_mapBaseType = SymbolType::Primitive(
        "MapBase",
        nullptr);

    s_int8Type = SymbolType::Primitive(
        "int8",
        MakeHandle<AstInteger>(0, CBS_8, SourceLocation::Eof()),
        CBS_8);

    s_int16Type = SymbolType::Primitive(
        "int16",
        MakeHandle<AstInteger>(0, CBS_16, SourceLocation::Eof()),
        CBS_16);

    s_int32Type = SymbolType::Primitive(
        "int32",
        MakeHandle<AstInteger>(0, CBS_32, SourceLocation::Eof()),
        CBS_32);

    s_int64Type = SymbolType::Primitive(
        "int64",
        MakeHandle<AstInteger>(0, CBS_64, SourceLocation::Eof()),
        CBS_64);

    s_uint8Type = SymbolType::Primitive(
        "uint8",
        MakeHandle<AstUnsignedInteger>(0, CBS_8, SourceLocation::Eof()),
        CBS_8);

    s_uint16Type = SymbolType::Primitive(
        "uint16",
        MakeHandle<AstUnsignedInteger>(0, CBS_16, SourceLocation::Eof()),
        CBS_16);

    s_uint32Type = SymbolType::Primitive(
        "uint32",
        MakeHandle<AstUnsignedInteger>(0, CBS_32, SourceLocation::Eof()),
        CBS_32);

    s_uint64Type = SymbolType::Primitive(
        "uint64",
        MakeHandle<AstUnsignedInteger>(0, CBS_64, SourceLocation::Eof()),
        CBS_64);

    s_floatType = SymbolType::Primitive(
        "float",
        MakeHandle<AstFloat>(0.0, CBS_32, SourceLocation::Eof()),
        CBS_32);

    s_doubleType = SymbolType::Primitive(
        "double",
        MakeHandle<AstFloat>(0.0, CBS_64, SourceLocation::Eof()),
        CBS_64);

    s_boolType = SymbolType::Primitive(
        "bool",
        MakeHandle<AstFalse>(SourceLocation::Eof()));

    s_stringType = SymbolType::Primitive(
        "string",
        MakeHandle<AstString>("", SourceLocation::Eof()));

    s_nullType = SymbolType::Primitive(
        "<null>",
        MakeHandle<AstNil>(SourceLocation::Eof()));

    s_nameType = SymbolType::Primitive(
        "Name",
        MakeHandle<AstName>("", SourceLocation::Eof()));

    s_varArgsType = SymbolType::Generic(
        "VarArgs",
        s_varArgsBaseType,
        Array<SymbolTypeMember> {},
        Array<SymbolTypeMember> {},
        GenericInstanceTypeInfo {
            {
                { "type", SymbolType::GenericParameter("T") }
            }
        });

    s_functionType = SymbolType::Generic(
        "Function",
        BuiltinTypes::s_functionBaseType,
        Array<SymbolTypeMember> {},
        Array<SymbolTypeMember> {},
        GenericInstanceTypeInfo {
            {
                { "@return", SymbolType::GenericParameter("ReturnType") },
                { "@args", SymbolType::GenericInstance(BuiltinTypes::s_varArgsType, {}, {}, GenericInstanceTypeInfo {}) }
            }
        });

    // See ScriptArrayWrapper.cpp in Hyperion Engine for implementation of array methods
    s_arrayType = SymbolType::Generic(
        "Array",
        BuiltinTypes::s_arrayBaseType,
        Array<SymbolTypeMember> {
            SymbolTypeMember {
                "operator[]",
                SymbolType::GenericInstance(
                    BuiltinTypes::s_functionType,
                    {}, {},
                    GenericInstanceTypeInfo {
                        {
                            { "@return", SymbolType::GenericParameter("T") },
                            { "self", SymbolType::Placeholder("SelfType") },
                            { "index", BuiltinTypes::s_uint64Type }
                        }
                    })
                },
            SymbolTypeMember {
                "operator[]=",
                SymbolType::GenericInstance(
                    BuiltinTypes::s_functionType,
                    {}, {},
                    GenericInstanceTypeInfo {
                        {
                            { "@return", SymbolType::GenericParameter("T") },
                            { "self", SymbolType::Placeholder("SelfType") },
                            { "index", BuiltinTypes::s_uint64Type },
                            { "value", SymbolType::GenericParameter("T") }
                        }
                    })
                },
            SymbolTypeMember {
                "PushBack",
                SymbolType::GenericInstance(
                    BuiltinTypes::s_functionType,
                    {}, {},
                    GenericInstanceTypeInfo {
                        {
                            { "@return", SymbolType::GenericParameter("T") },
                            { "self", SymbolType::Placeholder("SelfType") },
                            { "value", SymbolType::GenericParameter("T") }
                        }
                    })
                },
            // There is an issue showing up when using this method "Array<string> cannot be passed as Array<any>"
            SymbolTypeMember {
                "PopBack",
                SymbolType::GenericInstance(
                    BuiltinTypes::s_functionType,
                    {}, {},
                    GenericInstanceTypeInfo {
                        {
                            { "@return", SymbolType::GenericParameter("T") },
                            { "self", SymbolType::Placeholder("SelfType") }
                        }
                    })
                },
            SymbolTypeMember {
                "PushFront",
                SymbolType::GenericInstance(
                    BuiltinTypes::s_functionType,
                    {}, {},
                    GenericInstanceTypeInfo {
                        {
                            { "@return", SymbolType::GenericParameter("T") },
                            { "self", SymbolType::Placeholder("SelfType") },
                            { "value", SymbolType::GenericParameter("T") }
                        }
                    })
                },
            SymbolTypeMember {
                "PopFront",
                SymbolType::GenericInstance(
                    BuiltinTypes::s_functionType,
                    {}, {},
                    GenericInstanceTypeInfo {
                        {
                            { "@return", SymbolType::GenericParameter("T") },
                            { "self", SymbolType::Placeholder("SelfType") }
                        }
                    })
                },
            SymbolTypeMember {
                "Add",
                SymbolType::GenericInstance(
                    BuiltinTypes::s_functionType,
                    {}, {},
                    GenericInstanceTypeInfo {
                        {
                            { "@return", SymbolType::GenericParameter("T") },
                            { "self", SymbolType::Placeholder("SelfType") },
                            { "value", SymbolType::GenericParameter("T") }
                        }
                    })
                },
            SymbolTypeMember {
                "Remove",
                SymbolType::GenericInstance(
                    BuiltinTypes::s_functionType,
                    {}, {},
                    GenericInstanceTypeInfo {
                        {
                            { "@return", BuiltinTypes::s_boolType },
                            { "self", SymbolType::Placeholder("SelfType") },
                            { "value", SymbolType::GenericParameter("T") }
                        }
                    })
                },
            SymbolTypeMember {
                "Clear",
                SymbolType::GenericInstance(
                    BuiltinTypes::s_functionType,
                    {}, {},
                    GenericInstanceTypeInfo {
                        {
                            { "@return", BuiltinTypes::s_voidType },
                            { "self", SymbolType::Placeholder("SelfType") }
                        }
                    })
                },
            SymbolTypeMember {
                "Resize",
                SymbolType::GenericInstance(
                    BuiltinTypes::s_functionType,
                    {}, {},
                    GenericInstanceTypeInfo {
                        {
                            { "@return", BuiltinTypes::s_voidType },
                            { "self", SymbolType::Placeholder("SelfType") },
                            { "newSize", BuiltinTypes::s_uint64Type }
                        }
                    })
                },
            SymbolTypeMember {
                "Reserve",
                SymbolType::GenericInstance(
                    BuiltinTypes::s_functionType,
                    {}, {},
                    GenericInstanceTypeInfo {
                        {
                            { "@return", BuiltinTypes::s_voidType },
                            { "self", SymbolType::Placeholder("SelfType") },
                            { "capacity", BuiltinTypes::s_uint64Type }
                        }
                    })
                },
            SymbolTypeMember {
                "Empty",
                SymbolType::GenericInstance(
                    BuiltinTypes::s_functionType,
                    {}, {},
                    GenericInstanceTypeInfo {
                        {
                            { "@return", BuiltinTypes::s_boolType },
                            { "self", SymbolType::Placeholder("SelfType") }
                        }
                    })
                },
            SymbolTypeMember {
                "Any",
                SymbolType::GenericInstance(
                    BuiltinTypes::s_functionType,
                    {}, {},
                    GenericInstanceTypeInfo {
                        {
                            { "@return", BuiltinTypes::s_boolType },
                            { "self", SymbolType::Placeholder("SelfType") }
                        }
                    })
                },
            SymbolTypeMember {
                "Front",
                SymbolType::GenericInstance(
                    BuiltinTypes::s_functionType,
                    {}, {},
                    GenericInstanceTypeInfo {
                        {
                            { "@return", SymbolType::GenericParameter("T") },
                            { "self", SymbolType::Placeholder("SelfType") }
                        }
                    })
                },
            SymbolTypeMember {
                "Back",
                SymbolType::GenericInstance(
                    BuiltinTypes::s_functionType,
                    {}, {},
                    GenericInstanceTypeInfo {
                        {
                            { "@return", SymbolType::GenericParameter("T") },
                            { "self", SymbolType::Placeholder("SelfType") }
                        }
                    })
                },
            SymbolTypeMember {
                "Size",
                SymbolType::GenericInstance(
                    BuiltinTypes::s_functionType,
                    {}, {},
                    GenericInstanceTypeInfo {
                        {
                            { "@return", BuiltinTypes::s_uint64Type },
                            { "self", SymbolType::Placeholder("SelfType") }
                        }
                    })
                }
            },
        Array<SymbolTypeMember> {},
        GenericInstanceTypeInfo { { { "type", SymbolType::GenericParameter("T") } } });

    s_mapType = SymbolType::Generic(
        "Map",
        BuiltinTypes::s_mapBaseType,
        Array<SymbolTypeMember> {
            SymbolTypeMember {
                "operator[]",
                SymbolType::GenericInstance(
                    BuiltinTypes::s_functionType,
                    {}, {},
                    GenericInstanceTypeInfo {
                        {
                            { "@return", SymbolType::GenericParameter("V") },
                            { "self", /*SymbolType::Placeholder("SelfType")*/ BuiltinTypes::s_anyType },
                            { "key", SymbolType::GenericParameter("K") }
                        }
                    })
                },
            SymbolTypeMember {
                "operator[]=",
                SymbolType::GenericInstance(
                    BuiltinTypes::s_functionType,
                    {}, {},
                    GenericInstanceTypeInfo {
                        {
                            { "@return", SymbolType::GenericParameter("V") },
                            { "self", /*SymbolType::Placeholder("SelfType")*/ BuiltinTypes::s_anyType },
                            { "key", SymbolType::GenericParameter("K") },
                            { "value", SymbolType::GenericParameter("V") }
                        }
                    })
                }
            },
        Array<SymbolTypeMember> {},
        GenericInstanceTypeInfo {
            {
                { "key", SymbolType::GenericParameter("K") },
                { "value", SymbolType::GenericParameter("V") }
            }
        });

    s_mathType = new SymbolType(
        "Math",
        TYPE_BUILTIN,
        nullptr,
        nullptr,
        {}, {});

#pragma region Varargs
    SymbolType* varargsTypeNonConst = const_cast<SymbolType*>(BuiltinTypes::s_varArgsType);

    // VarArgs has Size() type because it is really an array
    varargsTypeNonConst->GetMembers().PushBack(SymbolTypeMember {
        "Size",
        SymbolType::GenericInstance(
            BuiltinTypes::s_functionType,
            {}, {},
            GenericInstanceTypeInfo {
                {
                    { "@return", BuiltinTypes::s_uint64Type },
                    { "self", SymbolType::Placeholder("SelfType") }
                }
            })
        });

#pragma endregion Varargs

#pragma region String
    // HAX - we need to cast away const-ness here because we want to add members to the string type
    SymbolType* stringTypeNonConst = const_cast<SymbolType*>(BuiltinTypes::s_stringType);

    stringTypeNonConst->GetMembers().PushBack(SymbolTypeMember {
        "operator+",
        SymbolType::GenericInstance(
            BuiltinTypes::s_functionType,
            {}, {},
            GenericInstanceTypeInfo {
                {
                    { "@return", BuiltinTypes::s_stringType },
                    { "self", BuiltinTypes::s_stringType },
                    { "other", BuiltinTypes::s_stringType }
                }
            })
        });

    stringTypeNonConst->GetMembers().PushBack(SymbolTypeMember {
        "Length",
        SymbolType::GenericInstance(
            BuiltinTypes::s_functionType,
            {}, {},
            GenericInstanceTypeInfo {
                {
                    { "@return", BuiltinTypes::s_uint64Type },
                    { "self", BuiltinTypes::s_stringType }
                }
            })
        });

    stringTypeNonConst->GetStaticMembers().PushBack(SymbolTypeMember {
        "Join",
        SymbolType::GenericInstance(
            BuiltinTypes::s_functionType,
            {}, {},
            GenericInstanceTypeInfo {
                {
                    { "@return", BuiltinTypes::s_stringType },
                    { "elems", SymbolType::GenericInstance(BuiltinTypes::s_arrayType, {}, {}, GenericInstanceTypeInfo { { { "type", BuiltinTypes::s_anyType } } }) },
                    { "sep", BuiltinTypes::s_stringType }
                }
            })
        });

    stringTypeNonConst->GetStaticMembers().PushBack(SymbolTypeMember {
        "$invoke",
        SymbolType::GenericInstance(
            BuiltinTypes::s_functionType,
            {}, {},
            GenericInstanceTypeInfo {
                {
                    { "@return", BuiltinTypes::s_stringType },
                    { "val", BuiltinTypes::s_anyType }
                }
            })
        });
#pragma endregion String

#pragma region Name
    // HAX - we need to cast away const-ness here because we want to add members to the name type
    SymbolType* nameTypeNonConst = const_cast<SymbolType*>(BuiltinTypes::s_nameType);

    nameTypeNonConst->GetMembers().PushBack(SymbolTypeMember {
        "ToString",
        SymbolType::GenericInstance(
            BuiltinTypes::s_functionType,
            {}, {},
            GenericInstanceTypeInfo {
                {
                    { "@return", BuiltinTypes::s_stringType },
                    { "self", BuiltinTypes::s_nameType }
                }
            })
        });

    nameTypeNonConst->GetStaticMembers().PushBack(SymbolTypeMember {
        "FromString",
        SymbolType::GenericInstance(
            BuiltinTypes::s_functionType,
            {}, {},
            GenericInstanceTypeInfo {
                {
                    { "@return", BuiltinTypes::s_nameType },
                    { "val", BuiltinTypes::s_stringType }
                }
            })
        });
#pragma endregion Name

#pragma region Class
    SymbolType* classTypeNonConst = const_cast<SymbolType*>(BuiltinTypes::s_classType);

    classTypeNonConst->GetMembers().PushBack(SymbolTypeMember {
        "IsValid",
        SymbolType::GenericInstance(
            BuiltinTypes::s_functionType,
            {}, {},
            GenericInstanceTypeInfo {
                {
                    { "@return", BuiltinTypes::s_boolType },
                    { "self", BuiltinTypes::s_classType }
                }
            })
        });

    classTypeNonConst->GetMembers().PushBack(SymbolTypeMember {
        "GetName",
        SymbolType::GenericInstance(
            BuiltinTypes::s_functionType,
            {}, {},
            GenericInstanceTypeInfo {
                {
                    { "@return", BuiltinTypes::s_nameType },
                    { "self", BuiltinTypes::s_classType }
                }
            })
        });

    classTypeNonConst->GetMembers().PushBack(SymbolTypeMember {
        "GetSize",
        SymbolType::GenericInstance(
            BuiltinTypes::s_functionType,
            {}, {},
            GenericInstanceTypeInfo {
                {
                    { "@return", BuiltinTypes::s_uint64Type },
                    { "self", BuiltinTypes::s_classType }
                }
            })
        });

    classTypeNonConst->GetMembers().PushBack(SymbolTypeMember {
        "GetAlignment",
        SymbolType::GenericInstance(
            BuiltinTypes::s_functionType,
            {}, {},
            GenericInstanceTypeInfo {
                {
                    { "@return", BuiltinTypes::s_uint64Type },
                    { "self", BuiltinTypes::s_classType }
                }
            })
        });

    classTypeNonConst->GetMembers().PushBack(SymbolTypeMember {
        "GetParent",
        SymbolType::GenericInstance(
            BuiltinTypes::s_functionType,
            {}, {},
            GenericInstanceTypeInfo {
                {
                    { "@return", BuiltinTypes::s_classType },
                    { "self", BuiltinTypes::s_classType }
                }
            })
        });

    classTypeNonConst->GetMembers().PushBack(SymbolTypeMember {
        "IsClassType",
        SymbolType::GenericInstance(
            BuiltinTypes::s_functionType,
            {}, {},
            GenericInstanceTypeInfo {
                {
                    { "@return", BuiltinTypes::s_boolType },
                    { "self", BuiltinTypes::s_classType }
                }
            })
        });

    classTypeNonConst->GetMembers().PushBack(SymbolTypeMember {
        "IsStructType",
        SymbolType::GenericInstance(
            BuiltinTypes::s_functionType,
            {}, {},
            GenericInstanceTypeInfo {
                {
                    { "@return", BuiltinTypes::s_boolType },
                    { "self", BuiltinTypes::s_classType }
                }
            })
        });

    classTypeNonConst->GetMembers().PushBack(SymbolTypeMember {
        "IsEnumType",
        SymbolType::GenericInstance(
            BuiltinTypes::s_functionType,
            {}, {},
            GenericInstanceTypeInfo {
                {
                    { "@return", BuiltinTypes::s_boolType },
                    { "self", BuiltinTypes::s_classType }
                }
            })
        });

    classTypeNonConst->GetMembers().PushBack(SymbolTypeMember {
        "IsPodType",
        SymbolType::GenericInstance(
            BuiltinTypes::s_functionType,
            {}, {},
            GenericInstanceTypeInfo {
                {
                    { "@return", BuiltinTypes::s_boolType },
                    { "self", BuiltinTypes::s_classType }
                }
            })
        });

    classTypeNonConst->GetMembers().PushBack(SymbolTypeMember {
        "IsAbstract",
        SymbolType::GenericInstance(
            BuiltinTypes::s_functionType,
            {}, {},
            GenericInstanceTypeInfo {
                {
                    { "@return", BuiltinTypes::s_boolType },
                    { "self", BuiltinTypes::s_classType }
                }
            })
        });

    classTypeNonConst->GetMembers().PushBack(SymbolTypeMember {
        "IsDynamic",
        SymbolType::GenericInstance(
            BuiltinTypes::s_functionType,
            {}, {},
            GenericInstanceTypeInfo {
                {
                    { "@return", BuiltinTypes::s_boolType },
                    { "self", BuiltinTypes::s_classType }
                }
            })
        });

    classTypeNonConst->GetMembers().PushBack(SymbolTypeMember {
        "IsDerivedFrom",
        SymbolType::GenericInstance(
            BuiltinTypes::s_functionType,
            {}, {},
            GenericInstanceTypeInfo {
                {
                    { "@return", BuiltinTypes::s_boolType },
                    { "self", BuiltinTypes::s_classType },
                    { "other", BuiltinTypes::s_classType }
                }
            })
        });

    classTypeNonConst->GetMembers().PushBack(SymbolTypeMember {
        "IsBaseOf",
        SymbolType::GenericInstance(
            BuiltinTypes::s_functionType,
            {}, {},
            GenericInstanceTypeInfo {
                {
                    { "@return", BuiltinTypes::s_boolType },
                    { "self", BuiltinTypes::s_classType },
                    { "other", BuiltinTypes::s_classType }
                }
            })
        });

    classTypeNonConst->GetMembers().PushBack(SymbolTypeMember {
        "CanCreateInstance",
        SymbolType::GenericInstance(
            BuiltinTypes::s_functionType,
            {}, {},
            GenericInstanceTypeInfo {
                {
                    { "@return", BuiltinTypes::s_boolType },
                    { "self", BuiltinTypes::s_classType }
                }
            })
        });

    classTypeNonConst->GetMembers().PushBack(SymbolTypeMember {
        "CreateInstance",
        SymbolType::GenericInstance(
            BuiltinTypes::s_functionType,
            {}, {},
            GenericInstanceTypeInfo {
                {
                    { "@return", BuiltinTypes::s_anyType },
                    { "self", BuiltinTypes::s_classType }
                }
            })
        });
#pragma endregion Class

#pragma region Math
    SymbolType* mathTypeNonConst = const_cast<SymbolType*>(BuiltinTypes::s_mathType);

    mathTypeNonConst->GetStaticMembers().PushBack(SymbolTypeMember {
        "Sin",
        SymbolType::GenericInstance(
            BuiltinTypes::s_functionType,
            {}, {},
            GenericInstanceTypeInfo {
                {
                    { "@return", BuiltinTypes::s_floatType },
                    { "x", BuiltinTypes::s_floatType }
                }
            })
        });

    mathTypeNonConst->GetStaticMembers().PushBack(SymbolTypeMember {
        "Cos",
        SymbolType::GenericInstance(
            BuiltinTypes::s_functionType,
            {}, {},
            GenericInstanceTypeInfo {
                {
                    { "@return", BuiltinTypes::s_floatType },
                    { "x", BuiltinTypes::s_floatType }
                }
            })
        });

    mathTypeNonConst->GetStaticMembers().PushBack(SymbolTypeMember {
        "Tan",
        SymbolType::GenericInstance(
            BuiltinTypes::s_functionType,
            {}, {},
            GenericInstanceTypeInfo {
                {
                    { "@return", BuiltinTypes::s_floatType },
                    { "x", BuiltinTypes::s_floatType }
                }
            })
        });

    mathTypeNonConst->GetStaticMembers().PushBack(SymbolTypeMember {
        "Arcsin",
        SymbolType::GenericInstance(
            BuiltinTypes::s_functionType,
            {}, {},
            GenericInstanceTypeInfo {
                {
                    { "@return", BuiltinTypes::s_floatType },
                    { "x", BuiltinTypes::s_floatType }
                }
            })
        });

    mathTypeNonConst->GetStaticMembers().PushBack(SymbolTypeMember {
        "Arccos",
        SymbolType::GenericInstance(
            BuiltinTypes::s_functionType,
            {}, {},
            GenericInstanceTypeInfo {
                {
                    { "@return", BuiltinTypes::s_floatType },
                    { "x", BuiltinTypes::s_floatType }
                }
            })
        });

    mathTypeNonConst->GetStaticMembers().PushBack(SymbolTypeMember {
        "Arctan",
        SymbolType::GenericInstance(
            BuiltinTypes::s_functionType,
            {}, {},
            GenericInstanceTypeInfo {
                {
                    { "@return", BuiltinTypes::s_floatType },
                    { "x", BuiltinTypes::s_floatType }
                }
            })
        });

    mathTypeNonConst->GetStaticMembers().PushBack(SymbolTypeMember {
        "RadToDeg",
        SymbolType::GenericInstance(
            BuiltinTypes::s_functionType,
            {}, {},
            GenericInstanceTypeInfo {
                {
                    { "@return", BuiltinTypes::s_floatType },
                    { "rad", BuiltinTypes::s_floatType }
                }
            })
        });

    mathTypeNonConst->GetStaticMembers().PushBack(SymbolTypeMember {
        "DegToRad",
        SymbolType::GenericInstance(
            BuiltinTypes::s_functionType,
            {}, {},
            GenericInstanceTypeInfo {
                {
                    { "@return", BuiltinTypes::s_floatType },
                    { "deg", BuiltinTypes::s_floatType }
                }
            })
        });

    mathTypeNonConst->GetStaticMembers().PushBack(SymbolTypeMember {
        "Sqrt",
        SymbolType::GenericInstance(
            BuiltinTypes::s_functionType,
            {}, {},
            GenericInstanceTypeInfo {
                {
                    { "@return", BuiltinTypes::s_floatType },
                    { "value", BuiltinTypes::s_floatType }
                }
            })
        });

    mathTypeNonConst->GetStaticMembers().PushBack(SymbolTypeMember {
        "Pow",
        SymbolType::GenericInstance(
            BuiltinTypes::s_functionType,
            {}, {},
            GenericInstanceTypeInfo {
                {
                    { "@return", BuiltinTypes::s_floatType },
                    { "value", BuiltinTypes::s_floatType },
                    { "exponent", BuiltinTypes::s_floatType }
                }
            })
        });

    mathTypeNonConst->GetStaticMembers().PushBack(SymbolTypeMember {
        "Abs",
        SymbolType::GenericInstance(
            BuiltinTypes::s_functionType,
            {}, {},
            GenericInstanceTypeInfo {
                {
                    { "@return", BuiltinTypes::s_floatType },
                    { "value", BuiltinTypes::s_floatType }
                }
            })
        });

    mathTypeNonConst->GetStaticMembers().PushBack(SymbolTypeMember {
        "Min",
        SymbolType::GenericInstance(
            BuiltinTypes::s_functionType,
            {}, {},
            GenericInstanceTypeInfo {
                {
                    { "@return", BuiltinTypes::s_floatType },
                    { "a", BuiltinTypes::s_floatType },
                    { "b", BuiltinTypes::s_floatType }
                }
            })
        });

    mathTypeNonConst->GetStaticMembers().PushBack(SymbolTypeMember {
        "Max",
        SymbolType::GenericInstance(
            BuiltinTypes::s_functionType,
            {}, {},
            GenericInstanceTypeInfo {
                {
                    { "@return", BuiltinTypes::s_floatType },
                    { "a", BuiltinTypes::s_floatType },
                    { "b", BuiltinTypes::s_floatType }
                }
            })
        });

    mathTypeNonConst->GetStaticMembers().PushBack(SymbolTypeMember {
        "Clamp",
        SymbolType::GenericInstance(
            BuiltinTypes::s_functionType,
            {}, {},
            GenericInstanceTypeInfo {
                {
                    { "@return", BuiltinTypes::s_floatType },
                    { "val", BuiltinTypes::s_floatType },
                    { "min", BuiltinTypes::s_floatType },
                    { "max", BuiltinTypes::s_floatType }
                }
            })
        });

    mathTypeNonConst->GetStaticMembers().PushBack(SymbolTypeMember {
        "Floor",
        SymbolType::GenericInstance(
            BuiltinTypes::s_functionType,
            {}, {},
            GenericInstanceTypeInfo {
                {
                    { "@return", BuiltinTypes::s_floatType },
                    { "value", BuiltinTypes::s_floatType }
                }
            })
        });

    mathTypeNonConst->GetStaticMembers().PushBack(SymbolTypeMember {
        "Ceil",
        SymbolType::GenericInstance(
            BuiltinTypes::s_functionType,
            {}, {},
            GenericInstanceTypeInfo {
                {
                    { "@return", BuiltinTypes::s_floatType },
                    { "value", BuiltinTypes::s_floatType }
                }
            })
        });

    mathTypeNonConst->GetStaticMembers().PushBack(SymbolTypeMember {
        "Trunc",
        SymbolType::GenericInstance(
            BuiltinTypes::s_functionType,
            {}, {},
            GenericInstanceTypeInfo {
                {
                    { "@return", BuiltinTypes::s_floatType },
                    { "value", BuiltinTypes::s_floatType }
                }
            })
        });

    mathTypeNonConst->GetStaticMembers().PushBack(SymbolTypeMember {
        "Round",
        SymbolType::GenericInstance(
            BuiltinTypes::s_functionType,
            {}, {},
            GenericInstanceTypeInfo {
                {
                    { "@return", BuiltinTypes::s_floatType },
                    { "value", BuiltinTypes::s_floatType }
                }
            })
        });

    mathTypeNonConst->GetStaticMembers().PushBack(SymbolTypeMember {
        "Fract",
        SymbolType::GenericInstance(
            BuiltinTypes::s_functionType,
            {}, {},
            GenericInstanceTypeInfo {
                {
                    { "@return", BuiltinTypes::s_floatType },
                    { "value", BuiltinTypes::s_floatType }
                }
            })
        });

    mathTypeNonConst->GetStaticMembers().PushBack(SymbolTypeMember {
        "Sign",
        SymbolType::GenericInstance(
            BuiltinTypes::s_functionType,
            {}, {},
            GenericInstanceTypeInfo {
                {
                    { "@return", BuiltinTypes::s_floatType },
                    { "value", BuiltinTypes::s_floatType }
                }
            })
        });

    mathTypeNonConst->GetStaticMembers().PushBack(SymbolTypeMember {
        "Lerp",
        SymbolType::GenericInstance(
            BuiltinTypes::s_functionType,
            {}, {},
            GenericInstanceTypeInfo {
                {
                    { "@return", BuiltinTypes::s_floatType },
                    { "from", BuiltinTypes::s_floatType },
                    { "to", BuiltinTypes::s_floatType },
                    { "amt", BuiltinTypes::s_floatType }
                }
            })
        });

    mathTypeNonConst->GetStaticMembers().PushBack(SymbolTypeMember {
        "Step",
        SymbolType::GenericInstance(
            BuiltinTypes::s_functionType,
            {}, {},
            GenericInstanceTypeInfo {
                {
                    { "@return", BuiltinTypes::s_floatType },
                    { "edge", BuiltinTypes::s_floatType },
                    { "x", BuiltinTypes::s_floatType }
                }
            })
        });

    mathTypeNonConst->GetStaticMembers().PushBack(SymbolTypeMember {
        "Exp",
        SymbolType::GenericInstance(
            BuiltinTypes::s_functionType,
            {}, {},
            GenericInstanceTypeInfo {
                {
                    { "@return", BuiltinTypes::s_floatType },
                    { "value", BuiltinTypes::s_floatType }
                }
            })
        });

    mathTypeNonConst->GetStaticMembers().PushBack(SymbolTypeMember {
        "Mod",
        SymbolType::GenericInstance(
            BuiltinTypes::s_functionType,
            {}, {},
            GenericInstanceTypeInfo {
                {
                    { "@return", BuiltinTypes::s_floatType },
                    { "a", BuiltinTypes::s_floatType },
                    { "b", BuiltinTypes::s_floatType }
                }
            })
        });

    mathTypeNonConst->GetStaticMembers().PushBack(SymbolTypeMember {
        "IsNaN",
        SymbolType::GenericInstance(
            BuiltinTypes::s_functionType,
            {}, {},
            GenericInstanceTypeInfo {
                {
                    { "@return", BuiltinTypes::s_boolType },
                    { "value", BuiltinTypes::s_floatType }
                }
            })
        });

    mathTypeNonConst->GetStaticMembers().PushBack(SymbolTypeMember {
        "IsFinite",
        SymbolType::GenericInstance(
            BuiltinTypes::s_functionType,
            {}, {},
            GenericInstanceTypeInfo {
                {
                    { "@return", BuiltinTypes::s_boolType },
                    { "value", BuiltinTypes::s_floatType }
                }
            })
        });

    mathTypeNonConst->GetStaticMembers().PushBack(SymbolTypeMember {
        "ApproxEqual",
        SymbolType::GenericInstance(
            BuiltinTypes::s_functionType,
            {}, {},
            GenericInstanceTypeInfo {
                {
                    { "@return", BuiltinTypes::s_boolType },
                    { "a", BuiltinTypes::s_floatType },
                    { "b", BuiltinTypes::s_floatType }
                }
            })
        });

    mathTypeNonConst->GetStaticMembers().PushBack(SymbolTypeMember {
        "Factorial",
        SymbolType::GenericInstance(
            BuiltinTypes::s_functionType,
            {}, {},
            GenericInstanceTypeInfo {
                {
                    { "@return", BuiltinTypes::s_floatType },
                    { "value", BuiltinTypes::s_floatType }
                }
            })
        });
#pragma endregion Math

#define REGISTER_GLOBAL_TYPE(type)                                      \
    do                                                                  \
    {                                                                   \
        const_cast<SymbolType*>(type)->Register(globalCompilationUnit); \
    }                                                                   \
    while (0)

    // Register static types in global compilation unit
    REGISTER_GLOBAL_TYPE(BuiltinTypes::s_primitiveType);
    REGISTER_GLOBAL_TYPE(BuiltinTypes::s_errorType);
    REGISTER_GLOBAL_TYPE(BuiltinTypes::s_anyType);
    REGISTER_GLOBAL_TYPE(BuiltinTypes::s_placeholderType);
    REGISTER_GLOBAL_TYPE(BuiltinTypes::s_voidType);
    REGISTER_GLOBAL_TYPE(BuiltinTypes::s_objectType);
    REGISTER_GLOBAL_TYPE(BuiltinTypes::s_functionBaseType);
    REGISTER_GLOBAL_TYPE(BuiltinTypes::s_arrayBaseType);
    REGISTER_GLOBAL_TYPE(BuiltinTypes::s_varArgsBaseType);
    REGISTER_GLOBAL_TYPE(BuiltinTypes::s_mapBaseType);
    REGISTER_GLOBAL_TYPE(BuiltinTypes::s_int8Type);
    REGISTER_GLOBAL_TYPE(BuiltinTypes::s_int16Type);
    REGISTER_GLOBAL_TYPE(BuiltinTypes::s_int32Type);
    REGISTER_GLOBAL_TYPE(BuiltinTypes::s_int64Type);
    REGISTER_GLOBAL_TYPE(BuiltinTypes::s_uint8Type);
    REGISTER_GLOBAL_TYPE(BuiltinTypes::s_uint16Type);
    REGISTER_GLOBAL_TYPE(BuiltinTypes::s_uint32Type);
    REGISTER_GLOBAL_TYPE(BuiltinTypes::s_uint64Type);
    REGISTER_GLOBAL_TYPE(BuiltinTypes::s_floatType);
    REGISTER_GLOBAL_TYPE(BuiltinTypes::s_doubleType);
    REGISTER_GLOBAL_TYPE(BuiltinTypes::s_boolType);
    REGISTER_GLOBAL_TYPE(BuiltinTypes::s_nameType);
    REGISTER_GLOBAL_TYPE(BuiltinTypes::s_stringType);
    REGISTER_GLOBAL_TYPE(BuiltinTypes::s_nullType);
    REGISTER_GLOBAL_TYPE(BuiltinTypes::s_varArgsType);
    REGISTER_GLOBAL_TYPE(BuiltinTypes::s_functionType);
    REGISTER_GLOBAL_TYPE(BuiltinTypes::s_arrayType);
    REGISTER_GLOBAL_TYPE(BuiltinTypes::s_mapType);
    REGISTER_GLOBAL_TYPE(BuiltinTypes::s_classType);
    REGISTER_GLOBAL_TYPE(BuiltinTypes::s_mathType);

#undef REGISTER_GLOBAL_TYPE
}

void BuiltinTypes::RegisterTypes(CompilationUnit* compilationUnit)
{
    Assert(compilationUnit != nullptr);

    IdentifierTable& table = compilationUnit->GetGlobalModule()->scopeTree.Top().identifierTable;

    // Create new types per-compilation unit:

    SymbolType* sbyteType = SymbolType::Alias("sbyte", { BuiltinTypes::s_int8Type });
    sbyteType->Register(compilationUnit);

    SymbolType* byteType = SymbolType::Alias("byte", { BuiltinTypes::s_uint8Type });
    byteType->Register(compilationUnit);

    SymbolType* shortType = SymbolType::Alias("short", { BuiltinTypes::s_int16Type });
    shortType->Register(compilationUnit);

    SymbolType* ushortType = SymbolType::Alias("ushort", { BuiltinTypes::s_uint16Type });
    ushortType->Register(compilationUnit);

    SymbolType* intType = SymbolType::Alias("int", { BuiltinTypes::s_int32Type });
    intType->Register(compilationUnit);

    SymbolType* uintType = SymbolType::Alias("uint", { BuiltinTypes::s_uint32Type });
    uintType->Register(compilationUnit);

    SymbolType* longType = SymbolType::Alias("long", { BuiltinTypes::s_int64Type });
    longType->Register(compilationUnit);

    SymbolType* ulongType = SymbolType::Alias("ulong", { BuiltinTypes::s_uint64Type });
    ulongType->Register(compilationUnit);

    SymbolType* uintptrType = SymbolType::Alias("UIntPtr", { sizeof(void*) == 4 ? BuiltinTypes::s_uint32Type : BuiltinTypes::s_uint64Type });
    uintptrType->Register(compilationUnit);

    SymbolType* intptrType = SymbolType::Alias("IntPtr", { sizeof(void*) == 4 ? BuiltinTypes::s_int32Type : BuiltinTypes::s_int64Type });
    intptrType->Register(compilationUnit);

    SymbolType* vec2iType = SymbolType::Struct(
        "Vec2i",
        {
            SymbolTypeMember { "x", const_cast<SymbolType*>(BuiltinTypes::s_int32Type) },
            SymbolTypeMember { "y", const_cast<SymbolType*>(BuiltinTypes::s_int32Type) }
        },
        {});

    vec2iType->Register(compilationUnit);

    SymbolType* vec2uType = SymbolType::Struct(
        "Vec2u",
        {
            SymbolTypeMember { "x", const_cast<SymbolType*>(BuiltinTypes::s_uint32Type) },
            SymbolTypeMember { "y", const_cast<SymbolType*>(BuiltinTypes::s_uint32Type) }
        },
        {});

    vec2uType->Register(compilationUnit);

    SymbolType* vec2fType = SymbolType::Struct(
        "Vec2f",
        {
            SymbolTypeMember { "x", const_cast<SymbolType*>(BuiltinTypes::s_floatType) },
            SymbolTypeMember { "y", const_cast<SymbolType*>(BuiltinTypes::s_floatType) }
        },
        {});

    vec2fType->Register(compilationUnit);

    SymbolType* vec3iType = SymbolType::Struct(
        "Vec3i",
        {
            SymbolTypeMember { "x", const_cast<SymbolType*>(BuiltinTypes::s_int32Type) },
            SymbolTypeMember { "y", const_cast<SymbolType*>(BuiltinTypes::s_int32Type) },
            SymbolTypeMember { "z", const_cast<SymbolType*>(BuiltinTypes::s_int32Type) }
        },
        {});

    vec3iType->Register(compilationUnit);

    SymbolType* vec3uType = SymbolType::Struct(
        "Vec3u",
        {
            SymbolTypeMember { "x", const_cast<SymbolType*>(BuiltinTypes::s_uint32Type) },
            SymbolTypeMember { "y", const_cast<SymbolType*>(BuiltinTypes::s_uint32Type) },
            SymbolTypeMember { "z", const_cast<SymbolType*>(BuiltinTypes::s_uint32Type) }
        },
        {});

    vec3uType->Register(compilationUnit);

    SymbolType* vec3fType = SymbolType::Struct(
        "Vec3f",
        {
            SymbolTypeMember { "x", const_cast<SymbolType*>(BuiltinTypes::s_floatType) },
            SymbolTypeMember { "y", const_cast<SymbolType*>(BuiltinTypes::s_floatType) },
            SymbolTypeMember { "z", const_cast<SymbolType*>(BuiltinTypes::s_floatType) }
        },
        {});

    vec3fType->Register(compilationUnit);

    SymbolType* vec4iType = SymbolType::Struct(
        "Vec4i",
        {
            SymbolTypeMember { "x", const_cast<SymbolType*>(BuiltinTypes::s_int32Type) },
            SymbolTypeMember { "y", const_cast<SymbolType*>(BuiltinTypes::s_int32Type) },
            SymbolTypeMember { "z", const_cast<SymbolType*>(BuiltinTypes::s_int32Type) },
            SymbolTypeMember { "w", const_cast<SymbolType*>(BuiltinTypes::s_int32Type) }
        },
        {});

    vec4iType->Register(compilationUnit);

    SymbolType* vec4uType = SymbolType::Struct(
        "Vec4u",
        {
            SymbolTypeMember { "x", const_cast<SymbolType*>(BuiltinTypes::s_uint32Type) },
            SymbolTypeMember { "y", const_cast<SymbolType*>(BuiltinTypes::s_uint32Type) },
            SymbolTypeMember { "z", const_cast<SymbolType*>(BuiltinTypes::s_uint32Type) },
            SymbolTypeMember { "w", const_cast<SymbolType*>(BuiltinTypes::s_uint32Type) }
        },
        {});

    vec4uType->Register(compilationUnit);

    SymbolType* vec4fType = SymbolType::Struct(
        "Vec4f",
        {
            SymbolTypeMember { "x", const_cast<SymbolType*>(BuiltinTypes::s_floatType) },
            SymbolTypeMember { "y", const_cast<SymbolType*>(BuiltinTypes::s_floatType) },
            SymbolTypeMember { "z", const_cast<SymbolType*>(BuiltinTypes::s_floatType) },
            SymbolTypeMember { "w", const_cast<SymbolType*>(BuiltinTypes::s_floatType) }
        },
        {});

    vec4fType->Register(compilationUnit);

    SymbolType* byteBufferType = SymbolType::Struct("ByteBuffer", {}, {});
    byteBufferType->Register(compilationUnit);

    const SymbolType* const s_globalVisibleTypes[] {
        BuiltinTypes::s_anyType,
        BuiltinTypes::s_objectType,
        BuiltinTypes::s_voidType,
        BuiltinTypes::s_int8Type,
        BuiltinTypes::s_int16Type,
        BuiltinTypes::s_int32Type,
        BuiltinTypes::s_int64Type,
        BuiltinTypes::s_uint8Type,
        BuiltinTypes::s_uint16Type,
        BuiltinTypes::s_uint32Type,
        BuiltinTypes::s_uint64Type,
        BuiltinTypes::s_floatType,
        BuiltinTypes::s_doubleType,
        BuiltinTypes::s_boolType,
        BuiltinTypes::s_nameType,
        BuiltinTypes::s_stringType,
        BuiltinTypes::s_functionType,
        BuiltinTypes::s_arrayType,
        BuiltinTypes::s_mapType,
        BuiltinTypes::s_classType,
        BuiltinTypes::s_mathType,
        sbyteType,
        byteType,
        shortType,
        ushortType,
        intType,
        uintType,
        longType,
        ulongType,
        uintptrType,
        intptrType,
        vec2iType,
        vec2uType,
        vec2fType,
        vec3iType,
        vec3uType,
        vec3fType,
        vec4iType,
        vec4uType,
        vec4fType,
        byteBufferType
    };

    for (const SymbolType* type : s_globalVisibleTypes)
    {
        table.AddSymbolType(const_cast<SymbolType*>(type));
    }
}

const String& BuiltinTypes::GetNativeClassNameForType(const SymbolType* type)
{
    if (!type)
    {
        return String::empty;
    }

    // Handle builtin types
    if (type->IsString())
    {
        return s_stringClassName;
    }

    if (type->IsMapType())
    {
        return s_mapClassName;
    }

    if (type->IsName())
    {
        return s_nameClassName;
    }

    if (type->IsClassType())
    {
        return s_classRefClassName;
    }

    if (type == BuiltinTypes::s_mathType)
    {
        return s_mathClassName;
    }

    return type->GetName();
}

} // namespace Hyperion
