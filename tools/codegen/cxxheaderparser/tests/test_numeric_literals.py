# Note: testcases generated via `python -m cxxheaderparser.gentest`

from cxxheaderparser.types import (
    FundamentalSpecifier,
    NameSpecifier,
    PQName,
    Token,
    Type,
    Value,
    Variable,
)
from cxxheaderparser.simple import (
    Include,
    NamespaceScope,
    Pragma,
    parse_string,
    ParsedData,
)


def test_numeric_literals() -> None:
    content = """
      #pragma once
      #include <cstdint>

      int test_binary = 0b01'10'01;
      int test_decimal = 123'456'789u;
      int test_octal = 012'42'11l;
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            variables=[
                Variable(
                    name=PQName(segments=[NameSpecifier(name="test_binary")]),
                    type=Type(
                        typename=PQName(segments=[FundamentalSpecifier(name="int")])
                    ),
                    value=Value(tokens=[Token(value="0b01'10'01")]),
                ),
                Variable(
                    name=PQName(segments=[NameSpecifier(name="test_decimal")]),
                    type=Type(
                        typename=PQName(segments=[FundamentalSpecifier(name="int")])
                    ),
                    value=Value(tokens=[Token(value="123'456'789u")]),
                ),
                Variable(
                    name=PQName(segments=[NameSpecifier(name="test_octal")]),
                    type=Type(
                        typename=PQName(segments=[FundamentalSpecifier(name="int")])
                    ),
                    value=Value(tokens=[Token(value="012'42'11l")]),
                ),
            ]
        ),
        pragmas=[Pragma(content=Value(tokens=[Token(value="once")]))],
        includes=[Include(filename="<cstdint>")],
    )
