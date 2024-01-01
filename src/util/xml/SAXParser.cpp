#include <util/xml/SAXParser.hpp>

namespace hyperion {
namespace xml {

SAXParser::SAXParser(SAXHandler *handler)
    : handler(handler)
{
}

SAXParser::Result SAXParser::Parse(const FilePath &filepath)
{
    BufferedReader<2048> reader(filepath);

    return Parse(&reader);
}

SAXParser::Result SAXParser::Parse(BufferedReader<2048> *reader)
{
    if (reader == nullptr) {
        return Result(Result::SAX_ERR, "Reader was null");
    }

    if (!reader->IsOpen()) {
        return Result(Result::SAX_ERR, "File could not be read.");
    }

    Bool is_reading = false,
        is_opening = false,
        is_closing = false,
        in_element = false,
        in_comment = false,
        in_characters = false,
        in_header = false,
        in_attributes = false,
        in_attribute_value = false,
        in_attribute_name = false;

    char last_char = '\0';
    String element_str, comment_str, value_str;
    Array<Pair<String, String>> attribs;

    // shield your eyes
    // 2022-02-09: wtf
    // 2022-04-10: still lookin' good
    // 2023-10-15: never change, baby <3
    // 2024-01-01: i'm still here, still kickin'. i love you.
    reader->ReadChars([&](char ch) {
        if (ch != '\t' && ch != '\n') {
            if (ch == '<') {
                element_str.Clear();
                in_characters = false;
                if (!value_str.Empty()) {
                    handler->Characters(value_str);
                }
                is_opening = true;
                is_reading = true;
                in_element = true;
                in_attributes = false;
                is_closing = false;
                value_str.Clear();
                attribs.Clear();
            } else if (ch == '!' && in_element) {
                in_comment = true;
                comment_str = "";
            } else if (ch == '?' && in_element) {
                in_header = true;
            } else if (ch == '/' && (in_element || (in_attributes && !in_attribute_value))) {
                is_opening = false;
                is_closing = true;
            } else if (ch == '>') {
                in_characters = true;
                if (in_comment) {
                    in_comment = false;
                    handler->Comment(comment_str);
                } else if (in_header) {
                    in_header = false;
                } else {
                    if (is_opening || last_char == '/') {
                        AttributeMap locals;

                        for (auto &attr : attribs) {
                            if (!attr.first.Empty()) {
                                locals[attr.first] = attr.second;
                            }
                        }

                        handler->Begin(element_str, locals);
                        is_opening = false;
                    }

                    if (is_closing) {
                        handler->End(element_str);
                    }

                    in_attributes = false;
                    in_element = false;
                    is_closing = false;
                    is_reading = false;

                    attribs.Clear();
                }
            } else {
                if (!in_comment && !in_header) {
                    if (is_reading) {
                        if (in_element) {
                            if (ch == ' ') {
                                in_element = false;
                                in_attributes = true;
                                attribs.PushBack({ "", "" });
                            } else {
                                element_str += ch;
                            }
                        } else if (in_attributes && is_opening) {
                            if (!in_attribute_value && ch == ' ') {
                                attribs.PushBack({ "", "" });
                            } else if (ch == '\"') {
                                in_attribute_value = !in_attribute_value;
                            } else {
                                auto &last = attribs.Back();
                                if (!in_attribute_value && ch != '=') {
                                    last.first += ch;
                                } else if (in_attribute_value) {
                                    last.second += ch;
                                }
                            }
                        }
                    } else if (in_characters) {
                        if (ch != ' ' || (last_char != ' ' && (last_char != '\n' && last_char != '<'))) {
                            value_str += ch;
                        }
                    }
                } else if (in_comment && ch != '-') {
                    comment_str += ch;
                }
            }
        }
        last_char = ch;
    });

    return Result(Result::SAX_OK);
}

} // namespace xml
} // namespace hyperion
