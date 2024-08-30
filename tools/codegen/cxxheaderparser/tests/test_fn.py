# Note: testcases generated via `python -m cxxheaderparser.gentest`

from cxxheaderparser.types import (
    Array,
    AutoSpecifier,
    ClassDecl,
    DecltypeSpecifier,
    Field,
    Function,
    FunctionType,
    FundamentalSpecifier,
    Method,
    MoveReference,
    NameSpecifier,
    PQName,
    Parameter,
    Pointer,
    Reference,
    TemplateArgument,
    TemplateDecl,
    TemplateSpecialization,
    TemplateTypeParam,
    Token,
    Type,
    Typedef,
    Value,
)
from cxxheaderparser.simple import (
    ClassScope,
    NamespaceScope,
    parse_string,
    ParsedData,
)


def test_fn_returns_class() -> None:
    content = """
      class X *fn1();
      struct Y fn2();
      enum E fn3();
      
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            functions=[
                Function(
                    return_type=Pointer(
                        ptr_to=Type(
                            typename=PQName(
                                segments=[NameSpecifier(name="X")], classkey="class"
                            )
                        )
                    ),
                    name=PQName(segments=[NameSpecifier(name="fn1")]),
                    parameters=[],
                ),
                Function(
                    return_type=Type(
                        typename=PQName(
                            segments=[NameSpecifier(name="Y")], classkey="struct"
                        )
                    ),
                    name=PQName(segments=[NameSpecifier(name="fn2")]),
                    parameters=[],
                ),
                Function(
                    return_type=Type(
                        typename=PQName(
                            segments=[NameSpecifier(name="E")], classkey="enum"
                        )
                    ),
                    name=PQName(segments=[NameSpecifier(name="fn3")]),
                    parameters=[],
                ),
            ]
        )
    )


def test_fn_returns_typename() -> None:
    content = """
      typename ns::X fn();
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            functions=[
                Function(
                    return_type=Type(
                        typename=PQName(
                            segments=[
                                NameSpecifier(name="ns"),
                                NameSpecifier(name="X"),
                            ],
                            has_typename=True,
                        )
                    ),
                    name=PQName(segments=[NameSpecifier(name="fn")]),
                    parameters=[],
                )
            ]
        )
    )


def test_fn_returns_typename_const() -> None:
    content = """
      const typename ns::X fn();
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            functions=[
                Function(
                    return_type=Type(
                        typename=PQName(
                            segments=[
                                NameSpecifier(name="ns"),
                                NameSpecifier(name="X"),
                            ],
                            has_typename=True,
                        ),
                        const=True,
                    ),
                    name=PQName(segments=[NameSpecifier(name="fn")]),
                    parameters=[],
                )
            ]
        )
    )


def test_fn_pointer_params() -> None:
    content = """
      int fn1(int *);
      int fn2(int *p);
      int fn3(int(*p));
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            functions=[
                Function(
                    return_type=Type(
                        typename=PQName(segments=[FundamentalSpecifier(name="int")])
                    ),
                    name=PQName(segments=[NameSpecifier(name="fn1")]),
                    parameters=[
                        Parameter(
                            type=Pointer(
                                ptr_to=Type(
                                    typename=PQName(
                                        segments=[FundamentalSpecifier(name="int")]
                                    )
                                )
                            ),
                        )
                    ],
                ),
                Function(
                    return_type=Type(
                        typename=PQName(segments=[FundamentalSpecifier(name="int")])
                    ),
                    name=PQName(segments=[NameSpecifier(name="fn2")]),
                    parameters=[
                        Parameter(
                            name="p",
                            type=Pointer(
                                ptr_to=Type(
                                    typename=PQName(
                                        segments=[FundamentalSpecifier(name="int")]
                                    )
                                )
                            ),
                        )
                    ],
                ),
                Function(
                    return_type=Type(
                        typename=PQName(segments=[FundamentalSpecifier(name="int")])
                    ),
                    name=PQName(segments=[NameSpecifier(name="fn3")]),
                    parameters=[
                        Parameter(
                            name="p",
                            type=Pointer(
                                ptr_to=Type(
                                    typename=PQName(
                                        segments=[FundamentalSpecifier(name="int")]
                                    )
                                )
                            ),
                        )
                    ],
                ),
            ]
        )
    )


def test_fn_void_is_no_params() -> None:
    content = """
      int fn(void);
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            functions=[
                Function(
                    return_type=Type(
                        typename=PQName(segments=[FundamentalSpecifier(name="int")])
                    ),
                    name=PQName(segments=[NameSpecifier(name="fn")]),
                    parameters=[],
                )
            ]
        )
    )


def test_fn_array_param() -> None:
    content = """
      void fn(int array[]);
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            functions=[
                Function(
                    return_type=Type(
                        typename=PQName(segments=[FundamentalSpecifier(name="void")])
                    ),
                    name=PQName(segments=[NameSpecifier(name="fn")]),
                    parameters=[
                        Parameter(
                            name="array",
                            type=Array(
                                array_of=Type(
                                    typename=PQName(
                                        segments=[FundamentalSpecifier(name="int")]
                                    )
                                ),
                                size=None,
                            ),
                        )
                    ],
                )
            ]
        )
    )


def test_fn_typename_param() -> None:
    content = """
      void MethodA(const mynamespace::SomeObject &x,
                   typename mynamespace::SomeObject * = 0);
      
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            functions=[
                Function(
                    return_type=Type(
                        typename=PQName(segments=[FundamentalSpecifier(name="void")])
                    ),
                    name=PQName(segments=[NameSpecifier(name="MethodA")]),
                    parameters=[
                        Parameter(
                            type=Reference(
                                ref_to=Type(
                                    typename=PQName(
                                        segments=[
                                            NameSpecifier(name="mynamespace"),
                                            NameSpecifier(name="SomeObject"),
                                        ]
                                    ),
                                    const=True,
                                )
                            ),
                            name="x",
                        ),
                        Parameter(
                            type=Pointer(
                                ptr_to=Type(
                                    typename=PQName(
                                        segments=[
                                            NameSpecifier(name="mynamespace"),
                                            NameSpecifier(name="SomeObject"),
                                        ],
                                        has_typename=True,
                                    )
                                )
                            ),
                            default=Value(tokens=[Token(value="0")]),
                        ),
                    ],
                )
            ]
        )
    )


def test_fn_weird_refs() -> None:
    content = """
      int aref(int(&x));
      void ptr_ref(int(*&name));
      void ref_to_array(int (&array)[]);
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            functions=[
                Function(
                    return_type=Type(
                        typename=PQName(segments=[FundamentalSpecifier(name="int")])
                    ),
                    name=PQName(segments=[NameSpecifier(name="aref")]),
                    parameters=[
                        Parameter(
                            name="x",
                            type=Reference(
                                ref_to=Type(
                                    typename=PQName(
                                        segments=[FundamentalSpecifier(name="int")]
                                    )
                                )
                            ),
                        )
                    ],
                ),
                Function(
                    return_type=Type(
                        typename=PQName(segments=[FundamentalSpecifier(name="void")])
                    ),
                    name=PQName(segments=[NameSpecifier(name="ptr_ref")]),
                    parameters=[
                        Parameter(
                            name="name",
                            type=Reference(
                                ref_to=Pointer(
                                    ptr_to=Type(
                                        typename=PQName(
                                            segments=[FundamentalSpecifier(name="int")]
                                        )
                                    )
                                )
                            ),
                        )
                    ],
                ),
                Function(
                    return_type=Type(
                        typename=PQName(segments=[FundamentalSpecifier(name="void")])
                    ),
                    name=PQName(segments=[NameSpecifier(name="ref_to_array")]),
                    parameters=[
                        Parameter(
                            name="array",
                            type=Reference(
                                ref_to=Array(
                                    array_of=Type(
                                        typename=PQName(
                                            segments=[FundamentalSpecifier(name="int")]
                                        )
                                    ),
                                    size=None,
                                )
                            ),
                        )
                    ],
                ),
            ]
        )
    )


def test_fn_too_many_parens() -> None:
    content = """
      int fn1(int (x));
      void (fn2 (int (*const (name))));
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            functions=[
                Function(
                    return_type=Type(
                        typename=PQName(segments=[FundamentalSpecifier(name="int")])
                    ),
                    name=PQName(segments=[NameSpecifier(name="fn1")]),
                    parameters=[
                        Parameter(
                            name="x",
                            type=Type(
                                typename=PQName(
                                    segments=[FundamentalSpecifier(name="int")]
                                )
                            ),
                        )
                    ],
                ),
                Function(
                    return_type=Type(
                        typename=PQName(segments=[FundamentalSpecifier(name="void")])
                    ),
                    name=PQName(segments=[NameSpecifier(name="fn2")]),
                    parameters=[
                        Parameter(
                            name="name",
                            type=Pointer(
                                ptr_to=Type(
                                    typename=PQName(
                                        segments=[FundamentalSpecifier(name="int")]
                                    )
                                ),
                                const=True,
                            ),
                        )
                    ],
                ),
            ]
        )
    )


# TODO calling conventions
"""
void __stdcall fn();
void (__stdcall * fn)
"""


def test_fn_same_line() -> None:
    # multiple functions on the same line
    content = """
      void fn1(), fn2();
      void *fn3(), fn4();
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            functions=[
                Function(
                    return_type=Type(
                        typename=PQName(segments=[FundamentalSpecifier(name="void")])
                    ),
                    name=PQName(segments=[NameSpecifier(name="fn1")]),
                    parameters=[],
                ),
                Function(
                    return_type=Type(
                        typename=PQName(segments=[FundamentalSpecifier(name="void")])
                    ),
                    name=PQName(segments=[NameSpecifier(name="fn2")]),
                    parameters=[],
                ),
                Function(
                    return_type=Pointer(
                        ptr_to=Type(
                            typename=PQName(
                                segments=[FundamentalSpecifier(name="void")]
                            )
                        )
                    ),
                    name=PQName(segments=[NameSpecifier(name="fn3")]),
                    parameters=[],
                ),
                Function(
                    return_type=Type(
                        typename=PQName(segments=[FundamentalSpecifier(name="void")])
                    ),
                    name=PQName(segments=[NameSpecifier(name="fn4")]),
                    parameters=[],
                ),
            ]
        )
    )


def test_fn_auto_template() -> None:
    content = """
      template<class T, class U>
      auto add(T t, U u) { return t + u; }
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            functions=[
                Function(
                    return_type=Type(typename=PQName(segments=[AutoSpecifier()])),
                    name=PQName(segments=[NameSpecifier(name="add")]),
                    parameters=[
                        Parameter(
                            type=Type(
                                typename=PQName(segments=[NameSpecifier(name="T")])
                            ),
                            name="t",
                        ),
                        Parameter(
                            type=Type(
                                typename=PQName(segments=[NameSpecifier(name="U")])
                            ),
                            name="u",
                        ),
                    ],
                    has_body=True,
                    template=TemplateDecl(
                        params=[
                            TemplateTypeParam(typekey="class", name="T"),
                            TemplateTypeParam(typekey="class", name="U"),
                        ]
                    ),
                )
            ]
        )
    )


def test_fn_template_ptr() -> None:
    content = """
      std::vector<Pointer *> *fn(std::vector<Pointer *> *ps);
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            functions=[
                Function(
                    return_type=Pointer(
                        ptr_to=Type(
                            typename=PQName(
                                segments=[
                                    NameSpecifier(name="std"),
                                    NameSpecifier(
                                        name="vector",
                                        specialization=TemplateSpecialization(
                                            args=[
                                                TemplateArgument(
                                                    arg=Pointer(
                                                        ptr_to=Type(
                                                            typename=PQName(
                                                                segments=[
                                                                    NameSpecifier(
                                                                        name="Pointer"
                                                                    )
                                                                ]
                                                            )
                                                        )
                                                    )
                                                )
                                            ]
                                        ),
                                    ),
                                ]
                            )
                        )
                    ),
                    name=PQName(segments=[NameSpecifier(name="fn")]),
                    parameters=[
                        Parameter(
                            type=Pointer(
                                ptr_to=Type(
                                    typename=PQName(
                                        segments=[
                                            NameSpecifier(name="std"),
                                            NameSpecifier(
                                                name="vector",
                                                specialization=TemplateSpecialization(
                                                    args=[
                                                        TemplateArgument(
                                                            arg=Pointer(
                                                                ptr_to=Type(
                                                                    typename=PQName(
                                                                        segments=[
                                                                            NameSpecifier(
                                                                                name="Pointer"
                                                                            )
                                                                        ]
                                                                    )
                                                                )
                                                            )
                                                        )
                                                    ]
                                                ),
                                            ),
                                        ]
                                    )
                                )
                            ),
                            name="ps",
                        )
                    ],
                )
            ]
        )
    )


def test_fn_with_impl() -> None:
    content = """
      // clang-format off
      void termite(void)
      {
          return ((structA*) (Func())->element);
      }
      
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            functions=[
                Function(
                    return_type=Type(
                        typename=PQName(segments=[FundamentalSpecifier(name="void")])
                    ),
                    name=PQName(segments=[NameSpecifier(name="termite")]),
                    parameters=[],
                    has_body=True,
                )
            ]
        )
    )


def test_fn_return_std_function() -> None:
    content = """
      std::function<void(int)> fn();
    """
    data1 = parse_string(content, cleandoc=True)

    content = """
      std::function<void((int))> fn();
    """
    data2 = parse_string(content, cleandoc=True)

    expected = ParsedData(
        namespace=NamespaceScope(
            functions=[
                Function(
                    return_type=Type(
                        typename=PQName(
                            segments=[
                                NameSpecifier(name="std"),
                                NameSpecifier(
                                    name="function",
                                    specialization=TemplateSpecialization(
                                        args=[
                                            TemplateArgument(
                                                arg=FunctionType(
                                                    return_type=Type(
                                                        typename=PQName(
                                                            segments=[
                                                                FundamentalSpecifier(
                                                                    name="void"
                                                                )
                                                            ]
                                                        )
                                                    ),
                                                    parameters=[
                                                        Parameter(
                                                            type=Type(
                                                                typename=PQName(
                                                                    segments=[
                                                                        FundamentalSpecifier(
                                                                            name="int"
                                                                        )
                                                                    ]
                                                                )
                                                            )
                                                        )
                                                    ],
                                                )
                                            )
                                        ]
                                    ),
                                ),
                            ]
                        )
                    ),
                    name=PQName(segments=[NameSpecifier(name="fn")]),
                    parameters=[],
                )
            ]
        )
    )

    assert data1 == expected
    assert data2 == expected


def test_fn_return_std_function_trailing() -> None:
    content = """
      std::function<auto(int)->int> fn();
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            functions=[
                Function(
                    return_type=Type(
                        typename=PQName(
                            segments=[
                                NameSpecifier(name="std"),
                                NameSpecifier(
                                    name="function",
                                    specialization=TemplateSpecialization(
                                        args=[
                                            TemplateArgument(
                                                arg=FunctionType(
                                                    return_type=Type(
                                                        typename=PQName(
                                                            segments=[
                                                                FundamentalSpecifier(
                                                                    name="int"
                                                                )
                                                            ]
                                                        )
                                                    ),
                                                    parameters=[
                                                        Parameter(
                                                            type=Type(
                                                                typename=PQName(
                                                                    segments=[
                                                                        FundamentalSpecifier(
                                                                            name="int"
                                                                        )
                                                                    ]
                                                                )
                                                            )
                                                        )
                                                    ],
                                                    has_trailing_return=True,
                                                )
                                            )
                                        ]
                                    ),
                                ),
                            ]
                        )
                    ),
                    name=PQName(segments=[NameSpecifier(name="fn")]),
                    parameters=[],
                )
            ]
        )
    )


def test_fn_trailing_return_simple() -> None:
    content = """
      auto fn() -> int;
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            functions=[
                Function(
                    return_type=Type(
                        typename=PQName(segments=[FundamentalSpecifier(name="int")])
                    ),
                    name=PQName(segments=[NameSpecifier(name="fn")]),
                    parameters=[],
                    has_trailing_return=True,
                )
            ]
        )
    )


def test_fn_trailing_return_std_function() -> None:
    content = """
      auto fn() -> std::function<int()>;
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            functions=[
                Function(
                    return_type=Type(
                        typename=PQName(
                            segments=[
                                NameSpecifier(name="std"),
                                NameSpecifier(
                                    name="function",
                                    specialization=TemplateSpecialization(
                                        args=[
                                            TemplateArgument(
                                                arg=FunctionType(
                                                    return_type=Type(
                                                        typename=PQName(
                                                            segments=[
                                                                FundamentalSpecifier(
                                                                    name="int"
                                                                )
                                                            ]
                                                        )
                                                    ),
                                                    parameters=[],
                                                )
                                            )
                                        ]
                                    ),
                                ),
                            ]
                        )
                    ),
                    name=PQName(segments=[NameSpecifier(name="fn")]),
                    parameters=[],
                    has_trailing_return=True,
                )
            ]
        )
    )


def test_inline_volatile_fn() -> None:
    content = """
      inline int Standard_Atomic_Increment (volatile int* theValue);
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            functions=[
                Function(
                    return_type=Type(
                        typename=PQName(segments=[FundamentalSpecifier(name="int")])
                    ),
                    name=PQName(
                        segments=[NameSpecifier(name="Standard_Atomic_Increment")]
                    ),
                    parameters=[
                        Parameter(
                            type=Pointer(
                                ptr_to=Type(
                                    typename=PQName(
                                        segments=[FundamentalSpecifier(name="int")]
                                    ),
                                    volatile=True,
                                )
                            ),
                            name="theValue",
                        )
                    ],
                    inline=True,
                )
            ]
        )
    )


def test_method_w_reference() -> None:
    content = """
      struct StreamBuffer
      {
          StreamBuffer &operator<<(std::ostream &(*fn)(std::ostream &))
          {
              return *this;
          }
      };
      
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            classes=[
                ClassScope(
                    class_decl=ClassDecl(
                        typename=PQName(
                            segments=[NameSpecifier(name="StreamBuffer")],
                            classkey="struct",
                        )
                    ),
                    methods=[
                        Method(
                            return_type=Reference(
                                ref_to=Type(
                                    typename=PQName(
                                        segments=[NameSpecifier(name="StreamBuffer")]
                                    )
                                )
                            ),
                            name=PQName(segments=[NameSpecifier(name="operator<<")]),
                            parameters=[
                                Parameter(
                                    type=Pointer(
                                        ptr_to=FunctionType(
                                            return_type=Reference(
                                                ref_to=Type(
                                                    typename=PQName(
                                                        segments=[
                                                            NameSpecifier(name="std"),
                                                            NameSpecifier(
                                                                name="ostream"
                                                            ),
                                                        ]
                                                    )
                                                )
                                            ),
                                            parameters=[
                                                Parameter(
                                                    type=Reference(
                                                        ref_to=Type(
                                                            typename=PQName(
                                                                segments=[
                                                                    NameSpecifier(
                                                                        name="std"
                                                                    ),
                                                                    NameSpecifier(
                                                                        name="ostream"
                                                                    ),
                                                                ]
                                                            )
                                                        )
                                                    )
                                                )
                                            ],
                                        )
                                    ),
                                    name="fn",
                                )
                            ],
                            has_body=True,
                            access="public",
                            operator="<<",
                        )
                    ],
                )
            ]
        )
    )


def test_fn_w_mvreference() -> None:
    content = """
      void fn1(int && (*)(int));
      
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            functions=[
                Function(
                    return_type=Type(
                        typename=PQName(segments=[FundamentalSpecifier(name="void")])
                    ),
                    name=PQName(segments=[NameSpecifier(name="fn1")]),
                    parameters=[
                        Parameter(
                            type=Pointer(
                                ptr_to=FunctionType(
                                    return_type=MoveReference(
                                        moveref_to=Type(
                                            typename=PQName(
                                                segments=[
                                                    FundamentalSpecifier(name="int")
                                                ]
                                            )
                                        )
                                    ),
                                    parameters=[
                                        Parameter(
                                            type=Type(
                                                typename=PQName(
                                                    segments=[
                                                        FundamentalSpecifier(name="int")
                                                    ]
                                                )
                                            )
                                        )
                                    ],
                                )
                            )
                        )
                    ],
                )
            ]
        )
    )


def test_msvc_conventions() -> None:
    content = """
      void __cdecl fn();
      typedef const char* (__stdcall *wglGetExtensionsStringARB_t)(HDC theDeviceContext);
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            functions=[
                Function(
                    return_type=Type(
                        typename=PQName(segments=[FundamentalSpecifier(name="void")])
                    ),
                    name=PQName(segments=[NameSpecifier(name="fn")]),
                    parameters=[],
                    msvc_convention="__cdecl",
                )
            ],
            typedefs=[
                Typedef(
                    type=Pointer(
                        ptr_to=FunctionType(
                            return_type=Pointer(
                                ptr_to=Type(
                                    typename=PQName(
                                        segments=[FundamentalSpecifier(name="char")]
                                    ),
                                    const=True,
                                )
                            ),
                            parameters=[
                                Parameter(
                                    type=Type(
                                        typename=PQName(
                                            segments=[NameSpecifier(name="HDC")]
                                        )
                                    ),
                                    name="theDeviceContext",
                                )
                            ],
                            msvc_convention="__stdcall",
                        )
                    ),
                    name="wglGetExtensionsStringARB_t",
                )
            ],
        )
    )


def test_throw_empty() -> None:
    content = """
      void foo() throw() { throw std::runtime_error("foo"); }
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            functions=[
                Function(
                    return_type=Type(
                        typename=PQName(segments=[FundamentalSpecifier(name="void")])
                    ),
                    name=PQName(segments=[NameSpecifier(name="foo")]),
                    parameters=[],
                    has_body=True,
                    throw=Value(tokens=[]),
                )
            ]
        )
    )


def test_throw_dynamic() -> None:
    content = """
      void foo() throw(std::exception) { throw std::runtime_error("foo"); }
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            functions=[
                Function(
                    return_type=Type(
                        typename=PQName(segments=[FundamentalSpecifier(name="void")])
                    ),
                    name=PQName(segments=[NameSpecifier(name="foo")]),
                    parameters=[],
                    has_body=True,
                    throw=Value(
                        tokens=[
                            Token(value="std"),
                            Token(value="::"),
                            Token(value="exception"),
                        ]
                    ),
                )
            ]
        )
    )


def test_noexcept_empty() -> None:
    content = """
      void foo() noexcept;
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            functions=[
                Function(
                    return_type=Type(
                        typename=PQName(segments=[FundamentalSpecifier(name="void")])
                    ),
                    name=PQName(segments=[NameSpecifier(name="foo")]),
                    parameters=[],
                    noexcept=Value(tokens=[]),
                )
            ]
        )
    )


def test_noexcept_contents() -> None:
    content = """
      void foo() noexcept(false);
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            functions=[
                Function(
                    return_type=Type(
                        typename=PQName(segments=[FundamentalSpecifier(name="void")])
                    ),
                    name=PQName(segments=[NameSpecifier(name="foo")]),
                    parameters=[],
                    noexcept=Value(tokens=[Token(value="false")]),
                )
            ]
        )
    )


def test_auto_decltype_return() -> None:
    content = """
      class C {
      public:
        int x;
        auto GetSelected() -> decltype(x);
      };
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            classes=[
                ClassScope(
                    class_decl=ClassDecl(
                        typename=PQName(
                            segments=[NameSpecifier(name="C")], classkey="class"
                        )
                    ),
                    fields=[
                        Field(
                            access="public",
                            type=Type(
                                typename=PQName(
                                    segments=[FundamentalSpecifier(name="int")]
                                )
                            ),
                            name="x",
                        )
                    ],
                    methods=[
                        Method(
                            return_type=Type(
                                typename=PQName(
                                    segments=[
                                        DecltypeSpecifier(tokens=[Token(value="x")])
                                    ]
                                )
                            ),
                            name=PQName(segments=[NameSpecifier(name="GetSelected")]),
                            parameters=[],
                            has_trailing_return=True,
                            access="public",
                        )
                    ],
                )
            ]
        )
    )


def test_fn_trailing_return_with_body() -> None:
    content = """
      auto test() -> void
      {
      }
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            functions=[
                Function(
                    return_type=Type(
                        typename=PQName(segments=[FundamentalSpecifier(name="void")])
                    ),
                    name=PQName(segments=[NameSpecifier(name="test")]),
                    parameters=[],
                    has_body=True,
                    has_trailing_return=True,
                )
            ]
        )
    )


def test_method_trailing_return_with_body() -> None:
    content = """
      struct X {
          auto test() -> void
          {
          }
      };
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            classes=[
                ClassScope(
                    class_decl=ClassDecl(
                        typename=PQName(
                            segments=[NameSpecifier(name="X")], classkey="struct"
                        )
                    ),
                    methods=[
                        Method(
                            return_type=Type(
                                typename=PQName(
                                    segments=[FundamentalSpecifier(name="void")]
                                )
                            ),
                            name=PQName(segments=[NameSpecifier(name="test")]),
                            parameters=[],
                            has_body=True,
                            has_trailing_return=True,
                            access="public",
                        )
                    ],
                )
            ]
        )
    )


def test_msvc_inline() -> None:
    content = """
      __inline double fn1() {}
      __forceinline double fn2() {}
      static __inline double fn3() {}
      static __forceinline double fn4() {}
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            functions=[
                Function(
                    return_type=Type(
                        typename=PQName(segments=[FundamentalSpecifier(name="double")])
                    ),
                    name=PQName(segments=[NameSpecifier(name="fn1")]),
                    parameters=[],
                    inline=True,
                    has_body=True,
                ),
                Function(
                    return_type=Type(
                        typename=PQName(segments=[FundamentalSpecifier(name="double")])
                    ),
                    name=PQName(segments=[NameSpecifier(name="fn2")]),
                    parameters=[],
                    inline=True,
                    has_body=True,
                ),
                Function(
                    return_type=Type(
                        typename=PQName(segments=[FundamentalSpecifier(name="double")])
                    ),
                    name=PQName(segments=[NameSpecifier(name="fn3")]),
                    parameters=[],
                    static=True,
                    inline=True,
                    has_body=True,
                ),
                Function(
                    return_type=Type(
                        typename=PQName(segments=[FundamentalSpecifier(name="double")])
                    ),
                    name=PQName(segments=[NameSpecifier(name="fn4")]),
                    parameters=[],
                    static=True,
                    inline=True,
                    has_body=True,
                ),
            ]
        )
    )
