import re
import os

from cxxheaderparser.cxxheaderparser.types import Reference, Pointer, FundamentalSpecifier

class HypAttributeType:
    NIL = 0
    STRING = 1
    INT = 2
    DOUBLE = 3
    BOOL = 4

# map c++ types to c# types - (type, is_primitive) - non primitive types need to use generic GetValue() method
CXX_TO_CSHARP_TYPE_MAPPING = {
    'int': ('int', True),
    'float': ('float', True),
    'double': ('double', True),
    'bool': ('bool', True),
    'char': ('char', True),
    'unsigned char': ('byte', True),
    'short': ('short', True),
    'unsigned short': ('ushort', True),
    'long': ('long', True),
    'unsigned long': ('ulong', True),
    'long long': ('long', True),
    'unsigned long long': ('ulong', True),
    'uint8': ('byte', True),
    'uint16': ('ushort', True),
    'uint32': ('uint', True),
    'uint64': ('ulong', True),
    'int8': ('sbyte', True),
    'int16': ('short', True),
    'int32': ('int', True),
    'int64': ('long', True),
    'size_t': ('ulong', True),
    'void': ('void', True),
    'std::string': ('string', True),
    'String': ('string', True),
    'ANSIString': ('string', True),
    'UTF8StringView': ('string', True),
    'ANSIStringView': ('string', True),
    'FilePath': ('string', True),
    'ByteBuffer': ('byte[]', True),
    'ID': ('IDBase', True),
    'NodeProxy': ('Node', False),
    'Node': ('Node', False)
}

### Parse a c++ member function declaration (may contain body)
def parse_cxx_member_function_name(fn):
    pattern = r"([A-Za-z0-9_-]+)\s*\(.*\)"
    match = re.search(pattern, fn)

    if match:
        return match.group(1)
    
    return None

def parse_cxx_field_name(decl):
    decl = decl.strip().rstrip(';')
    
    pattern = re.compile(r'(\w+)(?:\s*\[.*\])?\s*(?:=|\{|$)')
    match = pattern.search(decl)
    
    if match:
        return match.group(1)
    else:
        return None

def get_generated_path(filepath, output_dir, extension='inl'):
    name_without_extension = os.path.splitext(filepath)[0]
    generated_filename = f'{name_without_extension}.{extension}'

    return os.path.join(output_dir, generated_filename)

def extract_type(type_object):
    if isinstance(type_object, Reference):
        type_object = type_object.ref_to

    if isinstance(type_object, Pointer) and not isinstance(type_object.ptr_to, FundamentalSpecifier):
        type_object = type_object.ptr_to

    return type_object

def map_type(type_object):
    type_object = extract_type(type_object)
    
    if isinstance(type_object, Pointer):
        return 'IntPtr'

    last_segment = type_object.typename.segments[-1]
    name = last_segment.name

    if name in CXX_TO_CSHARP_TYPE_MAPPING:
        return CXX_TO_CSHARP_TYPE_MAPPING[name][0]

    if name == 'RC' and last_segment.specialization:
        return map_type(last_segment.specialization.args[0].arg)
    
    if name == 'Handle' and last_segment.specialization:
        return map_type(last_segment.specialization.args[0].arg)

    return name

def parse_attributes_string(attributes_string) -> 'list[tuple[str, str, int]]':
    if not attributes_string or len(attributes_string) == 0:
        return []
    
    inner_args = []

    in_string_literal = False
    previous_char = None
    in_value = False
    current_arg = ("", "", HypAttributeType.NIL)

    def add_attribute(current_arg: 'tuple[str, str, int]'):
        nonlocal inner_args

        hyp_attribute_name = current_arg[0].strip()
        hyp_attribute_value = current_arg[1].strip()
        hyp_attribute_type = current_arg[2]

        if hyp_attribute_type == HypAttributeType.NIL:
            if hyp_attribute_value.lower() == 'true' or hyp_attribute_value.lower() == 'false':
                hyp_attribute_type = HypAttributeType.BOOL
            elif len(hyp_attribute_value) == 0:
                # true if empty string and no type specified
                hyp_attribute_type = HypAttributeType.BOOL
                hyp_attribute_value = 'true'
            else:
                # default to string
                hyp_attribute_type = HypAttributeType.STRING

        inner_args.append((hyp_attribute_name, hyp_attribute_value, hyp_attribute_type))

    for char in attributes_string:
        if char == '\"' and previous_char != '\\':
            in_string_literal = not in_string_literal
        else:
            if not in_string_literal and char == ',':
                add_attribute(current_arg)

                current_arg = ("", "", HypAttributeType.NIL)
                in_value = False
            elif not in_string_literal and char == '=':
                in_value = True
            else:
                if in_value:
                    hyp_attribute_type = current_arg[2]

                    # if digit and current_arg is int
                    if in_string_literal:
                        hyp_attribute_type = HypAttributeType.STRING
                    elif char.isdigit() and hyp_attribute_type in (HypAttributeType.NIL, HypAttributeType.INT):
                        hyp_attribute_type = HypAttributeType.INT
                    elif char.isdigit() and hyp_attribute_type in (HypAttributeType.NIL, HypAttributeType.DOUBLE):
                        hyp_attribute_type = HypAttributeType.DOUBLE
                    elif char == '.' and hyp_attribute_type in (HypAttributeType.NIL, HypAttributeType.DOUBLE, HypAttributeType.INT):
                        hyp_attribute_type = HypAttributeType.DOUBLE

                    current_arg = (current_arg[0], current_arg[1] + char, hyp_attribute_type)
                else:
                    current_arg = (current_arg[0] + char, current_arg[1], current_arg[2])

        previous_char = char

    if len(current_arg[0]) > 0 or len(current_arg[1]) > 0:
        add_attribute(current_arg)

    return inner_args

def format_attribute_value(attribute_value: str, attribute_type: int):
    if attribute_type == HypAttributeType.NIL:
        raise ValueError('Attribute type cannot be NIL')

    if attribute_type == HypAttributeType.STRING:
        return f'"{attribute_value}"'
    elif attribute_type == HypAttributeType.BOOL:
        return attribute_value.lower()
    else:
        return attribute_value