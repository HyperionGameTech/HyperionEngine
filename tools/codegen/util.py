import re
import os

from cxxheaderparser.cxxheaderparser.types import Reference, Pointer, FundamentalSpecifier

CXX_TO_CSHARP_TYPE_MAPPING = {
    'int': 'int',
    'float': 'float',
    'double': 'double',
    'bool': 'bool',
    'char': 'char',
    'unsigned char': 'byte',
    'short': 'short',
    'unsigned short': 'ushort',
    'long': 'long',
    'unsigned long': 'ulong',
    'long long': 'long',
    'unsigned long long': 'ulong',
    'uint8': 'byte',
    'uint16': 'ushort',
    'uint32': 'uint',
    'uint64': 'ulong',
    'int8': 'sbyte',
    'int16': 'short',
    'int32': 'int',
    'int64': 'long',
    'size_t': 'UIntPtr',
    'void': 'void',
    'std::string': 'string',
    'String': 'string',
    'UTF8StringView': 'string',
    'ANSIStringView': 'string',
    'FilePath': 'string',
    'ID': 'IDBase',
    'NodeProxy': 'Node',
    'Node': 'Node'
}

### Parse a c++ member function declaration (may contain body)
def parse_cxx_member_function_name(fn):
    pattern = r"([A-Za-z0-9_-]+)\s*\(.*\)"
    match = re.search(pattern, fn)

    if match:
        return match.group(1)
    
    return None

def parse_cxx_field_name(decl):
    pattern = r"([A-Za-z0-9_-]+)\s*;"
    match = re.search(pattern, decl)

    if match:
        return match.group(1)
    
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
        return CXX_TO_CSHARP_TYPE_MAPPING[name]

    if name == 'RC' and last_segment.specialization:
        return map_type(last_segment.specialization.args[0].arg)
    
    if name == 'Handle' and last_segment.specialization:
        return map_type(last_segment.specialization.args[0].arg)

    return name