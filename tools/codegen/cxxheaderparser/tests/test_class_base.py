# Note: testcases generated via `python -m cxxheaderparser.gentest`

from cxxheaderparser.types import (
    BaseClass,
    ClassDecl,
    Field,
    FundamentalSpecifier,
    Method,
    NameSpecifier,
    PQName,
    TemplateArgument,
    TemplateSpecialization,
    Token,
    Type,
)
from cxxheaderparser.simple import (
    ClassScope,
    NamespaceScope,
    parse_string,
    ParsedData,
)


def test_class_private_base() -> None:
    content = """
      namespace Citrus
      {
        class BloodOrange { };
      }

      class Bananna: public Citrus::BloodOrange
      {
      };

      class ExcellentCake: private Citrus::BloodOrange, Convoluted::Nested::Mixin
      {
      };
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            classes=[
                ClassScope(
                    class_decl=ClassDecl(
                        typename=PQName(
                            segments=[NameSpecifier(name="Bananna")], classkey="class"
                        ),
                        bases=[
                            BaseClass(
                                access="public",
                                typename=PQName(
                                    segments=[
                                        NameSpecifier(name="Citrus"),
                                        NameSpecifier(name="BloodOrange"),
                                    ]
                                ),
                            )
                        ],
                    )
                ),
                ClassScope(
                    class_decl=ClassDecl(
                        typename=PQName(
                            segments=[NameSpecifier(name="ExcellentCake")],
                            classkey="class",
                        ),
                        bases=[
                            BaseClass(
                                access="private",
                                typename=PQName(
                                    segments=[
                                        NameSpecifier(name="Citrus"),
                                        NameSpecifier(name="BloodOrange"),
                                    ]
                                ),
                            ),
                            BaseClass(
                                access="private",
                                typename=PQName(
                                    segments=[
                                        NameSpecifier(name="Convoluted"),
                                        NameSpecifier(name="Nested"),
                                        NameSpecifier(name="Mixin"),
                                    ]
                                ),
                            ),
                        ],
                    )
                ),
            ],
            namespaces={
                "Citrus": NamespaceScope(
                    name="Citrus",
                    classes=[
                        ClassScope(
                            class_decl=ClassDecl(
                                typename=PQName(
                                    segments=[NameSpecifier(name="BloodOrange")],
                                    classkey="class",
                                )
                            )
                        )
                    ],
                )
            },
        )
    )


def test_class_virtual_base() -> None:
    content = """
      class BaseMangoClass {};
      class MangoClass : virtual public BaseMangoClass {};
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            classes=[
                ClassScope(
                    class_decl=ClassDecl(
                        typename=PQName(
                            segments=[NameSpecifier(name="BaseMangoClass")],
                            classkey="class",
                        )
                    )
                ),
                ClassScope(
                    class_decl=ClassDecl(
                        typename=PQName(
                            segments=[NameSpecifier(name="MangoClass")],
                            classkey="class",
                        ),
                        bases=[
                            BaseClass(
                                access="public",
                                typename=PQName(
                                    segments=[NameSpecifier(name="BaseMangoClass")]
                                ),
                                virtual=True,
                            )
                        ],
                    )
                ),
            ]
        )
    )


def test_class_multiple_base_with_virtual() -> None:
    content = """
      class BlueJay : public Bird, public virtual Food {
      public:
        BlueJay() {}
      };
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            classes=[
                ClassScope(
                    class_decl=ClassDecl(
                        typename=PQName(
                            segments=[NameSpecifier(name="BlueJay")], classkey="class"
                        ),
                        bases=[
                            BaseClass(
                                access="public",
                                typename=PQName(segments=[NameSpecifier(name="Bird")]),
                            ),
                            BaseClass(
                                access="public",
                                typename=PQName(segments=[NameSpecifier(name="Food")]),
                                virtual=True,
                            ),
                        ],
                    ),
                    methods=[
                        Method(
                            return_type=None,
                            name=PQName(segments=[NameSpecifier(name="BlueJay")]),
                            parameters=[],
                            has_body=True,
                            access="public",
                            constructor=True,
                        )
                    ],
                )
            ]
        )
    )


def test_class_base_specialized() -> None:
    content = """
      class Pea : public Vegetable<Green> {
        int i;
      };
      
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            classes=[
                ClassScope(
                    class_decl=ClassDecl(
                        typename=PQName(
                            segments=[NameSpecifier(name="Pea")], classkey="class"
                        ),
                        bases=[
                            BaseClass(
                                access="public",
                                typename=PQName(
                                    segments=[
                                        NameSpecifier(
                                            name="Vegetable",
                                            specialization=TemplateSpecialization(
                                                args=[
                                                    TemplateArgument(
                                                        arg=Type(
                                                            typename=PQName(
                                                                segments=[
                                                                    NameSpecifier(
                                                                        name="Green"
                                                                    )
                                                                ]
                                                            )
                                                        )
                                                    )
                                                ]
                                            ),
                                        )
                                    ]
                                ),
                            )
                        ],
                    ),
                    fields=[
                        Field(
                            access="private",
                            type=Type(
                                typename=PQName(
                                    segments=[FundamentalSpecifier(name="int")]
                                )
                            ),
                            name="i",
                        )
                    ],
                )
            ]
        )
    )
