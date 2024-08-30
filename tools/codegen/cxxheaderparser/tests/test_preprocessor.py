import os
import pathlib
import pytest
import re
import shutil
import subprocess
import typing

from cxxheaderparser.options import ParserOptions, PreprocessorFunction
from cxxheaderparser import preprocessor
from cxxheaderparser.simple import (
    NamespaceScope,
    ParsedData,
    parse_file,
    parse_string,
    Include,
)
from cxxheaderparser.types import (
    FundamentalSpecifier,
    NameSpecifier,
    PQName,
    Token,
    Type,
    Value,
    Variable,
)


@pytest.fixture(params=["gcc", "msvc", "pcpp"])
def make_pp(request) -> typing.Callable[..., PreprocessorFunction]:
    param = request.param
    if param == "gcc":
        gcc_path = shutil.which("g++")
        if not gcc_path:
            pytest.skip("g++ not found")

        subprocess.run([gcc_path, "--version"])
        return preprocessor.make_gcc_preprocessor
    elif param == "msvc":
        gcc_path = shutil.which("cl.exe")
        if not gcc_path:
            pytest.skip("cl.exe not found")

        return preprocessor.make_msvc_preprocessor
    elif param == "pcpp":
        if preprocessor.pcpp is None:
            pytest.skip("pcpp not installed")
        return preprocessor.make_pcpp_preprocessor
    else:
        assert False


def test_basic_preprocessor(
    make_pp: typing.Callable[..., PreprocessorFunction]
) -> None:
    content = """
      #define X 1
      int x = X;
    """

    options = ParserOptions(preprocessor=make_pp())
    data = parse_string(content, cleandoc=True, options=options)

    assert data == ParsedData(
        namespace=NamespaceScope(
            variables=[
                Variable(
                    name=PQName(segments=[NameSpecifier(name="x")]),
                    type=Type(
                        typename=PQName(segments=[FundamentalSpecifier(name="int")])
                    ),
                    value=Value(tokens=[Token(value="1")]),
                )
            ]
        )
    )


def test_preprocessor_omit_content(
    make_pp: typing.Callable[..., PreprocessorFunction],
    tmp_path: pathlib.Path,
) -> None:
    """Ensure that content in other headers is omitted"""
    h_content = '#include "t2.h"' "\n" "int x = X;\n"
    h2_content = "#define X 2\n" "int omitted = 1;\n"

    with open(tmp_path / "t1.h", "w") as fp:
        fp.write(h_content)

    with open(tmp_path / "t2.h", "w") as fp:
        fp.write(h2_content)

    options = ParserOptions(preprocessor=make_pp())
    data = parse_file(tmp_path / "t1.h", options=options)

    assert data == ParsedData(
        namespace=NamespaceScope(
            variables=[
                Variable(
                    name=PQName(segments=[NameSpecifier(name="x")]),
                    type=Type(
                        typename=PQName(segments=[FundamentalSpecifier(name="int")])
                    ),
                    value=Value(tokens=[Token(value="2")]),
                )
            ]
        )
    )


def test_preprocessor_omit_content2(
    make_pp: typing.Callable[..., PreprocessorFunction],
    tmp_path: pathlib.Path,
) -> None:
    """
    Ensure that content in other headers is omitted while handling pcpp
    relative path quirk
    """
    h_content = '#include "t2.h"' "\n" "int x = X;\n"
    h2_content = "#define X 2\n" "int omitted = 1;\n"

    tmp_path2 = tmp_path / "l1"
    tmp_path2.mkdir()

    with open(tmp_path2 / "t1.h", "w") as fp:
        fp.write(h_content)

    with open(tmp_path2 / "t2.h", "w") as fp:
        fp.write(h2_content)

    options = ParserOptions(preprocessor=make_pp(include_paths=[str(tmp_path)]))

    # Weirdness happens here
    os.chdir(tmp_path)
    data = parse_file(tmp_path2 / "t1.h", options=options)

    assert data == ParsedData(
        namespace=NamespaceScope(
            variables=[
                Variable(
                    name=PQName(segments=[NameSpecifier(name="x")]),
                    type=Type(
                        typename=PQName(segments=[FundamentalSpecifier(name="int")])
                    ),
                    value=Value(tokens=[Token(value="2")]),
                )
            ]
        )
    )


def test_preprocessor_encoding(
    make_pp: typing.Callable[..., PreprocessorFunction], tmp_path: pathlib.Path
) -> None:
    """Ensure we can handle alternate encodings"""
    h_content = b"// \xa9 2023 someone\n" b'#include "t2.h"' b"\n" b"int x = X;\n"

    h2_content = b"// \xa9 2023 someone\n" b"#define X 3\n" b"int omitted = 1;\n"

    with open(tmp_path / "t1.h", "wb") as fp:
        fp.write(h_content)

    with open(tmp_path / "t2.h", "wb") as fp:
        fp.write(h2_content)

    options = ParserOptions(preprocessor=make_pp(encoding="cp1252"))
    data = parse_file(tmp_path / "t1.h", options=options, encoding="cp1252")

    assert data == ParsedData(
        namespace=NamespaceScope(
            variables=[
                Variable(
                    name=PQName(segments=[NameSpecifier(name="x")]),
                    type=Type(
                        typename=PQName(segments=[FundamentalSpecifier(name="int")])
                    ),
                    value=Value(tokens=[Token(value="3")]),
                )
            ]
        )
    )


@pytest.mark.skipif(preprocessor.pcpp is None, reason="pcpp not installed")
def test_preprocessor_passthru_includes(tmp_path: pathlib.Path) -> None:
    """Ensure that all #include pass through"""
    h_content = '#include "t2.h"\n'

    with open(tmp_path / "t1.h", "w") as fp:
        fp.write(h_content)

    with open(tmp_path / "t2.h", "w") as fp:
        fp.write("")

    options = ParserOptions(
        preprocessor=preprocessor.make_pcpp_preprocessor(
            passthru_includes=re.compile(".+")
        )
    )
    data = parse_file(tmp_path / "t1.h", options=options)

    assert data == ParsedData(
        namespace=NamespaceScope(), includes=[Include(filename='"t2.h"')]
    )
