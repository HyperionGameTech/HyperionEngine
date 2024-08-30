import typing

import pytest

from cxxheaderparser.tokfmt import Token
from cxxheaderparser.types import (
    Array,
    DecoratedType,
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
    TemplateSpecialization,
    TemplateDecl,
    Type,
    Value,
)


@pytest.mark.parametrize(
    "pytype,typestr,declstr",
    [
        (
            Type(typename=PQName(segments=[FundamentalSpecifier(name="int")])),
            "int",
            "int name",
        ),
        (
            Type(
                typename=PQName(segments=[FundamentalSpecifier(name="int")]), const=True
            ),
            "const int",
            "const int name",
        ),
        (
            Type(
                typename=PQName(segments=[NameSpecifier(name="S")], classkey="struct")
            ),
            "struct S",
            "struct S name",
        ),
        (
            Pointer(
                ptr_to=Type(
                    typename=PQName(segments=[FundamentalSpecifier(name="int")])
                )
            ),
            "int*",
            "int* name",
        ),
        (
            Pointer(
                ptr_to=Pointer(
                    ptr_to=Type(
                        typename=PQName(segments=[FundamentalSpecifier(name="int")])
                    )
                )
            ),
            "int**",
            "int** name",
        ),
        (
            Reference(
                ref_to=Type(
                    typename=PQName(segments=[FundamentalSpecifier(name="int")])
                )
            ),
            "int&",
            "int& name",
        ),
        (
            Reference(
                ref_to=Array(
                    array_of=Type(
                        typename=PQName(segments=[FundamentalSpecifier(name="int")])
                    ),
                    size=Value(tokens=[Token(value="3")]),
                )
            ),
            "int (&)[3]",
            "int (& name)[3]",
        ),
        (
            MoveReference(
                moveref_to=Type(
                    typename=PQName(
                        segments=[NameSpecifier(name="T"), NameSpecifier(name="T")]
                    )
                )
            ),
            "T::T&&",
            "T::T&& name",
        ),
        (
            Pointer(
                ptr_to=Array(
                    array_of=Type(
                        typename=PQName(segments=[FundamentalSpecifier(name="int")])
                    ),
                    size=Value(tokens=[Token(value="3")]),
                )
            ),
            "int (*)[3]",
            "int (* name)[3]",
        ),
        (
            Pointer(
                ptr_to=Array(
                    array_of=Type(
                        typename=PQName(segments=[FundamentalSpecifier(name="int")])
                    ),
                    size=Value(tokens=[Token(value="3")]),
                ),
                const=True,
            ),
            "int (* const)[3]",
            "int (* const name)[3]",
        ),
        (
            FunctionType(
                return_type=Type(
                    typename=PQName(segments=[FundamentalSpecifier(name="int")])
                ),
                parameters=[
                    Parameter(
                        type=Type(
                            typename=PQName(segments=[FundamentalSpecifier(name="int")])
                        )
                    )
                ],
            ),
            "int (int)",
            "int name(int)",
        ),
        (
            FunctionType(
                return_type=Type(
                    typename=PQName(segments=[FundamentalSpecifier(name="int")])
                ),
                parameters=[
                    Parameter(
                        type=Type(
                            typename=PQName(segments=[FundamentalSpecifier(name="int")])
                        )
                    )
                ],
                has_trailing_return=True,
            ),
            "auto (int) -> int",
            "auto name(int) -> int",
        ),
        (
            FunctionType(
                return_type=Type(
                    typename=PQName(
                        segments=[FundamentalSpecifier(name="void")],
                    ),
                ),
                parameters=[
                    Parameter(
                        type=Type(
                            typename=PQName(
                                segments=[FundamentalSpecifier(name="int")],
                            ),
                        ),
                        name="a",
                    ),
                    Parameter(
                        type=Type(
                            typename=PQName(
                                segments=[FundamentalSpecifier(name="int")],
                            ),
                        ),
                        name="b",
                    ),
                ],
            ),
            "void (int a, int b)",
            "void name(int a, int b)",
        ),
        (
            Pointer(
                ptr_to=FunctionType(
                    return_type=Type(
                        typename=PQName(segments=[FundamentalSpecifier(name="int")])
                    ),
                    parameters=[
                        Parameter(
                            type=Type(
                                typename=PQName(
                                    segments=[FundamentalSpecifier(name="int")]
                                )
                            )
                        )
                    ],
                )
            ),
            "int (*)(int)",
            "int (* name)(int)",
        ),
        (
            Type(
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
                                                        FundamentalSpecifier(name="int")
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
            "std::function<int (int)>",
            "std::function<int (int)> name",
        ),
        (
            Type(
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
                                                    NameSpecifier(name=""),
                                                    NameSpecifier(name="T"),
                                                ],
                                            )
                                        ),
                                    )
                                ]
                            ),
                        )
                    ]
                ),
            ),
            "foo<::T>",
            "foo<::T> name",
        ),
        (
            Type(
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
                                                    NameSpecifier(name=""),
                                                    NameSpecifier(name="T"),
                                                ],
                                                has_typename=True,
                                            )
                                        ),
                                    )
                                ]
                            ),
                        )
                    ]
                ),
            ),
            "foo<typename ::T>",
            "foo<typename ::T> name",
        ),
    ],
)
def test_typefmt(
    pytype: typing.Union[DecoratedType, FunctionType], typestr: str, declstr: str
):
    # basic formatting
    assert pytype.format() == typestr

    # as a type declaration
    assert pytype.format_decl("name") == declstr
