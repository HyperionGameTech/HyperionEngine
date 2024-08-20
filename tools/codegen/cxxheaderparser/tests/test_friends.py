# Note: testcases generated via `python -m cxxheaderparser.gentest`

from cxxheaderparser.types import (
    ClassDecl,
    Field,
    ForwardDecl,
    FriendDecl,
    FundamentalSpecifier,
    Method,
    NameSpecifier,
    PQName,
    Parameter,
    Reference,
    TemplateDecl,
    TemplateTypeParam,
    Type,
)
from cxxheaderparser.simple import (
    ClassScope,
    NamespaceScope,
    parse_string,
    ParsedData,
)


# friends
def test_various_friends() -> None:
    content = """
      class FX {
      public:
        FX(char);
        ~FX();
        void fn() const;
      };
      
      class FF {
        friend class FX;
        friend FX::FX(char), FX::~FX();
        friend void FX::fn() const;
      };
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            classes=[
                ClassScope(
                    class_decl=ClassDecl(
                        typename=PQName(
                            segments=[NameSpecifier(name="FX")], classkey="class"
                        )
                    ),
                    methods=[
                        Method(
                            return_type=None,
                            name=PQName(segments=[NameSpecifier(name="FX")]),
                            parameters=[
                                Parameter(
                                    type=Type(
                                        typename=PQName(
                                            segments=[FundamentalSpecifier(name="char")]
                                        )
                                    )
                                )
                            ],
                            access="public",
                            constructor=True,
                        ),
                        Method(
                            return_type=None,
                            name=PQName(segments=[NameSpecifier(name="~FX")]),
                            parameters=[],
                            access="public",
                            destructor=True,
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
                            const=True,
                        ),
                    ],
                ),
                ClassScope(
                    class_decl=ClassDecl(
                        typename=PQName(
                            segments=[NameSpecifier(name="FF")], classkey="class"
                        )
                    ),
                    friends=[
                        FriendDecl(
                            cls=ForwardDecl(
                                typename=PQName(
                                    segments=[NameSpecifier(name="FX")],
                                    classkey="class",
                                ),
                                access="private",
                            )
                        ),
                        FriendDecl(
                            fn=Method(
                                return_type=None,
                                name=PQName(
                                    segments=[
                                        NameSpecifier(name="FX"),
                                        NameSpecifier(name="FX"),
                                    ]
                                ),
                                parameters=[
                                    Parameter(
                                        type=Type(
                                            typename=PQName(
                                                segments=[
                                                    FundamentalSpecifier(name="char")
                                                ]
                                            )
                                        )
                                    )
                                ],
                                access="private",
                                constructor=True,
                            )
                        ),
                        FriendDecl(
                            fn=Method(
                                return_type=Type(
                                    typename=PQName(
                                        segments=[
                                            NameSpecifier(name="FX"),
                                            NameSpecifier(name="FX"),
                                        ]
                                    )
                                ),
                                name=PQName(
                                    segments=[
                                        NameSpecifier(name="FX"),
                                        NameSpecifier(name="~FX"),
                                    ]
                                ),
                                parameters=[],
                                access="private",
                            )
                        ),
                        FriendDecl(
                            fn=Method(
                                return_type=Type(
                                    typename=PQName(
                                        segments=[FundamentalSpecifier(name="void")]
                                    )
                                ),
                                name=PQName(
                                    segments=[
                                        NameSpecifier(name="FX"),
                                        NameSpecifier(name="fn"),
                                    ]
                                ),
                                parameters=[],
                                access="private",
                                const=True,
                            )
                        ),
                    ],
                ),
            ]
        )
    )


def test_more_friends() -> None:
    content = """
      template <typename T> struct X { static int x; };
      
      struct BFF {
        void fn() const;
      };
      
      struct F {
        friend enum B;
        friend void BFF::fn() const;
      
        template <typename T> friend class X;
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
                        ),
                        template=TemplateDecl(
                            params=[TemplateTypeParam(typekey="typename", name="T")]
                        ),
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
                            static=True,
                        )
                    ],
                ),
                ClassScope(
                    class_decl=ClassDecl(
                        typename=PQName(
                            segments=[NameSpecifier(name="BFF")], classkey="struct"
                        )
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
                            const=True,
                        )
                    ],
                ),
                ClassScope(
                    class_decl=ClassDecl(
                        typename=PQName(
                            segments=[NameSpecifier(name="F")], classkey="struct"
                        )
                    ),
                    friends=[
                        FriendDecl(
                            cls=ForwardDecl(
                                typename=PQName(
                                    segments=[NameSpecifier(name="B")], classkey="enum"
                                ),
                                access="public",
                            )
                        ),
                        FriendDecl(
                            fn=Method(
                                return_type=Type(
                                    typename=PQName(
                                        segments=[FundamentalSpecifier(name="void")]
                                    )
                                ),
                                name=PQName(
                                    segments=[
                                        NameSpecifier(name="BFF"),
                                        NameSpecifier(name="fn"),
                                    ]
                                ),
                                parameters=[],
                                access="public",
                                const=True,
                            )
                        ),
                        FriendDecl(
                            cls=ForwardDecl(
                                typename=PQName(
                                    segments=[NameSpecifier(name="X")], classkey="class"
                                ),
                                template=TemplateDecl(
                                    params=[
                                        TemplateTypeParam(typekey="typename", name="T")
                                    ]
                                ),
                                access="public",
                            )
                        ),
                    ],
                ),
            ]
        )
    )


def test_friend_type_no_class() -> None:
    content = """
      class DogClass;
      class CatClass {
        friend DogClass;
      };
      
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            classes=[
                ClassScope(
                    class_decl=ClassDecl(
                        typename=PQName(
                            segments=[NameSpecifier(name="CatClass")], classkey="class"
                        )
                    ),
                    friends=[
                        FriendDecl(
                            cls=ForwardDecl(
                                typename=PQName(
                                    segments=[NameSpecifier(name="DogClass")]
                                ),
                                access="private",
                            )
                        )
                    ],
                )
            ],
            forward_decls=[
                ForwardDecl(
                    typename=PQName(
                        segments=[NameSpecifier(name="DogClass")], classkey="class"
                    )
                )
            ],
        )
    )


def test_friend_with_impl() -> None:
    content = """
      // clang-format off
      class Garlic {
      public:
          friend int genNum(C& a)
          {
            return obj.meth().num();
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
                            segments=[NameSpecifier(name="Garlic")], classkey="class"
                        )
                    ),
                    friends=[
                        FriendDecl(
                            fn=Method(
                                return_type=Type(
                                    typename=PQName(
                                        segments=[FundamentalSpecifier(name="int")]
                                    )
                                ),
                                name=PQName(segments=[NameSpecifier(name="genNum")]),
                                parameters=[
                                    Parameter(
                                        type=Reference(
                                            ref_to=Type(
                                                typename=PQName(
                                                    segments=[NameSpecifier(name="C")]
                                                )
                                            )
                                        ),
                                        name="a",
                                    )
                                ],
                                has_body=True,
                                access="public",
                            )
                        )
                    ],
                )
            ]
        )
    )
