#include "sax_parser.h"

namespace hyperion {
namespace xml {

SaxParser::SaxParser(SaxHandler *handler)
    : handler(handler)
{
}

SaxParser::Result SaxParser::Parse(const std::string &filepath)
{
    file.open(filepath);
    if (!file.is_open()) {
        return SaxParser::Result(SaxParser::Result::SAX_ERR, "File could not be read.");
    }

    bool is_reading = false,
         is_opening = false,
         is_closing = false,
         in_element = false,
         in_comment = false,
         in_characters = false,
         in_header = false,
         in_attributes = false,
         in_attribute_value = false,
         in_attribute_name = false;

    char last_char;
    std::string element_str, comment_str, value_str;
    std::vector<std::pair<std::string, std::string>> attribs;

    // shield your eyes
    char ch;
    while (file.get(ch)) {
        if (ch != '\t' && ch != '\n') {
            if (ch == '<') {
                element_str.clear();
                in_characters = false;
                if (!value_str.empty()) {
                    handler->Characters(value_str);
                }
                is_opening = true;
                is_reading = true;
                in_element = true;
                in_attributes = false;
                is_closing = false;
                value_str.clear();
                attribs.clear();
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
                            if (!attr.first.empty()) {
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

                    attribs.clear();
                }
            } else {
                if (!in_comment && !in_header) {
                    if (is_reading) {
                        if (in_element) {
                            if (ch == ' ') {
                                in_element = false;
                                in_attributes = true;
                                attribs.push_back({ "", "" });
                            } else {
                                element_str += ch;
                            }
                        } else if (in_attributes && is_opening) {
                            if (!in_attribute_value && ch == ' ') {
                                attribs.push_back({ "", "" });
                            } else if (ch == '\"') {
                                in_attribute_value = !in_attribute_value;
                            } else {
                                auto &last = attribs.back();
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
    }

    return SaxParser::Result(SaxParser::Result::SAX_OK);
}

} // namespace xml
} // namespace hyperion
