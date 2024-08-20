# Note: testcases generated via `python -m cxxheaderparser.gentest`

from cxxheaderparser.types import (
    AnonymousName,
    Array,
    BaseClass,
    ClassDecl,
    EnumDecl,
    Enumerator,
    Field,
    ForwardDecl,
    Function,
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
    UsingDecl,
    Value,
    Variable,
)
from cxxheaderparser.simple import (
    ClassScope,
    NamespaceScope,
    parse_string,
    ParsedData,
)


def test_class_member_spec_1() -> None:
    content = """
      class S {
        int d1;                   // non-static data member
        int a[10] = {1, 2};       // non-static data member with initializer (C++11)
        static const int d2 = 1;  // static data member with initializer
        virtual void f1(int) = 0; // pure virtual member function
        std::string d3, *d4, f2(int); // two data members and a member function
        enum { NORTH, SOUTH, EAST, WEST };
        struct NestedS {
          std::string s;
        } d5, *d6;
        typedef NestedS value_type, *pointer_type;
      };
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            classes=[
                ClassScope(
                    class_decl=ClassDecl(
                        typename=PQName(
                            segments=[NameSpecifier(name="S")], classkey="class"
                        )
                    ),
                    classes=[
                        ClassScope(
                            class_decl=ClassDecl(
                                typename=PQName(
                                    segments=[NameSpecifier(name="NestedS")],
                                    classkey="struct",
                                ),
                                access="private",
                            ),
                            fields=[
                                Field(
                                    name="s",
                                    type=Type(
                                        typename=PQName(
                                            segments=[
                                                NameSpecifier(name="std"),
                                                NameSpecifier(name="string"),
                                            ]
                                        )
                                    ),
                                    access="public",
                                )
                            ],
                        )
                    ],
                    enums=[
                        EnumDecl(
                            typename=PQName(
                                segments=[AnonymousName(id=1)], classkey="enum"
                            ),
                            values=[
                                Enumerator(name="NORTH"),
                                Enumerator(name="SOUTH"),
                                Enumerator(name="EAST"),
                                Enumerator(name="WEST"),
                            ],
                            access="private",
                        )
                    ],
                    fields=[
                        Field(
                            name="d1",
                            type=Type(
                                typename=PQName(
                                    segments=[FundamentalSpecifier(name="int")]
                                )
                            ),
                            access="private",
                        ),
                        Field(
                            name="a",
                            type=Array(
                                array_of=Type(
                                    typename=PQName(
                                        segments=[FundamentalSpecifier(name="int")]
                                    )
                                ),
                                size=Value(tokens=[Token(value="10")]),
                            ),
                            access="private",
                            value=Value(
                                tokens=[
                                    Token(value="{"),
                                    Token(value="1"),
                                    Token(value=","),
                                    Token(value="2"),
                                    Token(value="}"),
                                ]
                            ),
                        ),
                        Field(
                            name="d2",
                            type=Type(
                                typename=PQName(
                                    segments=[FundamentalSpecifier(name="int")]
                                ),
                                const=True,
                            ),
                            access="private",
                            value=Value(tokens=[Token(value="1")]),
                            static=True,
                        ),
                        Field(
                            name="d3",
                            type=Type(
                                typename=PQName(
                                    segments=[
                                        NameSpecifier(name="std"),
                                        NameSpecifier(name="string"),
                                    ]
                                )
                            ),
                            access="private",
                        ),
                        Field(
                            name="d4",
                            type=Pointer(
                                ptr_to=Type(
                                    typename=PQName(
                                        segments=[
                                            NameSpecifier(name="std"),
                                            NameSpecifier(name="string"),
                                        ]
                                    )
                                )
                            ),
                            access="private",
                        ),
                        Field(
                            name="d5",
                            type=Type(
                                typename=PQName(
                                    segments=[NameSpecifier(name="NestedS")],
                                    classkey="struct",
                                )
                            ),
                            access="private",
                        ),
                        Field(
                            name="d6",
                            type=Pointer(
                                ptr_to=Type(
                                    typename=PQName(
                                        segments=[NameSpecifier(name="NestedS")],
                                        classkey="struct",
                                    )
                                )
                            ),
                            access="private",
                        ),
                    ],
                    methods=[
                        Method(
                            return_type=Type(
                                typename=PQName(
                                    segments=[FundamentalSpecifier(name="void")]
                                )
                            ),
                            name=PQName(segments=[NameSpecifier(name="f1")]),
                            parameters=[
                                Parameter(
                                    type=Type(
                                        typename=PQName(
                                            segments=[FundamentalSpecifier(name="int")]
                                        )
                                    )
                                )
                            ],
                            access="private",
                            pure_virtual=True,
                            virtual=True,
                        ),
                        Method(
                            return_type=Type(
                                typename=PQName(
                                    segments=[
                                        NameSpecifier(name="std"),
                                        NameSpecifier(name="string"),
                                    ]
                                )
                            ),
                            name=PQName(segments=[NameSpecifier(name="f2")]),
                            parameters=[
                                Parameter(
                                    type=Type(
                                        typename=PQName(
                                            segments=[FundamentalSpecifier(name="int")]
                                        )
                                    )
                                )
                            ],
                            access="private",
                        ),
                    ],
                    typedefs=[
                        Typedef(
                            type=Type(
                                typename=PQName(
                                    segments=[NameSpecifier(name="NestedS")]
                                )
                            ),
                            name="value_type",
                            access="private",
                        ),
                        Typedef(
                            type=Pointer(
                                ptr_to=Type(
                                    typename=PQName(
                                        segments=[NameSpecifier(name="NestedS")]
                                    )
                                )
                            ),
                            name="pointer_type",
                            access="private",
                        ),
                    ],
                )
            ]
        )
    )


def test_class_member_spec_2() -> None:
    content = """
      class M {
        std::size_t C;
        std::vector<int> data;
      
      public:
        M(std::size_t R, std::size_t C)
            : C(C), data(R * C) {}                 // constructor definition
        int operator()(size_t r, size_t c) const { // member function definition
          return data[r * C + c];
        }
        int &operator()(size_t r, size_t c) { // another member function definition
          return data[r * C + c];
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
                            segments=[NameSpecifier(name="M")], classkey="class"
                        )
                    ),
                    fields=[
                        Field(
                            access="private",
                            type=Type(
                                typename=PQName(
                                    segments=[
                                        NameSpecifier(name="std"),
                                        NameSpecifier(name="size_t"),
                                    ]
                                )
                            ),
                            name="C",
                        ),
                        Field(
                            access="private",
                            type=Type(
                                typename=PQName(
                                    segments=[
                                        NameSpecifier(name="std"),
                                        NameSpecifier(
                                            name="vector",
                                            specialization=TemplateSpecialization(
                                                args=[
                                                    TemplateArgument(
                                                        arg=Type(
                                                            typename=PQName(
                                                                segments=[
                                                                    FundamentalSpecifier(
                                                                        name="int"
                                                                    )
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
                            name="data",
                        ),
                    ],
                    methods=[
                        Method(
                            return_type=None,
                            name=PQName(segments=[NameSpecifier(name="M")]),
                            parameters=[
                                Parameter(
                                    type=Type(
                                        typename=PQName(
                                            segments=[
                                                NameSpecifier(name="std"),
                                                NameSpecifier(name="size_t"),
                                            ]
                                        )
                                    ),
                                    name="R",
                                ),
                                Parameter(
                                    type=Type(
                                        typename=PQName(
                                            segments=[
                                                NameSpecifier(name="std"),
                                                NameSpecifier(name="size_t"),
                                            ]
                                        )
                                    ),
                                    name="C",
                                ),
                            ],
                            has_body=True,
                            access="public",
                            constructor=True,
                        ),
                        Method(
                            return_type=Type(
                                typename=PQName(
                                    segments=[FundamentalSpecifier(name="int")]
                                )
                            ),
                            name=PQName(segments=[NameSpecifier(name="operator()")]),
                            parameters=[
                                Parameter(
                                    type=Type(
                                        typename=PQName(
                                            segments=[NameSpecifier(name="size_t")]
                                        )
                                    ),
                                    name="r",
                                ),
                                Parameter(
                                    type=Type(
                                        typename=PQName(
                                            segments=[NameSpecifier(name="size_t")]
                                        )
                                    ),
                                    name="c",
                                ),
                            ],
                            has_body=True,
                            access="public",
                            const=True,
                            operator="()",
                        ),
                        Method(
                            return_type=Reference(
                                ref_to=Type(
                                    typename=PQName(
                                        segments=[FundamentalSpecifier(name="int")]
                                    )
                                )
                            ),
                            name=PQName(segments=[NameSpecifier(name="operator()")]),
                            parameters=[
                                Parameter(
                                    type=Type(
                                        typename=PQName(
                                            segments=[NameSpecifier(name="size_t")]
                                        )
                                    ),
                                    name="r",
                                ),
                                Parameter(
                                    type=Type(
                                        typename=PQName(
                                            segments=[NameSpecifier(name="size_t")]
                                        )
                                    ),
                                    name="c",
                                ),
                            ],
                            has_body=True,
                            access="public",
                            operator="()",
                        ),
                    ],
                )
            ]
        )
    )


def test_class_member_spec_3() -> None:
    content = """
      class S {
      public:
        S();          // public constructor
        S(const S &); // public copy constructor
        virtual ~S(); // public virtual destructor
      private:
        int *ptr; // private data member
      };
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            classes=[
                ClassScope(
                    class_decl=ClassDecl(
                        typename=PQName(
                            segments=[NameSpecifier(name="S")], classkey="class"
                        )
                    ),
                    fields=[
                        Field(
                            name="ptr",
                            type=Pointer(
                                ptr_to=Type(
                                    typename=PQName(
                                        segments=[FundamentalSpecifier(name="int")]
                                    )
                                )
                            ),
                            access="private",
                        )
                    ],
                    methods=[
                        Method(
                            return_type=None,
                            name=PQName(segments=[NameSpecifier(name="S")]),
                            parameters=[],
                            access="public",
                            constructor=True,
                        ),
                        Method(
                            return_type=None,
                            name=PQName(segments=[NameSpecifier(name="S")]),
                            parameters=[
                                Parameter(
                                    type=Reference(
                                        ref_to=Type(
                                            typename=PQName(
                                                segments=[NameSpecifier(name="S")]
                                            ),
                                            const=True,
                                        )
                                    )
                                )
                            ],
                            access="public",
                            constructor=True,
                        ),
                        Method(
                            return_type=None,
                            name=PQName(segments=[NameSpecifier(name="~S")]),
                            parameters=[],
                            access="public",
                            destructor=True,
                            virtual=True,
                        ),
                    ],
                )
            ]
        )
    )


def test_class_using() -> None:
    content = """
      class Base {
      protected:
        int d;
      };
      class Derived : public Base {
      public:
        using Base::Base; // inherit all parent's constructors (C++11)
        using Base::d;    // make Base's protected member d a public member of Derived
      };
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            classes=[
                ClassScope(
                    class_decl=ClassDecl(
                        typename=PQName(
                            segments=[NameSpecifier(name="Base")], classkey="class"
                        )
                    ),
                    fields=[
                        Field(
                            name="d",
                            type=Type(
                                typename=PQName(
                                    segments=[FundamentalSpecifier(name="int")]
                                )
                            ),
                            access="protected",
                        )
                    ],
                ),
                ClassScope(
                    class_decl=ClassDecl(
                        typename=PQName(
                            segments=[NameSpecifier(name="Derived")], classkey="class"
                        ),
                        bases=[
                            BaseClass(
                                access="public",
                                typename=PQName(segments=[NameSpecifier(name="Base")]),
                            )
                        ],
                    ),
                    using=[
                        UsingDecl(
                            typename=PQName(
                                segments=[
                                    NameSpecifier(name="Base"),
                                    NameSpecifier(name="Base"),
                                ]
                            ),
                            access="public",
                        ),
                        UsingDecl(
                            typename=PQName(
                                segments=[
                                    NameSpecifier(name="Base"),
                                    NameSpecifier(name="d"),
                                ]
                            ),
                            access="public",
                        ),
                    ],
                ),
            ]
        )
    )


def test_class_member_spec_6() -> None:
    content = """
      struct S {
        template<typename T>
        void f(T&& n);
 
        template<class CharT>
        struct NestedS {
          std::basic_string<CharT> s;
        };
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
                    classes=[
                        ClassScope(
                            class_decl=ClassDecl(
                                typename=PQName(
                                    segments=[NameSpecifier(name="NestedS")],
                                    classkey="struct",
                                ),
                                template=TemplateDecl(
                                    params=[
                                        TemplateTypeParam(typekey="class", name="CharT")
                                    ]
                                ),
                                access="public",
                            ),
                            fields=[
                                Field(
                                    access="public",
                                    type=Type(
                                        typename=PQName(
                                            segments=[
                                                NameSpecifier(name="std"),
                                                NameSpecifier(
                                                    name="basic_string",
                                                    specialization=TemplateSpecialization(
                                                        args=[
                                                            TemplateArgument(
                                                                arg=Type(
                                                                    typename=PQName(
                                                                        segments=[
                                                                            NameSpecifier(
                                                                                name="CharT"
                                                                            )
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
                                    name="s",
                                )
                            ],
                        )
                    ],
                    methods=[
                        Method(
                            return_type=Type(
                                typename=PQName(
                                    segments=[FundamentalSpecifier(name="void")]
                                )
                            ),
                            name=PQName(segments=[NameSpecifier(name="f")]),
                            parameters=[
                                Parameter(
                                    type=MoveReference(
                                        moveref_to=Type(
                                            typename=PQName(
                                                segments=[NameSpecifier(name="T")]
                                            )
                                        )
                                    ),
                                    name="n",
                                )
                            ],
                            template=TemplateDecl(
                                params=[TemplateTypeParam(typekey="typename", name="T")]
                            ),
                            access="public",
                        )
                    ],
                )
            ]
        )
    )


def test_class_fn_default_params() -> None:
    content = """
      // clang-format off
      class Hen
      {
      public:
        void add(int a=100, b=0xfd, float c=1.7e-3, float d=3.14);
        void join(string s1="", string s2="nothing");
      };
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            classes=[
                ClassScope(
                    class_decl=ClassDecl(
                        typename=PQName(
                            segments=[NameSpecifier(name="Hen")], classkey="class"
                        )
                    ),
                    methods=[
                        Method(
                            return_type=Type(
                                typename=PQName(
                                    segments=[FundamentalSpecifier(name="void")]
                                )
                            ),
                            name=PQName(segments=[NameSpecifier(name="add")]),
                            parameters=[
                                Parameter(
                                    type=Type(
                                        typename=PQName(
                                            segments=[FundamentalSpecifier(name="int")]
                                        )
                                    ),
                                    name="a",
                                    default=Value(tokens=[Token(value="100")]),
                                ),
                                Parameter(
                                    type=Type(
                                        typename=PQName(
                                            segments=[NameSpecifier(name="b")]
                                        )
                                    ),
                                    default=Value(tokens=[Token(value="0xfd")]),
                                ),
                                Parameter(
                                    type=Type(
                                        typename=PQName(
                                            segments=[
                                                FundamentalSpecifier(name="float")
                                            ]
                                        )
                                    ),
                                    name="c",
                                    default=Value(tokens=[Token(value="1.7e-3")]),
                                ),
                                Parameter(
                                    type=Type(
                                        typename=PQName(
                                            segments=[
                                                FundamentalSpecifier(name="float")
                                            ]
                                        )
                                    ),
                                    name="d",
                                    default=Value(tokens=[Token(value="3.14")]),
                                ),
                            ],
                            access="public",
                        ),
                        Method(
                            return_type=Type(
                                typename=PQName(
                                    segments=[FundamentalSpecifier(name="void")]
                                )
                            ),
                            name=PQName(segments=[NameSpecifier(name="join")]),
                            parameters=[
                                Parameter(
                                    type=Type(
                                        typename=PQName(
                                            segments=[NameSpecifier(name="string")]
                                        )
                                    ),
                                    name="s1",
                                    default=Value(tokens=[Token(value='""')]),
                                ),
                                Parameter(
                                    type=Type(
                                        typename=PQName(
                                            segments=[NameSpecifier(name="string")]
                                        )
                                    ),
                                    name="s2",
                                    default=Value(tokens=[Token(value='"nothing"')]),
                                ),
                            ],
                            access="public",
                        ),
                    ],
                )
            ]
        )
    )


def test_class_fn_inline_virtual() -> None:
    content = """
      class B {
      public:
        virtual inline int aMethod();
      };
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            classes=[
                ClassScope(
                    class_decl=ClassDecl(
                        typename=PQName(
                            segments=[NameSpecifier(name="B")], classkey="class"
                        )
                    ),
                    methods=[
                        Method(
                            return_type=Type(
                                typename=PQName(
                                    segments=[FundamentalSpecifier(name="int")]
                                )
                            ),
                            name=PQName(segments=[NameSpecifier(name="aMethod")]),
                            parameters=[],
                            inline=True,
                            access="public",
                            virtual=True,
                        )
                    ],
                )
            ]
        )
    )


def test_class_fn_pure_virtual_const() -> None:
    content = """
      class StoneClass {
        virtual int getNum2() const = 0;
        int getNum3();
      };
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            classes=[
                ClassScope(
                    class_decl=ClassDecl(
                        typename=PQName(
                            segments=[NameSpecifier(name="StoneClass")],
                            classkey="class",
                        )
                    ),
                    methods=[
                        Method(
                            return_type=Type(
                                typename=PQName(
                                    segments=[FundamentalSpecifier(name="int")]
                                )
                            ),
                            name=PQName(segments=[NameSpecifier(name="getNum2")]),
                            parameters=[],
                            access="private",
                            const=True,
                            pure_virtual=True,
                            virtual=True,
                        ),
                        Method(
                            return_type=Type(
                                typename=PQName(
                                    segments=[FundamentalSpecifier(name="int")]
                                )
                            ),
                            name=PQName(segments=[NameSpecifier(name="getNum3")]),
                            parameters=[],
                            access="private",
                        ),
                    ],
                )
            ]
        )
    )


def test_class_fn_return_global_ns() -> None:
    content = """
      struct Avacado {
        uint8_t foo() { return 4; }
        ::uint8_t bar() { return 0; }
      };
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            classes=[
                ClassScope(
                    class_decl=ClassDecl(
                        typename=PQName(
                            segments=[NameSpecifier(name="Avacado")], classkey="struct"
                        )
                    ),
                    methods=[
                        Method(
                            return_type=Type(
                                typename=PQName(
                                    segments=[NameSpecifier(name="uint8_t")]
                                )
                            ),
                            name=PQName(segments=[NameSpecifier(name="foo")]),
                            parameters=[],
                            has_body=True,
                            access="public",
                        ),
                        Method(
                            return_type=Type(
                                typename=PQName(
                                    segments=[
                                        NameSpecifier(name=""),
                                        NameSpecifier(name="uint8_t"),
                                    ]
                                )
                            ),
                            name=PQName(segments=[NameSpecifier(name="bar")]),
                            parameters=[],
                            has_body=True,
                            access="public",
                        ),
                    ],
                )
            ]
        )
    )


def test_class_ns_class() -> None:
    content = """
      namespace ns {
        class N;
      };
      
      class ns::N {};
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            classes=[
                ClassScope(
                    class_decl=ClassDecl(
                        typename=PQName(
                            segments=[
                                NameSpecifier(name="ns"),
                                NameSpecifier(name="N"),
                            ],
                            classkey="class",
                        )
                    )
                )
            ],
            namespaces={
                "ns": NamespaceScope(
                    name="ns",
                    forward_decls=[
                        ForwardDecl(
                            typename=PQName(
                                segments=[NameSpecifier(name="N")], classkey="class"
                            )
                        )
                    ],
                )
            },
        )
    )


def test_class_ns_w_base() -> None:
    content = """
      class Herb::Cilantro : public Plant {};
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            classes=[
                ClassScope(
                    class_decl=ClassDecl(
                        typename=PQName(
                            segments=[
                                NameSpecifier(name="Herb"),
                                NameSpecifier(name="Cilantro"),
                            ],
                            classkey="class",
                        ),
                        bases=[
                            BaseClass(
                                access="public",
                                typename=PQName(segments=[NameSpecifier(name="Plant")]),
                            )
                        ],
                    )
                )
            ]
        )
    )


def test_class_inner_class() -> None:
    content = """
      class C {
        class Inner {};
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
                    classes=[
                        ClassScope(
                            class_decl=ClassDecl(
                                typename=PQName(
                                    segments=[NameSpecifier(name="Inner")],
                                    classkey="class",
                                ),
                                access="private",
                            )
                        )
                    ],
                )
            ]
        )
    )


def test_class_inner_fwd_class() -> None:
    content = """
      class C {
        class N;
      };
      
      class C::N {};
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
                    forward_decls=[
                        ForwardDecl(
                            typename=PQName(
                                segments=[NameSpecifier(name="N")], classkey="class"
                            ),
                            access="private",
                        )
                    ],
                ),
                ClassScope(
                    class_decl=ClassDecl(
                        typename=PQName(
                            segments=[NameSpecifier(name="C"), NameSpecifier(name="N")],
                            classkey="class",
                        )
                    )
                ),
            ]
        )
    )


def test_class_inner_var_access() -> None:
    content = """
      class Bug_3488053 {
      public:
        class Bug_3488053_Nested {
        public:
          int x;
        };
      };
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            classes=[
                ClassScope(
                    class_decl=ClassDecl(
                        typename=PQName(
                            segments=[NameSpecifier(name="Bug_3488053")],
                            classkey="class",
                        )
                    ),
                    classes=[
                        ClassScope(
                            class_decl=ClassDecl(
                                typename=PQName(
                                    segments=[NameSpecifier(name="Bug_3488053_Nested")],
                                    classkey="class",
                                ),
                                access="public",
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
                        )
                    ],
                )
            ]
        )
    )


def test_class_ns_and_inner() -> None:
    content = """
      namespace RoosterNamespace {
      class RoosterOuterClass {
      public:
        int member1;
      
        class RoosterSubClass1 {
        public:
          int publicMember1;
      
        private:
          int privateMember1;
        };
      
      private:
        int member2;
        class RoosterSubClass2 {
        public:
          int publicMember2;
      
        private:
          int privateMember2;
        };
      };
      } // namespace RoosterNamespace
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            namespaces={
                "RoosterNamespace": NamespaceScope(
                    name="RoosterNamespace",
                    classes=[
                        ClassScope(
                            class_decl=ClassDecl(
                                typename=PQName(
                                    segments=[NameSpecifier(name="RoosterOuterClass")],
                                    classkey="class",
                                )
                            ),
                            classes=[
                                ClassScope(
                                    class_decl=ClassDecl(
                                        typename=PQName(
                                            segments=[
                                                NameSpecifier(name="RoosterSubClass1")
                                            ],
                                            classkey="class",
                                        ),
                                        access="public",
                                    ),
                                    fields=[
                                        Field(
                                            access="public",
                                            type=Type(
                                                typename=PQName(
                                                    segments=[
                                                        FundamentalSpecifier(name="int")
                                                    ]
                                                )
                                            ),
                                            name="publicMember1",
                                        ),
                                        Field(
                                            access="private",
                                            type=Type(
                                                typename=PQName(
                                                    segments=[
                                                        FundamentalSpecifier(name="int")
                                                    ]
                                                )
                                            ),
                                            name="privateMember1",
                                        ),
                                    ],
                                ),
                                ClassScope(
                                    class_decl=ClassDecl(
                                        typename=PQName(
                                            segments=[
                                                NameSpecifier(name="RoosterSubClass2")
                                            ],
                                            classkey="class",
                                        ),
                                        access="private",
                                    ),
                                    fields=[
                                        Field(
                                            access="public",
                                            type=Type(
                                                typename=PQName(
                                                    segments=[
                                                        FundamentalSpecifier(name="int")
                                                    ]
                                                )
                                            ),
                                            name="publicMember2",
                                        ),
                                        Field(
                                            access="private",
                                            type=Type(
                                                typename=PQName(
                                                    segments=[
                                                        FundamentalSpecifier(name="int")
                                                    ]
                                                )
                                            ),
                                            name="privateMember2",
                                        ),
                                    ],
                                ),
                            ],
                            fields=[
                                Field(
                                    access="public",
                                    type=Type(
                                        typename=PQName(
                                            segments=[FundamentalSpecifier(name="int")]
                                        )
                                    ),
                                    name="member1",
                                ),
                                Field(
                                    access="private",
                                    type=Type(
                                        typename=PQName(
                                            segments=[FundamentalSpecifier(name="int")]
                                        )
                                    ),
                                    name="member2",
                                ),
                            ],
                        )
                    ],
                )
            }
        )
    )


def test_class_struct_access() -> None:
    content = """
      struct SampleStruct {
        unsigned int meth();
      
      private:
        int prop;
      };
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            classes=[
                ClassScope(
                    class_decl=ClassDecl(
                        typename=PQName(
                            segments=[NameSpecifier(name="SampleStruct")],
                            classkey="struct",
                        )
                    ),
                    fields=[
                        Field(
                            access="private",
                            type=Type(
                                typename=PQName(
                                    segments=[FundamentalSpecifier(name="int")]
                                )
                            ),
                            name="prop",
                        )
                    ],
                    methods=[
                        Method(
                            return_type=Type(
                                typename=PQName(
                                    segments=[FundamentalSpecifier(name="unsigned int")]
                                )
                            ),
                            name=PQName(segments=[NameSpecifier(name="meth")]),
                            parameters=[],
                            access="public",
                        )
                    ],
                )
            ]
        )
    )


def test_class_volatile_move_deleted_fn() -> None:
    content = """
      struct C {
        void foo() volatile && = delete;
      };
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            classes=[
                ClassScope(
                    class_decl=ClassDecl(
                        typename=PQName(
                            segments=[NameSpecifier(name="C")], classkey="struct"
                        )
                    ),
                    methods=[
                        Method(
                            return_type=Type(
                                typename=PQName(
                                    segments=[FundamentalSpecifier(name="void")]
                                )
                            ),
                            name=PQName(segments=[NameSpecifier(name="foo")]),
                            parameters=[],
                            access="public",
                            volatile=True,
                            ref_qualifier="&&",
                            deleted=True,
                        )
                    ],
                )
            ]
        )
    )


def test_class_bitfield_1() -> None:
    content = """
      struct S {
        // will usually occupy 2 bytes:
        // 3 bits: value of b1
        // 2 bits: unused
        // 6 bits: value of b2
        // 2 bits: value of b3
        // 3 bits: unused
        unsigned char b1 : 3, : 2, b2 : 6, b3 : 2;
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
                    fields=[
                        Field(
                            name="b1",
                            type=Type(
                                typename=PQName(
                                    segments=[
                                        FundamentalSpecifier(name="unsigned char")
                                    ]
                                )
                            ),
                            access="public",
                            bits=3,
                        ),
                        Field(
                            type=Type(
                                typename=PQName(
                                    segments=[
                                        FundamentalSpecifier(name="unsigned char")
                                    ]
                                )
                            ),
                            access="public",
                            bits=2,
                        ),
                        Field(
                            name="b2",
                            type=Type(
                                typename=PQName(
                                    segments=[
                                        FundamentalSpecifier(name="unsigned char")
                                    ]
                                )
                            ),
                            access="public",
                            bits=6,
                        ),
                        Field(
                            name="b3",
                            type=Type(
                                typename=PQName(
                                    segments=[
                                        FundamentalSpecifier(name="unsigned char")
                                    ]
                                )
                            ),
                            access="public",
                            bits=2,
                        ),
                    ],
                )
            ]
        )
    )


def test_class_bitfield_2() -> None:
    content = """
      struct HAL_ControlWord {
        int x : 1;
        int y : 1;
      };
      typedef struct HAL_ControlWord HAL_ControlWord;
      int HAL_GetControlWord(HAL_ControlWord *controlWord);
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            classes=[
                ClassScope(
                    class_decl=ClassDecl(
                        typename=PQName(
                            segments=[NameSpecifier(name="HAL_ControlWord")],
                            classkey="struct",
                        )
                    ),
                    fields=[
                        Field(
                            name="x",
                            type=Type(
                                typename=PQName(
                                    segments=[FundamentalSpecifier(name="int")]
                                )
                            ),
                            access="public",
                            bits=1,
                        ),
                        Field(
                            name="y",
                            type=Type(
                                typename=PQName(
                                    segments=[FundamentalSpecifier(name="int")]
                                )
                            ),
                            access="public",
                            bits=1,
                        ),
                    ],
                )
            ],
            functions=[
                Function(
                    return_type=Type(
                        typename=PQName(segments=[FundamentalSpecifier(name="int")])
                    ),
                    name=PQName(segments=[NameSpecifier(name="HAL_GetControlWord")]),
                    parameters=[
                        Parameter(
                            type=Pointer(
                                ptr_to=Type(
                                    typename=PQName(
                                        segments=[NameSpecifier(name="HAL_ControlWord")]
                                    )
                                )
                            ),
                            name="controlWord",
                        )
                    ],
                )
            ],
            typedefs=[
                Typedef(
                    type=Type(
                        typename=PQName(
                            segments=[NameSpecifier(name="HAL_ControlWord")],
                            classkey="struct",
                        )
                    ),
                    name="HAL_ControlWord",
                )
            ],
        )
    )


def test_class_anon_struct_as_globalvar() -> None:
    content = """
      struct {
        int m;
      } unnamed, *p_unnamed;
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            classes=[
                ClassScope(
                    class_decl=ClassDecl(
                        typename=PQName(
                            classkey="struct", segments=[AnonymousName(id=1)]
                        )
                    ),
                    fields=[
                        Field(
                            name="m",
                            type=Type(
                                typename=PQName(
                                    segments=[FundamentalSpecifier(name="int")],
                                )
                            ),
                            access="public",
                        )
                    ],
                )
            ],
            variables=[
                Variable(
                    name=PQName(segments=[NameSpecifier(name="unnamed")]),
                    type=Type(
                        typename=PQName(
                            classkey="struct", segments=[AnonymousName(id=1)]
                        )
                    ),
                ),
                Variable(
                    name=PQName(segments=[NameSpecifier(name="p_unnamed")]),
                    type=Pointer(
                        ptr_to=Type(
                            typename=PQName(
                                classkey="struct", segments=[AnonymousName(id=1)]
                            )
                        )
                    ),
                ),
            ],
        )
    )


def test_class_anon_struct_as_classvar() -> None:
    content = """
      struct AnonHolderClass {
        struct {
          int x;
        } a;
      };
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            classes=[
                ClassScope(
                    class_decl=ClassDecl(
                        typename=PQName(
                            segments=[NameSpecifier(name="AnonHolderClass")],
                            classkey="struct",
                        )
                    ),
                    classes=[
                        ClassScope(
                            class_decl=ClassDecl(
                                typename=PQName(
                                    segments=[AnonymousName(id=1)], classkey="struct"
                                ),
                                access="public",
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
                        )
                    ],
                    fields=[
                        Field(
                            access="public",
                            type=Type(
                                typename=PQName(
                                    segments=[AnonymousName(id=1)], classkey="struct"
                                )
                            ),
                            name="a",
                        )
                    ],
                )
            ]
        )
    )


def test_class_anon_struct_as_unnamed_classvar() -> None:
    content = """
      struct AnonHolderClass {
        struct {
          int x;
          int y;
        };
        int z;
      };
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            classes=[
                ClassScope(
                    class_decl=ClassDecl(
                        typename=PQName(
                            segments=[NameSpecifier(name="AnonHolderClass")],
                            classkey="struct",
                        )
                    ),
                    classes=[
                        ClassScope(
                            class_decl=ClassDecl(
                                typename=PQName(
                                    segments=[AnonymousName(id=1)], classkey="struct"
                                ),
                                access="public",
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
                                ),
                                Field(
                                    access="public",
                                    type=Type(
                                        typename=PQName(
                                            segments=[FundamentalSpecifier(name="int")]
                                        )
                                    ),
                                    name="y",
                                ),
                            ],
                        )
                    ],
                    fields=[
                        Field(
                            access="public",
                            type=Type(
                                typename=PQName(
                                    segments=[AnonymousName(id=1)], classkey="struct"
                                )
                            ),
                        ),
                        Field(
                            access="public",
                            type=Type(
                                typename=PQName(
                                    segments=[FundamentalSpecifier(name="int")]
                                )
                            ),
                            name="z",
                        ),
                    ],
                )
            ]
        )
    )


def test_initializer_with_initializer_list_1() -> None:
    content = """
      struct ComplexInit : SomeBase {
        ComplexInit(int i) : m_stuff{i, 2} { auto i = something(); }
      
        void fn();
      
        std::vector<int> m_stuff;
      };
      
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            classes=[
                ClassScope(
                    class_decl=ClassDecl(
                        typename=PQName(
                            segments=[NameSpecifier(name="ComplexInit")],
                            classkey="struct",
                        ),
                        bases=[
                            BaseClass(
                                access="public",
                                typename=PQName(
                                    segments=[NameSpecifier(name="SomeBase")]
                                ),
                            )
                        ],
                    ),
                    fields=[
                        Field(
                            access="public",
                            type=Type(
                                typename=PQName(
                                    segments=[
                                        NameSpecifier(name="std"),
                                        NameSpecifier(
                                            name="vector",
                                            specialization=TemplateSpecialization(
                                                args=[
                                                    TemplateArgument(
                                                        arg=Type(
                                                            typename=PQName(
                                                                segments=[
                                                                    FundamentalSpecifier(
                                                                        name="int"
                                                                    )
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
                            name="m_stuff",
                        )
                    ],
                    methods=[
                        Method(
                            return_type=None,
                            name=PQName(segments=[NameSpecifier(name="ComplexInit")]),
                            parameters=[
                                Parameter(
                                    type=Type(
                                        typename=PQName(
                                            segments=[FundamentalSpecifier(name="int")]
                                        )
                                    ),
                                    name="i",
                                )
                            ],
                            has_body=True,
                            access="public",
                            constructor=True,
                        ),
                        Method(
                            return_type=Type(
                                typename=PQName(
                                    segments=[FundamentalSpecifier(name="void")]
                                )
                            ),
                            name=PQName(segments=[NameSpecifier(name="fn")]),
                            parameters=[],
                            access="public",
                        ),
                    ],
                )
            ]
        )
    )


def test_initializer_with_initializer_list_2() -> None:
    content = """
      template <typename T> class future final {
      public:
        template <typename R>
        future(future<R> &&oth) noexcept
            : future(oth.then([](R &&val) -> T { return val; })) {}
      };
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            classes=[
                ClassScope(
                    class_decl=ClassDecl(
                        typename=PQName(
                            segments=[NameSpecifier(name="future")], classkey="class"
                        ),
                        template=TemplateDecl(
                            params=[TemplateTypeParam(typekey="typename", name="T")]
                        ),
                        final=True,
                    ),
                    methods=[
                        Method(
                            return_type=None,
                            name=PQName(segments=[NameSpecifier(name="future")]),
                            parameters=[
                                Parameter(
                                    type=MoveReference(
                                        moveref_to=Type(
                                            typename=PQName(
                                                segments=[
                                                    NameSpecifier(
                                                        name="future",
                                                        specialization=TemplateSpecialization(
                                                            args=[
                                                                TemplateArgument(
                                                                    arg=Type(
                                                                        typename=PQName(
                                                                            segments=[
                                                                                NameSpecifier(
                                                                                    name="R"
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
                                        )
                                    ),
                                    name="oth",
                                )
                            ],
                            has_body=True,
                            template=TemplateDecl(
                                params=[TemplateTypeParam(typekey="typename", name="R")]
                            ),
                            noexcept=Value(tokens=[]),
                            access="public",
                            constructor=True,
                        )
                    ],
                )
            ]
        )
    )


def test_class_with_arrays() -> None:
    content = """
      const int MAX_ITEM = 7;
      class Bird {
        int items[MAX_ITEM];
        int otherItems[7];
        int oneItem;
      };
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            classes=[
                ClassScope(
                    class_decl=ClassDecl(
                        typename=PQName(
                            segments=[NameSpecifier(name="Bird")], classkey="class"
                        )
                    ),
                    fields=[
                        Field(
                            access="private",
                            type=Array(
                                array_of=Type(
                                    typename=PQName(
                                        segments=[FundamentalSpecifier(name="int")]
                                    )
                                ),
                                size=Value(tokens=[Token(value="MAX_ITEM")]),
                            ),
                            name="items",
                        ),
                        Field(
                            access="private",
                            type=Array(
                                array_of=Type(
                                    typename=PQName(
                                        segments=[FundamentalSpecifier(name="int")]
                                    )
                                ),
                                size=Value(tokens=[Token(value="7")]),
                            ),
                            name="otherItems",
                        ),
                        Field(
                            access="private",
                            type=Type(
                                typename=PQName(
                                    segments=[FundamentalSpecifier(name="int")]
                                )
                            ),
                            name="oneItem",
                        ),
                    ],
                )
            ],
            variables=[
                Variable(
                    name=PQName(segments=[NameSpecifier(name="MAX_ITEM")]),
                    type=Type(
                        typename=PQName(segments=[FundamentalSpecifier(name="int")]),
                        const=True,
                    ),
                    value=Value(tokens=[Token(value="7")]),
                )
            ],
        )
    )


def test_class_fn_inline_impl() -> None:
    content = """
      class Monkey {
      private:
        static void Create();
      };
      inline void Monkey::Create() {}
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            classes=[
                ClassScope(
                    class_decl=ClassDecl(
                        typename=PQName(
                            segments=[NameSpecifier(name="Monkey")], classkey="class"
                        )
                    ),
                    methods=[
                        Method(
                            return_type=Type(
                                typename=PQName(
                                    segments=[FundamentalSpecifier(name="void")]
                                )
                            ),
                            name=PQName(segments=[NameSpecifier(name="Create")]),
                            parameters=[],
                            static=True,
                            access="private",
                        )
                    ],
                )
            ],
            method_impls=[
                Method(
                    return_type=Type(
                        typename=PQName(segments=[FundamentalSpecifier(name="void")])
                    ),
                    name=PQName(
                        segments=[
                            NameSpecifier(name="Monkey"),
                            NameSpecifier(name="Create"),
                        ]
                    ),
                    parameters=[],
                    inline=True,
                    has_body=True,
                )
            ],
        )
    )


def test_class_fn_virtual_final_override() -> None:
    content = """
      struct Lemon {
        virtual void foo() final;
        virtual void foo2();
      };
      
      struct Lime final : Lemon {
        void abc();
        void foo2() override;
      };
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            classes=[
                ClassScope(
                    class_decl=ClassDecl(
                        typename=PQName(
                            segments=[NameSpecifier(name="Lemon")], classkey="struct"
                        )
                    ),
                    methods=[
                        Method(
                            return_type=Type(
                                typename=PQName(
                                    segments=[FundamentalSpecifier(name="void")]
                                )
                            ),
                            name=PQName(segments=[NameSpecifier(name="foo")]),
                            parameters=[],
                            access="public",
                            virtual=True,
                            final=True,
                        ),
                        Method(
                            return_type=Type(
                                typename=PQName(
                                    segments=[FundamentalSpecifier(name="void")]
                                )
                            ),
                            name=PQName(segments=[NameSpecifier(name="foo2")]),
                            parameters=[],
                            access="public",
                            virtual=True,
                        ),
                    ],
                ),
                ClassScope(
                    class_decl=ClassDecl(
                        typename=PQName(
                            segments=[NameSpecifier(name="Lime")], classkey="struct"
                        ),
                        bases=[
                            BaseClass(
                                access="public",
                                typename=PQName(segments=[NameSpecifier(name="Lemon")]),
                            )
                        ],
                        final=True,
                    ),
                    methods=[
                        Method(
                            return_type=Type(
                                typename=PQName(
                                    segments=[FundamentalSpecifier(name="void")]
                                )
                            ),
                            name=PQName(segments=[NameSpecifier(name="abc")]),
                            parameters=[],
                            access="public",
                        ),
                        Method(
                            return_type=Type(
                                typename=PQName(
                                    segments=[FundamentalSpecifier(name="void")]
                                )
                            ),
                            name=PQName(segments=[NameSpecifier(name="foo2")]),
                            parameters=[],
                            access="public",
                            override=True,
                        ),
                    ],
                ),
            ]
        )
    )


def test_class_fn_return_class() -> None:
    content = """
      class Peach {
        int abc;
      };
      
      class Plumb {
        class Peach *doSomethingGreat(class Peach *pInCurPtr);
        class Peach *var;
      };
      
      class Peach *Plumb::myMethod(class Peach *pInPtr) {
        return pInPtr;
      }
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            classes=[
                ClassScope(
                    class_decl=ClassDecl(
                        typename=PQName(
                            segments=[NameSpecifier(name="Peach")], classkey="class"
                        )
                    ),
                    fields=[
                        Field(
                            access="private",
                            type=Type(
                                typename=PQName(
                                    segments=[FundamentalSpecifier(name="int")]
                                )
                            ),
                            name="abc",
                        )
                    ],
                ),
                ClassScope(
                    class_decl=ClassDecl(
                        typename=PQName(
                            segments=[NameSpecifier(name="Plumb")], classkey="class"
                        )
                    ),
                    fields=[
                        Field(
                            access="private",
                            type=Pointer(
                                ptr_to=Type(
                                    typename=PQName(
                                        segments=[NameSpecifier(name="Peach")],
                                        classkey="class",
                                    )
                                )
                            ),
                            name="var",
                        )
                    ],
                    methods=[
                        Method(
                            return_type=Pointer(
                                ptr_to=Type(
                                    typename=PQName(
                                        segments=[NameSpecifier(name="Peach")],
                                        classkey="class",
                                    )
                                )
                            ),
                            name=PQName(
                                segments=[NameSpecifier(name="doSomethingGreat")]
                            ),
                            parameters=[
                                Parameter(
                                    type=Pointer(
                                        ptr_to=Type(
                                            typename=PQName(
                                                segments=[NameSpecifier(name="Peach")],
                                                classkey="class",
                                            )
                                        )
                                    ),
                                    name="pInCurPtr",
                                )
                            ],
                            access="private",
                        )
                    ],
                ),
            ],
            method_impls=[
                Method(
                    return_type=Pointer(
                        ptr_to=Type(
                            typename=PQName(
                                segments=[NameSpecifier(name="Peach")], classkey="class"
                            )
                        )
                    ),
                    name=PQName(
                        segments=[
                            NameSpecifier(name="Plumb"),
                            NameSpecifier(name="myMethod"),
                        ]
                    ),
                    parameters=[
                        Parameter(
                            type=Pointer(
                                ptr_to=Type(
                                    typename=PQName(
                                        segments=[NameSpecifier(name="Peach")],
                                        classkey="class",
                                    )
                                )
                            ),
                            name="pInPtr",
                        )
                    ],
                    has_body=True,
                )
            ],
        )
    )


def test_class_fn_template_impl() -> None:
    content = """
      class Owl {
      private:
        template <typename T> int *tFunc(int count);
      };
      
      template <typename T> int *Owl::tFunc(int count) {
        if (count == 0) {
          return NULL;
        }
        return NULL;
      }
      
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            classes=[
                ClassScope(
                    class_decl=ClassDecl(
                        typename=PQName(
                            segments=[NameSpecifier(name="Owl")], classkey="class"
                        )
                    ),
                    methods=[
                        Method(
                            return_type=Pointer(
                                ptr_to=Type(
                                    typename=PQName(
                                        segments=[FundamentalSpecifier(name="int")]
                                    )
                                )
                            ),
                            name=PQName(segments=[NameSpecifier(name="tFunc")]),
                            parameters=[
                                Parameter(
                                    type=Type(
                                        typename=PQName(
                                            segments=[FundamentalSpecifier(name="int")]
                                        )
                                    ),
                                    name="count",
                                )
                            ],
                            template=TemplateDecl(
                                params=[TemplateTypeParam(typekey="typename", name="T")]
                            ),
                            access="private",
                        )
                    ],
                )
            ],
            method_impls=[
                Method(
                    return_type=Pointer(
                        ptr_to=Type(
                            typename=PQName(segments=[FundamentalSpecifier(name="int")])
                        )
                    ),
                    name=PQName(
                        segments=[
                            NameSpecifier(name="Owl"),
                            NameSpecifier(name="tFunc"),
                        ]
                    ),
                    parameters=[
                        Parameter(
                            type=Type(
                                typename=PQName(
                                    segments=[FundamentalSpecifier(name="int")]
                                )
                            ),
                            name="count",
                        )
                    ],
                    has_body=True,
                    template=TemplateDecl(
                        params=[TemplateTypeParam(typekey="typename", name="T")]
                    ),
                )
            ],
        )
    )


def test_class_fn_inline_template_impl() -> None:
    content = """
      class Chicken {
        template <typename T> static T Get();
      };
      template <typename T> T Chicken::Get() { return T(); }
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            classes=[
                ClassScope(
                    class_decl=ClassDecl(
                        typename=PQName(
                            segments=[NameSpecifier(name="Chicken")], classkey="class"
                        )
                    ),
                    methods=[
                        Method(
                            return_type=Type(
                                typename=PQName(segments=[NameSpecifier(name="T")])
                            ),
                            name=PQName(segments=[NameSpecifier(name="Get")]),
                            parameters=[],
                            static=True,
                            template=TemplateDecl(
                                params=[TemplateTypeParam(typekey="typename", name="T")]
                            ),
                            access="private",
                        )
                    ],
                )
            ],
            method_impls=[
                Method(
                    return_type=Type(
                        typename=PQName(segments=[NameSpecifier(name="T")])
                    ),
                    name=PQName(
                        segments=[
                            NameSpecifier(name="Chicken"),
                            NameSpecifier(name="Get"),
                        ]
                    ),
                    parameters=[],
                    has_body=True,
                    template=TemplateDecl(
                        params=[TemplateTypeParam(typekey="typename", name="T")]
                    ),
                )
            ],
        )
    )


def test_class_fn_explicit_constructors() -> None:
    content = """
      class Lizzard {
        Lizzard();
        explicit Lizzard(int a);
      };
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            classes=[
                ClassScope(
                    class_decl=ClassDecl(
                        typename=PQName(
                            segments=[NameSpecifier(name="Lizzard")], classkey="class"
                        )
                    ),
                    methods=[
                        Method(
                            return_type=None,
                            name=PQName(segments=[NameSpecifier(name="Lizzard")]),
                            parameters=[],
                            access="private",
                            constructor=True,
                        ),
                        Method(
                            return_type=None,
                            name=PQName(segments=[NameSpecifier(name="Lizzard")]),
                            parameters=[
                                Parameter(
                                    type=Type(
                                        typename=PQName(
                                            segments=[FundamentalSpecifier(name="int")]
                                        )
                                    ),
                                    name="a",
                                )
                            ],
                            access="private",
                            constructor=True,
                            explicit=True,
                        ),
                    ],
                )
            ]
        )
    )


def test_class_fn_default_constructor() -> None:
    content = """
      class DefaultConstDest {
      public:
        DefaultConstDest() = default;
      };
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            classes=[
                ClassScope(
                    class_decl=ClassDecl(
                        typename=PQName(
                            segments=[NameSpecifier(name="DefaultConstDest")],
                            classkey="class",
                        )
                    ),
                    methods=[
                        Method(
                            return_type=None,
                            name=PQName(
                                segments=[NameSpecifier(name="DefaultConstDest")]
                            ),
                            parameters=[],
                            access="public",
                            constructor=True,
                            default=True,
                        )
                    ],
                )
            ]
        )
    )


def test_class_fn_delete_constructor() -> None:
    content = """
      class A {
      public:
        A() = delete;
      };
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            classes=[
                ClassScope(
                    class_decl=ClassDecl(
                        typename=PQName(
                            segments=[NameSpecifier(name="A")], classkey="class"
                        )
                    ),
                    methods=[
                        Method(
                            return_type=None,
                            name=PQName(segments=[NameSpecifier(name="A")]),
                            parameters=[],
                            access="public",
                            constructor=True,
                            deleted=True,
                        )
                    ],
                )
            ]
        )
    )


def test_class_multi_vars() -> None:
    content = """
      class Grape {
      public:
        int a, b, c;
        map<string, int> d;
        map<string, int> e, f;
      };
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            classes=[
                ClassScope(
                    class_decl=ClassDecl(
                        typename=PQName(
                            segments=[NameSpecifier(name="Grape")], classkey="class"
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
                            name="a",
                        ),
                        Field(
                            access="public",
                            type=Type(
                                typename=PQName(
                                    segments=[FundamentalSpecifier(name="int")]
                                )
                            ),
                            name="b",
                        ),
                        Field(
                            access="public",
                            type=Type(
                                typename=PQName(
                                    segments=[FundamentalSpecifier(name="int")]
                                )
                            ),
                            name="c",
                        ),
                        Field(
                            access="public",
                            type=Type(
                                typename=PQName(
                                    segments=[
                                        NameSpecifier(
                                            name="map",
                                            specialization=TemplateSpecialization(
                                                args=[
                                                    TemplateArgument(
                                                        arg=Type(
                                                            typename=PQName(
                                                                segments=[
                                                                    NameSpecifier(
                                                                        name="string"
                                                                    )
                                                                ]
                                                            )
                                                        )
                                                    ),
                                                    TemplateArgument(
                                                        arg=Type(
                                                            typename=PQName(
                                                                segments=[
                                                                    FundamentalSpecifier(
                                                                        name="int"
                                                                    )
                                                                ]
                                                            )
                                                        )
                                                    ),
                                                ]
                                            ),
                                        )
                                    ]
                                )
                            ),
                            name="d",
                        ),
                        Field(
                            access="public",
                            type=Type(
                                typename=PQName(
                                    segments=[
                                        NameSpecifier(
                                            name="map",
                                            specialization=TemplateSpecialization(
                                                args=[
                                                    TemplateArgument(
                                                        arg=Type(
                                                            typename=PQName(
                                                                segments=[
                                                                    NameSpecifier(
                                                                        name="string"
                                                                    )
                                                                ]
                                                            )
                                                        )
                                                    ),
                                                    TemplateArgument(
                                                        arg=Type(
                                                            typename=PQName(
                                                                segments=[
                                                                    FundamentalSpecifier(
                                                                        name="int"
                                                                    )
                                                                ]
                                                            )
                                                        )
                                                    ),
                                                ]
                                            ),
                                        )
                                    ]
                                )
                            ),
                            name="e",
                        ),
                        Field(
                            access="public",
                            type=Type(
                                typename=PQName(
                                    segments=[
                                        NameSpecifier(
                                            name="map",
                                            specialization=TemplateSpecialization(
                                                args=[
                                                    TemplateArgument(
                                                        arg=Type(
                                                            typename=PQName(
                                                                segments=[
                                                                    NameSpecifier(
                                                                        name="string"
                                                                    )
                                                                ]
                                                            )
                                                        )
                                                    ),
                                                    TemplateArgument(
                                                        arg=Type(
                                                            typename=PQName(
                                                                segments=[
                                                                    FundamentalSpecifier(
                                                                        name="int"
                                                                    )
                                                                ]
                                                            )
                                                        )
                                                    ),
                                                ]
                                            ),
                                        )
                                    ]
                                )
                            ),
                            name="f",
                        ),
                    ],
                )
            ]
        )
    )


def test_class_static_const_var_expr() -> None:
    content = """
      class PandaClass {
        static const int CONST_A = (1 << 7) - 1;
        static const int CONST_B = sizeof(int);
      };
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            classes=[
                ClassScope(
                    class_decl=ClassDecl(
                        typename=PQName(
                            segments=[NameSpecifier(name="PandaClass")],
                            classkey="class",
                        )
                    ),
                    fields=[
                        Field(
                            access="private",
                            type=Type(
                                typename=PQName(
                                    segments=[FundamentalSpecifier(name="int")]
                                ),
                                const=True,
                            ),
                            name="CONST_A",
                            value=Value(
                                tokens=[
                                    Token(value="("),
                                    Token(value="1"),
                                    Token(value="<<"),
                                    Token(value="7"),
                                    Token(value=")"),
                                    Token(value="-"),
                                    Token(value="1"),
                                ]
                            ),
                            static=True,
                        ),
                        Field(
                            access="private",
                            type=Type(
                                typename=PQName(
                                    segments=[FundamentalSpecifier(name="int")]
                                ),
                                const=True,
                            ),
                            name="CONST_B",
                            value=Value(
                                tokens=[
                                    Token(value="sizeof"),
                                    Token(value="("),
                                    Token(value="int"),
                                    Token(value=")"),
                                ]
                            ),
                            static=True,
                        ),
                    ],
                )
            ]
        )
    )


def test_class_fwd_struct() -> None:
    content = """
      class PotatoClass {
        struct FwdStruct;
        FwdStruct *ptr;
        struct FwdStruct {
          int a;
        };
      };
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            classes=[
                ClassScope(
                    class_decl=ClassDecl(
                        typename=PQName(
                            segments=[NameSpecifier(name="PotatoClass")],
                            classkey="class",
                        )
                    ),
                    classes=[
                        ClassScope(
                            class_decl=ClassDecl(
                                typename=PQName(
                                    segments=[NameSpecifier(name="FwdStruct")],
                                    classkey="struct",
                                ),
                                access="private",
                            ),
                            fields=[
                                Field(
                                    access="public",
                                    type=Type(
                                        typename=PQName(
                                            segments=[FundamentalSpecifier(name="int")]
                                        )
                                    ),
                                    name="a",
                                )
                            ],
                        )
                    ],
                    fields=[
                        Field(
                            access="private",
                            type=Pointer(
                                ptr_to=Type(
                                    typename=PQName(
                                        segments=[NameSpecifier(name="FwdStruct")]
                                    )
                                )
                            ),
                            name="ptr",
                        )
                    ],
                    forward_decls=[
                        ForwardDecl(
                            typename=PQName(
                                segments=[NameSpecifier(name="FwdStruct")],
                                classkey="struct",
                            ),
                            access="private",
                        )
                    ],
                )
            ]
        )
    )


def test_class_multi_array() -> None:
    content = """
      struct Picture {
        char name[25];
        unsigned int pdata[128][256];
      };
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            classes=[
                ClassScope(
                    class_decl=ClassDecl(
                        typename=PQName(
                            segments=[NameSpecifier(name="Picture")], classkey="struct"
                        )
                    ),
                    fields=[
                        Field(
                            access="public",
                            type=Array(
                                array_of=Type(
                                    typename=PQName(
                                        segments=[FundamentalSpecifier(name="char")]
                                    )
                                ),
                                size=Value(tokens=[Token(value="25")]),
                            ),
                            name="name",
                        ),
                        Field(
                            access="public",
                            type=Array(
                                array_of=Array(
                                    array_of=Type(
                                        typename=PQName(
                                            segments=[
                                                FundamentalSpecifier(
                                                    name="unsigned int"
                                                )
                                            ]
                                        )
                                    ),
                                    size=Value(tokens=[Token(value="256")]),
                                ),
                                size=Value(tokens=[Token(value="128")]),
                            ),
                            name="pdata",
                        ),
                    ],
                )
            ]
        )
    )


def test_class_noexcept() -> None:
    content = """
      struct Grackle {
        void no_noexcept();
        void just_noexcept() noexcept;
        void const_noexcept() const noexcept;
        void noexcept_bool() noexcept(true);
        void const_noexcept_bool() const noexcept(true);
        void noexcept_noexceptOperator() noexcept(noexcept(Grackle()));
        void const_noexcept_noexceptOperator() const noexcept(noexcept(Grackle()));
      };
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            classes=[
                ClassScope(
                    class_decl=ClassDecl(
                        typename=PQName(
                            segments=[NameSpecifier(name="Grackle")], classkey="struct"
                        )
                    ),
                    methods=[
                        Method(
                            return_type=Type(
                                typename=PQName(
                                    segments=[FundamentalSpecifier(name="void")]
                                )
                            ),
                            name=PQName(segments=[NameSpecifier(name="no_noexcept")]),
                            parameters=[],
                            access="public",
                        ),
                        Method(
                            return_type=Type(
                                typename=PQName(
                                    segments=[FundamentalSpecifier(name="void")]
                                )
                            ),
                            name=PQName(segments=[NameSpecifier(name="just_noexcept")]),
                            parameters=[],
                            noexcept=Value(tokens=[]),
                            access="public",
                        ),
                        Method(
                            return_type=Type(
                                typename=PQName(
                                    segments=[FundamentalSpecifier(name="void")]
                                )
                            ),
                            name=PQName(
                                segments=[NameSpecifier(name="const_noexcept")]
                            ),
                            parameters=[],
                            noexcept=Value(tokens=[]),
                            access="public",
                            const=True,
                        ),
                        Method(
                            return_type=Type(
                                typename=PQName(
                                    segments=[FundamentalSpecifier(name="void")]
                                )
                            ),
                            name=PQName(segments=[NameSpecifier(name="noexcept_bool")]),
                            parameters=[],
                            noexcept=Value(tokens=[Token(value="true")]),
                            access="public",
                        ),
                        Method(
                            return_type=Type(
                                typename=PQName(
                                    segments=[FundamentalSpecifier(name="void")]
                                )
                            ),
                            name=PQName(
                                segments=[NameSpecifier(name="const_noexcept_bool")]
                            ),
                            parameters=[],
                            noexcept=Value(tokens=[Token(value="true")]),
                            access="public",
                            const=True,
                        ),
                        Method(
                            return_type=Type(
                                typename=PQName(
                                    segments=[FundamentalSpecifier(name="void")]
                                )
                            ),
                            name=PQName(
                                segments=[
                                    NameSpecifier(name="noexcept_noexceptOperator")
                                ]
                            ),
                            parameters=[],
                            noexcept=Value(
                                tokens=[
                                    Token(value="noexcept"),
                                    Token(value="("),
                                    Token(value="Grackle"),
                                    Token(value="("),
                                    Token(value=")"),
                                    Token(value=")"),
                                ]
                            ),
                            access="public",
                        ),
                        Method(
                            return_type=Type(
                                typename=PQName(
                                    segments=[FundamentalSpecifier(name="void")]
                                )
                            ),
                            name=PQName(
                                segments=[
                                    NameSpecifier(
                                        name="const_noexcept_noexceptOperator"
                                    )
                                ]
                            ),
                            parameters=[],
                            noexcept=Value(
                                tokens=[
                                    Token(value="noexcept"),
                                    Token(value="("),
                                    Token(value="Grackle"),
                                    Token(value="("),
                                    Token(value=")"),
                                    Token(value=")"),
                                ]
                            ),
                            access="public",
                            const=True,
                        ),
                    ],
                )
            ]
        )
    )


def test_class_volatile() -> None:
    content = """
      class Foo
      {
      public:
      
      private:
      
      volatile bool               myToShutDown;
      
      };
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            classes=[
                ClassScope(
                    class_decl=ClassDecl(
                        typename=PQName(
                            segments=[NameSpecifier(name="Foo")], classkey="class"
                        )
                    ),
                    fields=[
                        Field(
                            access="private",
                            type=Type(
                                typename=PQName(
                                    segments=[FundamentalSpecifier(name="bool")]
                                ),
                                volatile=True,
                            ),
                            name="myToShutDown",
                        )
                    ],
                )
            ]
        )
    )


def test_class_mutable() -> None:
    content = """
      class Foo
      {
      private:
      
      mutable volatile Standard_Integer myRefCount_;
      
      };
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            classes=[
                ClassScope(
                    class_decl=ClassDecl(
                        typename=PQName(
                            segments=[NameSpecifier(name="Foo")], classkey="class"
                        )
                    ),
                    fields=[
                        Field(
                            access="private",
                            type=Type(
                                typename=PQName(
                                    segments=[NameSpecifier(name="Standard_Integer")]
                                ),
                                volatile=True,
                            ),
                            name="myRefCount_",
                            mutable=True,
                        )
                    ],
                )
            ]
        )
    )


def test_nested_class_access() -> None:
    content = """
      class Outer {
          struct Inner {
              void fn();
          };

          void ofn();
      };
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            classes=[
                ClassScope(
                    class_decl=ClassDecl(
                        typename=PQName(
                            segments=[NameSpecifier(name="Outer")], classkey="class"
                        )
                    ),
                    classes=[
                        ClassScope(
                            class_decl=ClassDecl(
                                typename=PQName(
                                    segments=[NameSpecifier(name="Inner")],
                                    classkey="struct",
                                ),
                                access="private",
                            ),
                            methods=[
                                Method(
                                    return_type=Type(
                                        typename=PQName(
                                            segments=[FundamentalSpecifier(name="void")]
                                        )
                                    ),
                                    name=PQName(segments=[NameSpecifier(name="fn")]),
                                    parameters=[],
                                    access="public",
                                )
                            ],
                        )
                    ],
                    methods=[
                        Method(
                            return_type=Type(
                                typename=PQName(
                                    segments=[FundamentalSpecifier(name="void")]
                                )
                            ),
                            name=PQName(segments=[NameSpecifier(name="ofn")]),
                            parameters=[],
                            access="private",
                        )
                    ],
                )
            ]
        )
    )


def test_class_with_typedef() -> None:
    content = """
      template <class SomeType> class A {
       public:
        typedef B <SomeType> C;

        A();

       protected:
        C aCInstance;
      };
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            classes=[
                ClassScope(
                    class_decl=ClassDecl(
                        typename=PQName(
                            segments=[NameSpecifier(name="A")], classkey="class"
                        ),
                        template=TemplateDecl(
                            params=[TemplateTypeParam(typekey="class", name="SomeType")]
                        ),
                    ),
                    fields=[
                        Field(
                            access="protected",
                            type=Type(
                                typename=PQName(segments=[NameSpecifier(name="C")])
                            ),
                            name="aCInstance",
                        )
                    ],
                    methods=[
                        Method(
                            return_type=None,
                            name=PQName(segments=[NameSpecifier(name="A")]),
                            parameters=[],
                            access="public",
                            constructor=True,
                        )
                    ],
                    typedefs=[
                        Typedef(
                            type=Type(
                                typename=PQName(
                                    segments=[
                                        NameSpecifier(
                                            name="B",
                                            specialization=TemplateSpecialization(
                                                args=[
                                                    TemplateArgument(
                                                        arg=Type(
                                                            typename=PQName(
                                                                segments=[
                                                                    NameSpecifier(
                                                                        name="SomeType"
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
                            name="C",
                            access="public",
                        )
                    ],
                )
            ]
        )
    )


def test_class_ref_qualifiers() -> None:
    content = """
      struct X {
        void fn0();
        void fn1() &;
        void fn2() &&;
        void fn3() && = 0;
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
                            name=PQName(segments=[NameSpecifier(name="fn0")]),
                            parameters=[],
                            access="public",
                        ),
                        Method(
                            return_type=Type(
                                typename=PQName(
                                    segments=[FundamentalSpecifier(name="void")]
                                )
                            ),
                            name=PQName(segments=[NameSpecifier(name="fn1")]),
                            parameters=[],
                            access="public",
                            ref_qualifier="&",
                        ),
                        Method(
                            return_type=Type(
                                typename=PQName(
                                    segments=[FundamentalSpecifier(name="void")]
                                )
                            ),
                            name=PQName(segments=[NameSpecifier(name="fn2")]),
                            parameters=[],
                            access="public",
                            ref_qualifier="&&",
                        ),
                        Method(
                            return_type=Type(
                                typename=PQName(
                                    segments=[FundamentalSpecifier(name="void")]
                                )
                            ),
                            name=PQName(segments=[NameSpecifier(name="fn3")]),
                            parameters=[],
                            access="public",
                            ref_qualifier="&&",
                            pure_virtual=True,
                        ),
                    ],
                )
            ]
        )
    )


def test_method_outside_class() -> None:
    content = """
      int foo::bar() { return 1; }
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            method_impls=[
                Method(
                    return_type=Type(
                        typename=PQName(segments=[FundamentalSpecifier(name="int")])
                    ),
                    name=PQName(
                        segments=[NameSpecifier(name="foo"), NameSpecifier(name="bar")]
                    ),
                    parameters=[],
                    has_body=True,
                )
            ]
        )
    )


def test_constructor_outside_class() -> None:
    content = """
      inline foo::foo() {}
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            method_impls=[
                Method(
                    return_type=None,
                    name=PQName(
                        segments=[NameSpecifier(name="foo"), NameSpecifier(name="foo")]
                    ),
                    parameters=[],
                    inline=True,
                    has_body=True,
                    constructor=True,
                )
            ]
        )
    )


def test_class_inline_static() -> None:
    content = """
      struct X {
          inline static bool Foo = 1;
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
                    fields=[
                        Field(
                            access="public",
                            type=Type(
                                typename=PQName(
                                    segments=[FundamentalSpecifier(name="bool")]
                                )
                            ),
                            name="Foo",
                            value=Value(tokens=[Token(value="1")]),
                            static=True,
                            inline=True,
                        )
                    ],
                )
            ]
        )
    )
