from cxxheaderparser.simple import ClassScope, NamespaceScope, ParsedData, parse_string
from cxxheaderparser.tokfmt import Token
from cxxheaderparser.types import (
    AutoSpecifier,
    ClassDecl,
    Concept,
    Function,
    FundamentalSpecifier,
    Method,
    MoveReference,
    NameSpecifier,
    PQName,
    Parameter,
    TemplateArgument,
    TemplateDecl,
    TemplateNonTypeParam,
    TemplateSpecialization,
    TemplateTypeParam,
    Type,
    Value,
    Variable,
)


def test_concept_basic_constraint() -> None:
    content = """
      template <class T, class U>
      concept Derived = std::is_base_of<U, T>::value;

      template <Derived<Base> T> void f(T); // T is constrained by Derived<T, Base>
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            functions=[
                Function(
                    return_type=Type(
                        typename=PQName(segments=[FundamentalSpecifier(name="void")])
                    ),
                    name=PQName(segments=[NameSpecifier(name="f")]),
                    parameters=[
                        Parameter(
                            type=Type(
                                typename=PQName(segments=[NameSpecifier(name="T")])
                            )
                        )
                    ],
                    template=TemplateDecl(
                        params=[
                            TemplateNonTypeParam(
                                type=Type(
                                    typename=PQName(
                                        segments=[
                                            NameSpecifier(
                                                name="Derived",
                                                specialization=TemplateSpecialization(
                                                    args=[
                                                        TemplateArgument(
                                                            arg=Type(
                                                                typename=PQName(
                                                                    segments=[
                                                                        NameSpecifier(
                                                                            name="Base"
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
                                name="T",
                            )
                        ]
                    ),
                )
            ],
            concepts=[
                Concept(
                    template=TemplateDecl(
                        params=[
                            TemplateTypeParam(typekey="class", name="T"),
                            TemplateTypeParam(typekey="class", name="U"),
                        ]
                    ),
                    name="Derived",
                    raw_constraint=Value(
                        tokens=[
                            Token(value="std"),
                            Token(value="::"),
                            Token(value="is_base_of"),
                            Token(value="<"),
                            Token(value="U"),
                            Token(value=","),
                            Token(value="T"),
                            Token(value=">"),
                            Token(value="::"),
                            Token(value="value"),
                        ]
                    ),
                )
            ],
        )
    )


def test_concept_basic_constraint2() -> None:
    content = """
      template <class T> constexpr bool is_meowable = true;

      template <class T> constexpr bool is_cat = true;

      template <class T>
      concept Meowable = is_meowable<T>;

      template <class T>
      concept BadMeowableCat = is_meowable<T> && is_cat<T>;
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            variables=[
                Variable(
                    name=PQName(segments=[NameSpecifier(name="is_meowable")]),
                    type=Type(
                        typename=PQName(segments=[FundamentalSpecifier(name="bool")])
                    ),
                    value=Value(tokens=[Token(value="true")]),
                    constexpr=True,
                    template=TemplateDecl(
                        params=[TemplateTypeParam(typekey="class", name="T")]
                    ),
                ),
                Variable(
                    name=PQName(segments=[NameSpecifier(name="is_cat")]),
                    type=Type(
                        typename=PQName(segments=[FundamentalSpecifier(name="bool")])
                    ),
                    value=Value(tokens=[Token(value="true")]),
                    constexpr=True,
                    template=TemplateDecl(
                        params=[TemplateTypeParam(typekey="class", name="T")]
                    ),
                ),
            ],
            concepts=[
                Concept(
                    template=TemplateDecl(
                        params=[TemplateTypeParam(typekey="class", name="T")]
                    ),
                    name="Meowable",
                    raw_constraint=Value(
                        tokens=[
                            Token(value="is_meowable"),
                            Token(value="<"),
                            Token(value="T"),
                            Token(value=">"),
                        ]
                    ),
                ),
                Concept(
                    template=TemplateDecl(
                        params=[TemplateTypeParam(typekey="class", name="T")]
                    ),
                    name="BadMeowableCat",
                    raw_constraint=Value(
                        tokens=[
                            Token(value="is_meowable"),
                            Token(value="<"),
                            Token(value="T"),
                            Token(value=">"),
                            Token(value="&&"),
                            Token(value="is_cat"),
                            Token(value="<"),
                            Token(value="T"),
                            Token(value=">"),
                        ]
                    ),
                ),
            ],
        )
    )


def test_concept_basic_requires() -> None:
    content = """
      template <typename T>
      concept Hashable = requires(T a) {
        { std::hash<T>{}(a) } -> std::convertible_to<std::size_t>;
      };

      template <Hashable T> void f(T) {}
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            functions=[
                Function(
                    return_type=Type(
                        typename=PQName(segments=[FundamentalSpecifier(name="void")])
                    ),
                    name=PQName(segments=[NameSpecifier(name="f")]),
                    parameters=[
                        Parameter(
                            type=Type(
                                typename=PQName(segments=[NameSpecifier(name="T")])
                            )
                        )
                    ],
                    has_body=True,
                    template=TemplateDecl(
                        params=[
                            TemplateNonTypeParam(
                                type=Type(
                                    typename=PQName(
                                        segments=[NameSpecifier(name="Hashable")]
                                    )
                                ),
                                name="T",
                            )
                        ]
                    ),
                )
            ],
            concepts=[
                Concept(
                    template=TemplateDecl(
                        params=[TemplateTypeParam(typekey="typename", name="T")]
                    ),
                    name="Hashable",
                    raw_constraint=Value(
                        tokens=[
                            Token(value="requires"),
                            Token(value="("),
                            Token(value="T"),
                            Token(value="a"),
                            Token(value=")"),
                            Token(value="{"),
                            Token(value="{"),
                            Token(value="std"),
                            Token(value="::"),
                            Token(value="hash"),
                            Token(value="<"),
                            Token(value="T"),
                            Token(value=">"),
                            Token(value="{"),
                            Token(value="}"),
                            Token(value="("),
                            Token(value="a"),
                            Token(value=")"),
                            Token(value="}"),
                            Token(value="->"),
                            Token(value="std"),
                            Token(value="::"),
                            Token(value="convertible_to"),
                            Token(value="<"),
                            Token(value="std"),
                            Token(value="::"),
                            Token(value="size_t"),
                            Token(value=">"),
                            Token(value=";"),
                            Token(value="}"),
                        ]
                    ),
                )
            ],
        )
    )


def test_concept_nested_requirements() -> None:
    content = """
      template<class T>
      concept Semiregular = DefaultConstructible<T> &&
          CopyConstructible<T> && CopyAssignable<T> && Destructible<T> &&
      requires(T a, std::size_t n)
      {
          requires Same<T*, decltype(&a)>; // nested: "Same<...> evaluates to true"
          { a.~T() } noexcept; // compound: "a.~T()" is a valid expression that doesn't throw
          requires Same<T*, decltype(new T)>; // nested: "Same<...> evaluates to true"
          requires Same<T*, decltype(new T[n])>; // nested
          { delete new T }; // compound
          { delete new T[n] }; // compound
      };
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            concepts=[
                Concept(
                    template=TemplateDecl(
                        params=[TemplateTypeParam(typekey="class", name="T")]
                    ),
                    name="Semiregular",
                    raw_constraint=Value(
                        tokens=[
                            Token(value="DefaultConstructible"),
                            Token(value="<"),
                            Token(value="T"),
                            Token(value=">"),
                            Token(value="&&"),
                            Token(value="CopyConstructible"),
                            Token(value="<"),
                            Token(value="T"),
                            Token(value=">"),
                            Token(value="&&"),
                            Token(value="CopyAssignable"),
                            Token(value="<"),
                            Token(value="T"),
                            Token(value=">"),
                            Token(value="&&"),
                            Token(value="Destructible"),
                            Token(value="<"),
                            Token(value="T"),
                            Token(value=">"),
                            Token(value="&&"),
                            Token(value="requires"),
                            Token(value="("),
                            Token(value="T"),
                            Token(value="a"),
                            Token(value=","),
                            Token(value="std"),
                            Token(value="::"),
                            Token(value="size_t"),
                            Token(value="n"),
                            Token(value=")"),
                            Token(value="{"),
                            Token(value="requires"),
                            Token(value="Same"),
                            Token(value="<"),
                            Token(value="T"),
                            Token(value="*"),
                            Token(value=","),
                            Token(value="decltype"),
                            Token(value="("),
                            Token(value="&"),
                            Token(value="a"),
                            Token(value=")"),
                            Token(value=">"),
                            Token(value=";"),
                            Token(value="{"),
                            Token(value="a"),
                            Token(value="."),
                            Token(value="~T"),
                            Token(value="("),
                            Token(value=")"),
                            Token(value="}"),
                            Token(value="noexcept"),
                            Token(value=";"),
                            Token(value="requires"),
                            Token(value="Same"),
                            Token(value="<"),
                            Token(value="T"),
                            Token(value="*"),
                            Token(value=","),
                            Token(value="decltype"),
                            Token(value="("),
                            Token(value="new"),
                            Token(value="T"),
                            Token(value=")"),
                            Token(value=">"),
                            Token(value=";"),
                            Token(value="requires"),
                            Token(value="Same"),
                            Token(value="<"),
                            Token(value="T"),
                            Token(value="*"),
                            Token(value=","),
                            Token(value="decltype"),
                            Token(value="("),
                            Token(value="new"),
                            Token(value="T"),
                            Token(value="["),
                            Token(value="n"),
                            Token(value="]"),
                            Token(value=")"),
                            Token(value=">"),
                            Token(value=";"),
                            Token(value="{"),
                            Token(value="delete"),
                            Token(value="new"),
                            Token(value="T"),
                            Token(value="}"),
                            Token(value=";"),
                            Token(value="{"),
                            Token(value="delete"),
                            Token(value="new"),
                            Token(value="T"),
                            Token(value="["),
                            Token(value="n"),
                            Token(value="]"),
                            Token(value="}"),
                            Token(value=";"),
                            Token(value="}"),
                        ]
                    ),
                )
            ]
        )
    )


def test_concept_requires_class() -> None:
    content = """
      // clang-format off
      template <typename T>
      concept Number = std::integral<T> || std::floating_point<T>;

      template <typename T>
      requires Number<T>
      struct WrappedNumber {};
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            classes=[
                ClassScope(
                    class_decl=ClassDecl(
                        typename=PQName(
                            segments=[NameSpecifier(name="WrappedNumber")],
                            classkey="struct",
                        ),
                        template=TemplateDecl(
                            params=[TemplateTypeParam(typekey="typename", name="T")],
                            raw_requires_pre=Value(
                                tokens=[
                                    Token(value="Number"),
                                    Token(value="<"),
                                    Token(value="T"),
                                    Token(value=">"),
                                ]
                            ),
                        ),
                    )
                )
            ],
            concepts=[
                Concept(
                    template=TemplateDecl(
                        params=[TemplateTypeParam(typekey="typename", name="T")]
                    ),
                    name="Number",
                    raw_constraint=Value(
                        tokens=[
                            Token(value="std"),
                            Token(value="::"),
                            Token(value="integral"),
                            Token(value="<"),
                            Token(value="T"),
                            Token(value=">"),
                            Token(value="||"),
                            Token(value="std"),
                            Token(value="::"),
                            Token(value="floating_point"),
                            Token(value="<"),
                            Token(value="T"),
                            Token(value=">"),
                        ]
                    ),
                )
            ],
        )
    )


def test_requires_last_elem() -> None:
    content = """
      template<typename T>
      void f(T&&) requires Eq<T>;  // can appear as the last element of a function declarator
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            functions=[
                Function(
                    return_type=Type(
                        typename=PQName(segments=[FundamentalSpecifier(name="void")])
                    ),
                    name=PQName(segments=[NameSpecifier(name="f")]),
                    parameters=[
                        Parameter(
                            type=MoveReference(
                                moveref_to=Type(
                                    typename=PQName(segments=[NameSpecifier(name="T")])
                                )
                            )
                        )
                    ],
                    template=TemplateDecl(
                        params=[TemplateTypeParam(typekey="typename", name="T")]
                    ),
                    raw_requires=Value(
                        tokens=[
                            Token(value="Eq"),
                            Token(value="<"),
                            Token(value="T"),
                            Token(value=">"),
                        ]
                    ),
                )
            ]
        )
    )


def test_requires_first_elem1() -> None:
    content = """
      template<typename T> requires Addable<T> // or right after a template parameter list
      T add(T a, T b) { return a + b; }
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            functions=[
                Function(
                    return_type=Type(
                        typename=PQName(segments=[NameSpecifier(name="T")])
                    ),
                    name=PQName(segments=[NameSpecifier(name="add")]),
                    parameters=[
                        Parameter(
                            type=Type(
                                typename=PQName(segments=[NameSpecifier(name="T")])
                            ),
                            name="a",
                        ),
                        Parameter(
                            type=Type(
                                typename=PQName(segments=[NameSpecifier(name="T")])
                            ),
                            name="b",
                        ),
                    ],
                    has_body=True,
                    template=TemplateDecl(
                        params=[TemplateTypeParam(typekey="typename", name="T")],
                        raw_requires_pre=Value(
                            tokens=[
                                Token(value="Addable"),
                                Token(value="<"),
                                Token(value="T"),
                                Token(value=">"),
                            ]
                        ),
                    ),
                )
            ]
        )
    )


def test_requires_first_elem2() -> None:
    content = """
      template<typename T> requires std::is_arithmetic_v<T>
      T add(T a, T b) { return a + b; }
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            functions=[
                Function(
                    return_type=Type(
                        typename=PQName(segments=[NameSpecifier(name="T")])
                    ),
                    name=PQName(segments=[NameSpecifier(name="add")]),
                    parameters=[
                        Parameter(
                            type=Type(
                                typename=PQName(segments=[NameSpecifier(name="T")])
                            ),
                            name="a",
                        ),
                        Parameter(
                            type=Type(
                                typename=PQName(segments=[NameSpecifier(name="T")])
                            ),
                            name="b",
                        ),
                    ],
                    has_body=True,
                    template=TemplateDecl(
                        params=[TemplateTypeParam(typekey="typename", name="T")],
                        raw_requires_pre=Value(
                            tokens=[
                                Token(value="std"),
                                Token(value="is_arithmetic_v"),
                                Token(value="<"),
                                Token(value="T"),
                                Token(value=">"),
                            ]
                        ),
                    ),
                )
            ]
        )
    )


def test_requires_compound() -> None:
    content = """
      template<typename T> requires Addable<T> || Subtractable<T>
      T add(T a, T b) { return a + b; }
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            functions=[
                Function(
                    return_type=Type(
                        typename=PQName(segments=[NameSpecifier(name="T")])
                    ),
                    name=PQName(segments=[NameSpecifier(name="add")]),
                    parameters=[
                        Parameter(
                            type=Type(
                                typename=PQName(segments=[NameSpecifier(name="T")])
                            ),
                            name="a",
                        ),
                        Parameter(
                            type=Type(
                                typename=PQName(segments=[NameSpecifier(name="T")])
                            ),
                            name="b",
                        ),
                    ],
                    has_body=True,
                    template=TemplateDecl(
                        params=[TemplateTypeParam(typekey="typename", name="T")],
                        raw_requires_pre=Value(
                            tokens=[
                                Token(value="Addable"),
                                Token(value="<"),
                                Token(value="T"),
                                Token(value=">"),
                                Token(value="||"),
                                Token(value="Subtractable"),
                                Token(value="<"),
                                Token(value="T"),
                                Token(value=">"),
                            ]
                        ),
                    ),
                )
            ]
        )
    )


def test_requires_ad_hoc() -> None:
    content = """
      template<typename T>
          requires requires (T x) { x + x; } // ad-hoc constraint, note keyword used twice
      T add(T a, T b) { return a + b; }
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            functions=[
                Function(
                    return_type=Type(
                        typename=PQName(segments=[NameSpecifier(name="T")])
                    ),
                    name=PQName(segments=[NameSpecifier(name="add")]),
                    parameters=[
                        Parameter(
                            type=Type(
                                typename=PQName(segments=[NameSpecifier(name="T")])
                            ),
                            name="a",
                        ),
                        Parameter(
                            type=Type(
                                typename=PQName(segments=[NameSpecifier(name="T")])
                            ),
                            name="b",
                        ),
                    ],
                    has_body=True,
                    template=TemplateDecl(
                        params=[TemplateTypeParam(typekey="typename", name="T")],
                        raw_requires_pre=Value(
                            tokens=[
                                Token(value="requires"),
                                Token(value="("),
                                Token(value="T"),
                                Token(value="x"),
                                Token(value=")"),
                                Token(value="{"),
                                Token(value="x"),
                                Token(value="+"),
                                Token(value="x"),
                                Token(value=";"),
                                Token(value="}"),
                            ]
                        ),
                    ),
                )
            ]
        )
    )


def test_requires_both() -> None:
    content = """
      // clang-format off
      template<typename T>
      requires Addable<T>
      auto f1(T a, T b) requires Subtractable<T>;
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            functions=[
                Function(
                    return_type=Type(typename=PQName(segments=[AutoSpecifier()])),
                    name=PQName(segments=[NameSpecifier(name="f1")]),
                    parameters=[
                        Parameter(
                            type=Type(
                                typename=PQName(segments=[NameSpecifier(name="T")])
                            ),
                            name="a",
                        ),
                        Parameter(
                            type=Type(
                                typename=PQName(segments=[NameSpecifier(name="T")])
                            ),
                            name="b",
                        ),
                    ],
                    template=TemplateDecl(
                        params=[TemplateTypeParam(typekey="typename", name="T")],
                        raw_requires_pre=Value(
                            tokens=[
                                Token(value="Addable"),
                                Token(value="<"),
                                Token(value="T"),
                                Token(value=">"),
                            ]
                        ),
                    ),
                    raw_requires=Value(
                        tokens=[
                            Token(value="Subtractable"),
                            Token(value="<"),
                            Token(value="T"),
                            Token(value=">"),
                        ]
                    ),
                )
            ]
        )
    )


def test_requires_paren() -> None:
    content = """
      // clang-format off
      template<class T>
      void h(T) requires (is_purrable<T>());
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            functions=[
                Function(
                    return_type=Type(
                        typename=PQName(segments=[FundamentalSpecifier(name="void")])
                    ),
                    name=PQName(segments=[NameSpecifier(name="h")]),
                    parameters=[
                        Parameter(
                            type=Type(
                                typename=PQName(segments=[NameSpecifier(name="T")])
                            )
                        )
                    ],
                    template=TemplateDecl(
                        params=[TemplateTypeParam(typekey="class", name="T")]
                    ),
                    raw_requires=Value(
                        tokens=[
                            Token(value="("),
                            Token(value="is_purrable"),
                            Token(value="<"),
                            Token(value="T"),
                            Token(value=">"),
                            Token(value="("),
                            Token(value=")"),
                            Token(value=")"),
                        ]
                    ),
                )
            ]
        )
    )


def test_non_template_requires() -> None:
    content = """
      // clang-format off

      template <class T>
      struct Payload
      {
          constexpr Payload(T v)
              requires(std::is_pod_v<T>)
              : Value(v)
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
                            segments=[NameSpecifier(name="Payload")], classkey="struct"
                        ),
                        template=TemplateDecl(
                            params=[TemplateTypeParam(typekey="class", name="T")]
                        ),
                    ),
                    methods=[
                        Method(
                            return_type=None,
                            name=PQName(segments=[NameSpecifier(name="Payload")]),
                            parameters=[
                                Parameter(
                                    type=Type(
                                        typename=PQName(
                                            segments=[NameSpecifier(name="T")]
                                        )
                                    ),
                                    name="v",
                                )
                            ],
                            constexpr=True,
                            has_body=True,
                            raw_requires=Value(
                                tokens=[
                                    Token(value="("),
                                    Token(value="std"),
                                    Token(value="::"),
                                    Token(value="is_pod_v"),
                                    Token(value="<"),
                                    Token(value="T"),
                                    Token(value=">"),
                                    Token(value=")"),
                                ]
                            ),
                            access="public",
                            constructor=True,
                        )
                    ],
                )
            ]
        )
    )
