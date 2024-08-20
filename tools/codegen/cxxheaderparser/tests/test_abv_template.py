# Note: testcases generated via `python -m cxxheaderparser.gentest`
#
# Tests various aspects of abbreviated function templates
#

from cxxheaderparser.simple import NamespaceScope, ParsedData, parse_string
from cxxheaderparser.types import (
    AutoSpecifier,
    Function,
    FundamentalSpecifier,
    NameSpecifier,
    PQName,
    Parameter,
    Pointer,
    Reference,
    TemplateDecl,
    TemplateNonTypeParam,
    Type,
)


def test_abv_template_f1() -> None:
    content = """
      void f1(auto);       // same as template<class T> void f1(T)
      void f1p(auto p);
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            functions=[
                Function(
                    return_type=Type(
                        typename=PQName(segments=[FundamentalSpecifier(name="void")])
                    ),
                    name=PQName(segments=[NameSpecifier(name="f1")]),
                    parameters=[
                        Parameter(
                            type=Type(typename=PQName(segments=[AutoSpecifier()]))
                        )
                    ],
                    template=TemplateDecl(
                        params=[
                            TemplateNonTypeParam(
                                type=Type(typename=PQName(segments=[AutoSpecifier()])),
                                param_idx=0,
                            )
                        ]
                    ),
                ),
                Function(
                    return_type=Type(
                        typename=PQName(segments=[FundamentalSpecifier(name="void")])
                    ),
                    name=PQName(segments=[NameSpecifier(name="f1p")]),
                    parameters=[
                        Parameter(
                            type=Type(typename=PQName(segments=[AutoSpecifier()])),
                            name="p",
                        )
                    ],
                    template=TemplateDecl(
                        params=[
                            TemplateNonTypeParam(
                                type=Type(typename=PQName(segments=[AutoSpecifier()])),
                                param_idx=0,
                            )
                        ]
                    ),
                ),
            ]
        )
    )


def test_abv_template_f2() -> None:
    content = """
      void f2(C1 auto);    // same as template<C1 T> void f2(T), if C1 is a concept
      void f2p(C1 auto p);
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            functions=[
                Function(
                    return_type=Type(
                        typename=PQName(segments=[FundamentalSpecifier(name="void")])
                    ),
                    name=PQName(segments=[NameSpecifier(name="f2")]),
                    parameters=[
                        Parameter(
                            type=Type(typename=PQName(segments=[AutoSpecifier()]))
                        )
                    ],
                    template=TemplateDecl(
                        params=[
                            TemplateNonTypeParam(
                                type=Type(
                                    typename=PQName(segments=[NameSpecifier(name="C1")])
                                ),
                                param_idx=0,
                            )
                        ]
                    ),
                ),
                Function(
                    return_type=Type(
                        typename=PQName(segments=[FundamentalSpecifier(name="void")])
                    ),
                    name=PQName(segments=[NameSpecifier(name="f2p")]),
                    parameters=[
                        Parameter(
                            type=Type(typename=PQName(segments=[AutoSpecifier()])),
                            name="p",
                        )
                    ],
                    template=TemplateDecl(
                        params=[
                            TemplateNonTypeParam(
                                type=Type(
                                    typename=PQName(segments=[NameSpecifier(name="C1")])
                                ),
                                param_idx=0,
                            )
                        ]
                    ),
                ),
            ]
        )
    )


def test_abv_template_f3() -> None:
    content = """
      void f3(C2 auto...);     // same as template<C2... Ts> void f3(Ts...), if C2 is a
                               // concept
      void f3p(C2 auto p...);
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            functions=[
                Function(
                    return_type=Type(
                        typename=PQName(segments=[FundamentalSpecifier(name="void")])
                    ),
                    name=PQName(segments=[NameSpecifier(name="f3")]),
                    parameters=[
                        Parameter(
                            type=Type(typename=PQName(segments=[AutoSpecifier()])),
                            param_pack=True,
                        )
                    ],
                    template=TemplateDecl(
                        params=[
                            TemplateNonTypeParam(
                                type=Type(
                                    typename=PQName(segments=[NameSpecifier(name="C2")])
                                ),
                                param_idx=0,
                                param_pack=True,
                            )
                        ]
                    ),
                ),
                Function(
                    return_type=Type(
                        typename=PQName(segments=[FundamentalSpecifier(name="void")])
                    ),
                    name=PQName(segments=[NameSpecifier(name="f3p")]),
                    parameters=[
                        Parameter(
                            type=Type(typename=PQName(segments=[AutoSpecifier()])),
                            name="p",
                            param_pack=True,
                        )
                    ],
                    template=TemplateDecl(
                        params=[
                            TemplateNonTypeParam(
                                type=Type(
                                    typename=PQName(segments=[NameSpecifier(name="C2")])
                                ),
                                param_idx=0,
                                param_pack=True,
                            )
                        ]
                    ),
                ),
            ]
        )
    )


def test_abv_template_f4() -> None:
    content = """
      void f4(C2 auto, ...);   // same as template<C2 T> void f4(T...), if C2 is a concept
      void f4p(C2 auto p,...);
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            functions=[
                Function(
                    return_type=Type(
                        typename=PQName(segments=[FundamentalSpecifier(name="void")])
                    ),
                    name=PQName(segments=[NameSpecifier(name="f4")]),
                    parameters=[
                        Parameter(
                            type=Type(typename=PQName(segments=[AutoSpecifier()]))
                        )
                    ],
                    vararg=True,
                    template=TemplateDecl(
                        params=[
                            TemplateNonTypeParam(
                                type=Type(
                                    typename=PQName(segments=[NameSpecifier(name="C2")])
                                ),
                                param_idx=0,
                            )
                        ]
                    ),
                ),
                Function(
                    return_type=Type(
                        typename=PQName(segments=[FundamentalSpecifier(name="void")])
                    ),
                    name=PQName(segments=[NameSpecifier(name="f4p")]),
                    parameters=[
                        Parameter(
                            type=Type(typename=PQName(segments=[AutoSpecifier()])),
                            name="p",
                        )
                    ],
                    vararg=True,
                    template=TemplateDecl(
                        params=[
                            TemplateNonTypeParam(
                                type=Type(
                                    typename=PQName(segments=[NameSpecifier(name="C2")])
                                ),
                                param_idx=0,
                            )
                        ]
                    ),
                ),
            ]
        )
    )


def test_abv_template_f5() -> None:
    content = """
      void f5(const C3 auto *, C4 auto &);       // same as template<C3 T, C4 U> void f5(const T*, U&);
      void f5p(const C3 auto * p1, C4 auto &p2);
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            functions=[
                Function(
                    return_type=Type(
                        typename=PQName(segments=[FundamentalSpecifier(name="void")])
                    ),
                    name=PQName(segments=[NameSpecifier(name="f5")]),
                    parameters=[
                        Parameter(
                            type=Pointer(
                                ptr_to=Type(
                                    typename=PQName(
                                        segments=[AutoSpecifier()],
                                    ),
                                    const=True,
                                )
                            )
                        ),
                        Parameter(
                            type=Reference(
                                ref_to=Type(typename=PQName(segments=[AutoSpecifier()]))
                            )
                        ),
                    ],
                    template=TemplateDecl(
                        params=[
                            TemplateNonTypeParam(
                                type=Type(
                                    typename=PQName(
                                        segments=[NameSpecifier(name="C3")]
                                    ),
                                ),
                                param_idx=0,
                            ),
                            TemplateNonTypeParam(
                                type=Type(
                                    typename=PQName(segments=[NameSpecifier(name="C4")])
                                ),
                                param_idx=1,
                            ),
                        ]
                    ),
                ),
                Function(
                    return_type=Type(
                        typename=PQName(segments=[FundamentalSpecifier(name="void")])
                    ),
                    name=PQName(segments=[NameSpecifier(name="f5p")]),
                    parameters=[
                        Parameter(
                            type=Pointer(
                                ptr_to=Type(
                                    typename=PQName(
                                        segments=[AutoSpecifier()],
                                    ),
                                    const=True,
                                )
                            ),
                            name="p1",
                        ),
                        Parameter(
                            type=Reference(
                                ref_to=Type(typename=PQName(segments=[AutoSpecifier()]))
                            ),
                            name="p2",
                        ),
                    ],
                    template=TemplateDecl(
                        params=[
                            TemplateNonTypeParam(
                                type=Type(
                                    typename=PQName(
                                        segments=[NameSpecifier(name="C3")]
                                    ),
                                ),
                                param_idx=0,
                            ),
                            TemplateNonTypeParam(
                                type=Type(
                                    typename=PQName(segments=[NameSpecifier(name="C4")])
                                ),
                                param_idx=1,
                            ),
                        ]
                    ),
                ),
            ]
        )
    )


def test_returned_abv_template() -> None:
    content = """
      constexpr std::signed_integral auto FloorDiv(std::signed_integral auto x,
                                                   std::signed_integral auto y);
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
                                NameSpecifier(name="signed_integral"),
                            ]
                        )
                    ),
                    name=PQName(segments=[NameSpecifier(name="FloorDiv")]),
                    parameters=[
                        Parameter(
                            type=Type(typename=PQName(segments=[AutoSpecifier()])),
                            name="x",
                        ),
                        Parameter(
                            type=Type(typename=PQName(segments=[AutoSpecifier()])),
                            name="y",
                        ),
                    ],
                    constexpr=True,
                    template=TemplateDecl(
                        params=[
                            TemplateNonTypeParam(
                                type=Type(
                                    typename=PQName(
                                        segments=[
                                            NameSpecifier(name="std"),
                                            NameSpecifier(name="signed_integral"),
                                        ]
                                    )
                                ),
                                param_idx=-1,
                            ),
                            TemplateNonTypeParam(
                                type=Type(
                                    typename=PQName(
                                        segments=[
                                            NameSpecifier(name="std"),
                                            NameSpecifier(name="signed_integral"),
                                        ]
                                    )
                                ),
                                param_idx=0,
                            ),
                            TemplateNonTypeParam(
                                type=Type(
                                    typename=PQName(
                                        segments=[
                                            NameSpecifier(name="std"),
                                            NameSpecifier(name="signed_integral"),
                                        ]
                                    )
                                ),
                                param_idx=1,
                            ),
                        ]
                    ),
                )
            ]
        )
    )
