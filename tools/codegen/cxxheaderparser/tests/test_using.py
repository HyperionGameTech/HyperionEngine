# Note: testcases generated via `python -m cxxheaderparser.gentest`

from cxxheaderparser.types import (
    BaseClass,
    ClassDecl,
    Function,
    FunctionType,
    FundamentalSpecifier,
    Method,
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
    UsingAlias,
    UsingDecl,
)
from cxxheaderparser.simple import (
    ClassScope,
    NamespaceScope,
    UsingNamespace,
    parse_string,
    ParsedData,
)


def test_using_namespace() -> None:
    content = """
      using namespace foo;
      using namespace foo::bar;
      using namespace ::foo;
      using namespace ::foo::bar;
      
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            using_ns=[
                UsingNamespace(ns="foo"),
                UsingNamespace(ns="foo::bar"),
                UsingNamespace(ns="::foo"),
                UsingNamespace(ns="::foo::bar"),
            ]
        )
    )


def test_using_declaration() -> None:
    content = """
      using ::foo;
      using foo::bar;
      using ::foo::bar;
      using typename ::foo::bar;
      using typename foo::bar;
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            using=[
                UsingDecl(
                    typename=PQName(
                        segments=[NameSpecifier(name=""), NameSpecifier(name="foo")]
                    )
                ),
                UsingDecl(
                    typename=PQName(
                        segments=[NameSpecifier(name="foo"), NameSpecifier(name="bar")]
                    )
                ),
                UsingDecl(
                    typename=PQName(
                        segments=[
                            NameSpecifier(name=""),
                            NameSpecifier(name="foo"),
                            NameSpecifier(name="bar"),
                        ]
                    )
                ),
                UsingDecl(
                    typename=PQName(
                        segments=[
                            NameSpecifier(name=""),
                            NameSpecifier(name="foo"),
                            NameSpecifier(name="bar"),
                        ]
                    )
                ),
                UsingDecl(
                    typename=PQName(
                        segments=[NameSpecifier(name="foo"), NameSpecifier(name="bar")]
                    )
                ),
            ]
        )
    )


# alias-declaration
def test_alias_declaration_1() -> None:
    content = """
      using alias = foo;
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            using_alias=[
                UsingAlias(
                    alias="alias",
                    type=Type(typename=PQName(segments=[NameSpecifier(name="foo")])),
                )
            ]
        )
    )


def test_alias_declaration_2() -> None:
    content = """
      template <typename T> using alias = foo<T>;
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            using_alias=[
                UsingAlias(
                    alias="alias",
                    type=Type(
                        typename=PQName(
                            segments=[
                                NameSpecifier(
                                    name="foo",
                                    specialization=TemplateSpecialization(
                                        args=[
                                            TemplateArgument(
                                                arg=Type(
                                                    typename=PQName(
                                                        segments=[
                                                            NameSpecifier(name="T")
                                                        ]
                                                    )
                                                )
                                            )
                                        ]
                                    ),
                                )
                            ]
                        )
                    ),
                    template=TemplateDecl(
                        params=[TemplateTypeParam(typekey="typename", name="T")]
                    ),
                )
            ]
        )
    )


def test_alias_declaration_3() -> None:
    content = """
      using alias = ::foo::bar;
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            using_alias=[
                UsingAlias(
                    alias="alias",
                    type=Type(
                        typename=PQName(
                            segments=[
                                NameSpecifier(name=""),
                                NameSpecifier(name="foo"),
                                NameSpecifier(name="bar"),
                            ]
                        )
                    ),
                )
            ]
        )
    )


def test_alias_declaration_4() -> None:
    content = """
      template <typename T> using alias = ::foo::bar<T>;
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            using_alias=[
                UsingAlias(
                    alias="alias",
                    type=Type(
                        typename=PQName(
                            segments=[
                                NameSpecifier(name=""),
                                NameSpecifier(name="foo"),
                                NameSpecifier(
                                    name="bar",
                                    specialization=TemplateSpecialization(
                                        args=[
                                            TemplateArgument(
                                                arg=Type(
                                                    typename=PQName(
                                                        segments=[
                                                            NameSpecifier(name="T")
                                                        ]
                                                    )
                                                )
                                            )
                                        ]
                                    ),
                                ),
                            ]
                        )
                    ),
                    template=TemplateDecl(
                        params=[TemplateTypeParam(typekey="typename", name="T")]
                    ),
                )
            ]
        )
    )


def test_alias_declaration_5() -> None:
    content = """
      using alias = foo::bar;
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            using_alias=[
                UsingAlias(
                    alias="alias",
                    type=Type(
                        typename=PQName(
                            segments=[
                                NameSpecifier(name="foo"),
                                NameSpecifier(name="bar"),
                            ]
                        )
                    ),
                )
            ]
        )
    )


def test_alias_declaration_6() -> None:
    content = """
      template <typename T> using alias = foo<T>::bar;
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            using_alias=[
                UsingAlias(
                    alias="alias",
                    type=Type(
                        typename=PQName(
                            segments=[
                                NameSpecifier(
                                    name="foo",
                                    specialization=TemplateSpecialization(
                                        args=[
                                            TemplateArgument(
                                                arg=Type(
                                                    typename=PQName(
                                                        segments=[
                                                            NameSpecifier(name="T")
                                                        ]
                                                    )
                                                )
                                            )
                                        ]
                                    ),
                                ),
                                NameSpecifier(name="bar"),
                            ]
                        )
                    ),
                    template=TemplateDecl(
                        params=[TemplateTypeParam(typekey="typename", name="T")]
                    ),
                )
            ]
        )
    )


def test_using_many_things() -> None:
    content = """
      // clang-format off
      
      using std::thing;
      using MyThing = SomeThing;
      namespace a {
          using std::string;
          using VoidFunction = std::function<void()>;
      
          void fn(string &s, VoidFunction fn, thing * t);
      
          class A : public B {
          public:
              using B::B;
              using IntFunction = std::function<int()>;
      
              void a(string &s, IntFunction fn, thing * t);
          };
      }
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            using=[
                UsingDecl(
                    typename=PQName(
                        segments=[
                            NameSpecifier(name="std"),
                            NameSpecifier(name="thing"),
                        ]
                    )
                )
            ],
            using_alias=[
                UsingAlias(
                    alias="MyThing",
                    type=Type(
                        typename=PQName(segments=[NameSpecifier(name="SomeThing")])
                    ),
                )
            ],
            namespaces={
                "a": NamespaceScope(
                    name="a",
                    classes=[
                        ClassScope(
                            class_decl=ClassDecl(
                                typename=PQName(
                                    segments=[NameSpecifier(name="A")], classkey="class"
                                ),
                                bases=[
                                    BaseClass(
                                        access="public",
                                        typename=PQName(
                                            segments=[NameSpecifier(name="B")]
                                        ),
                                    )
                                ],
                            ),
                            methods=[
                                Method(
                                    return_type=Type(
                                        typename=PQName(
                                            segments=[FundamentalSpecifier(name="void")]
                                        )
                                    ),
                                    name=PQName(segments=[NameSpecifier(name="a")]),
                                    parameters=[
                                        Parameter(
                                            type=Reference(
                                                ref_to=Type(
                                                    typename=PQName(
                                                        segments=[
                                                            NameSpecifier(name="string")
                                                        ]
                                                    )
                                                )
                                            ),
                                            name="s",
                                        ),
                                        Parameter(
                                            type=Type(
                                                typename=PQName(
                                                    segments=[
                                                        NameSpecifier(
                                                            name="IntFunction"
                                                        )
                                                    ]
                                                )
                                            ),
                                            name="fn",
                                        ),
                                        Parameter(
                                            type=Pointer(
                                                ptr_to=Type(
                                                    typename=PQName(
                                                        segments=[
                                                            NameSpecifier(name="thing")
                                                        ]
                                                    )
                                                )
                                            ),
                                            name="t",
                                        ),
                                    ],
                                    access="public",
                                )
                            ],
                            using=[
                                UsingDecl(
                                    typename=PQName(
                                        segments=[
                                            NameSpecifier(name="B"),
                                            NameSpecifier(name="B"),
                                        ]
                                    ),
                                    access="public",
                                )
                            ],
                            using_alias=[
                                UsingAlias(
                                    alias="IntFunction",
                                    type=Type(
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
                                    access="public",
                                )
                            ],
                        )
                    ],
                    functions=[
                        Function(
                            return_type=Type(
                                typename=PQName(
                                    segments=[FundamentalSpecifier(name="void")]
                                )
                            ),
                            name=PQName(segments=[NameSpecifier(name="fn")]),
                            parameters=[
                                Parameter(
                                    type=Reference(
                                        ref_to=Type(
                                            typename=PQName(
                                                segments=[NameSpecifier(name="string")]
                                            )
                                        )
                                    ),
                                    name="s",
                                ),
                                Parameter(
                                    type=Type(
                                        typename=PQName(
                                            segments=[
                                                NameSpecifier(name="VoidFunction")
                                            ]
                                        )
                                    ),
                                    name="fn",
                                ),
                                Parameter(
                                    type=Pointer(
                                        ptr_to=Type(
                                            typename=PQName(
                                                segments=[NameSpecifier(name="thing")]
                                            )
                                        )
                                    ),
                                    name="t",
                                ),
                            ],
                        )
                    ],
                    using=[
                        UsingDecl(
                            typename=PQName(
                                segments=[
                                    NameSpecifier(name="std"),
                                    NameSpecifier(name="string"),
                                ]
                            )
                        )
                    ],
                    using_alias=[
                        UsingAlias(
                            alias="VoidFunction",
                            type=Type(
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
                                                            parameters=[],
                                                        )
                                                    )
                                                ]
                                            ),
                                        ),
                                    ]
                                )
                            ),
                        )
                    ],
                )
            },
        )
    )


def test_using_template_in_class() -> None:
    content = """
      class X {
        template <typename T>
        using TT = U<T>;
      };
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            classes=[
                ClassScope(
                    class_decl=ClassDecl(
                        typename=PQName(
                            segments=[NameSpecifier(name="X")], classkey="class"
                        )
                    ),
                    using_alias=[
                        UsingAlias(
                            alias="TT",
                            type=Type(
                                typename=PQName(
                                    segments=[
                                        NameSpecifier(
                                            name="U",
                                            specialization=TemplateSpecialization(
                                                args=[
                                                    TemplateArgument(
                                                        arg=Type(
                                                            typename=PQName(
                                                                segments=[
                                                                    NameSpecifier(
                                                                        name="T"
                                                                    )
                                                                ]
                                                            )
                                                        )
                                                    )
                                                ]
                                            ),
                                        )
                                    ]
                                )
                            ),
                            template=TemplateDecl(
                                params=[TemplateTypeParam(typekey="typename", name="T")]
                            ),
                            access="private",
                        )
                    ],
                )
            ]
        )
    )


def test_using_typename_in_class() -> None:
    content = """
      template <class D> class P {
      using A = typename f::TP<D>::A;
      public:
        using State = typename f::TP<D>::S;
        P(State st);
      };
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            classes=[
                ClassScope(
                    class_decl=ClassDecl(
                        typename=PQName(
                            segments=[NameSpecifier(name="P")], classkey="class"
                        ),
                        template=TemplateDecl(
                            params=[TemplateTypeParam(typekey="class", name="D")]
                        ),
                    ),
                    methods=[
                        Method(
                            return_type=None,
                            name=PQName(segments=[NameSpecifier(name="P")]),
                            parameters=[
                                Parameter(
                                    type=Type(
                                        typename=PQName(
                                            segments=[NameSpecifier(name="State")]
                                        )
                                    ),
                                    name="st",
                                )
                            ],
                            access="public",
                            constructor=True,
                        )
                    ],
                    using_alias=[
                        UsingAlias(
                            alias="A",
                            type=Type(
                                typename=PQName(
                                    segments=[
                                        NameSpecifier(name="f"),
                                        NameSpecifier(
                                            name="TP",
                                            specialization=TemplateSpecialization(
                                                args=[
                                                    TemplateArgument(
                                                        arg=Type(
                                                            typename=PQName(
                                                                segments=[
                                                                    NameSpecifier(
                                                                        name="D"
                                                                    )
                                                                ]
                                                            )
                                                        )
                                                    )
                                                ]
                                            ),
                                        ),
                                        NameSpecifier(name="A"),
                                    ],
                                    has_typename=True,
                                )
                            ),
                            access="private",
                        ),
                        UsingAlias(
                            alias="State",
                            type=Type(
                                typename=PQName(
                                    segments=[
                                        NameSpecifier(name="f"),
                                        NameSpecifier(
                                            name="TP",
                                            specialization=TemplateSpecialization(
                                                args=[
                                                    TemplateArgument(
                                                        arg=Type(
                                                            typename=PQName(
                                                                segments=[
                                                                    NameSpecifier(
                                                                        name="D"
                                                                    )
                                                                ]
                                                            )
                                                        )
                                                    )
                                                ]
                                            ),
                                        ),
                                        NameSpecifier(name="S"),
                                    ],
                                    has_typename=True,
                                )
                            ),
                            access="public",
                        ),
                    ],
                )
            ]
        )
    )


def test_using_enum_global() -> None:
    content = """
      namespace A {
      using enum B::C;
      }
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            namespaces={
                "A": NamespaceScope(
                    name="A",
                    using=[
                        UsingDecl(
                            typename=PQName(
                                segments=[
                                    NameSpecifier(name="B"),
                                    NameSpecifier(name="C"),
                                ],
                                classkey="enum",
                            )
                        )
                    ],
                )
            }
        )
    )


def test_using_enum_in_struct() -> None:
    content = """
      struct S {
        using enum fruit;
      };
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            classes=[
                ClassScope(
                    class_decl=ClassDecl(
                        typename=PQName(
                            segments=[NameSpecifier(name="S")], classkey="struct"
                        )
                    ),
                    using=[
                        UsingDecl(
                            typename=PQName(
                                segments=[NameSpecifier(name="fruit")], classkey="enum"
                            ),
                            access="public",
                        )
                    ],
                )
            ]
        )
    )
