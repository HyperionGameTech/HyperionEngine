import pytest

from cxxheaderparser.lexer import PlyLexer, LexerTokenStream
from cxxheaderparser.tokfmt import tokfmt
from cxxheaderparser.types import Token


@pytest.mark.parametrize(
    "instr",
    [
        "int",
        "unsigned int",
        "::uint8_t",
        "void *",
        "void * *",
        "const char *",
        "const char[]",
        "void * (*)()",
        "void (*)(void * buf, int buflen)",
        "void (* fnType)(void * buf, int buflen)",
        "TypeName<int, void>& x",
        "vector<string>&",
        "std::vector<Pointer *> *",
        "Alpha::Omega",
        "Convoluted::Nested::Mixin",
        "std::function<void ()>",
        "std::shared_ptr<std::function<void (void)>>",
        "tr1::shared_ptr<SnailTemplateClass<SnailNamespace::SnailClass>>",
        "std::map<unsigned, std::pair<unsigned, SnailTemplateClass<SnailNamespace::SnailClass>>>",
        "std::is_base_of<std::random_access_iterator_tag, IteratorCategoryT>::value",
        "const char&&",
        "something<T, U>{1, 2, 3}",
        "operator-=",
        "operator[]",
        "operator*",
        "operator>=",
    ],
)
def test_tokfmt(instr: str) -> None:
    """
    Each input string is exactly what the output of tokfmt should be
    """
    toks = []
    lexer = PlyLexer("")
    lexer.input(instr)

    while True:
        tok = lexer.token()
        if not tok:
            break

        if tok.type not in LexerTokenStream._discard_types:
            toks.append(Token(tok.value, tok.type))

    assert tokfmt(toks) == instr
