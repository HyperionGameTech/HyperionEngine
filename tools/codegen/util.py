import re
import os

from cxxheaderparser.cxxheaderparser.types import Reference, Pointer

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

    if isinstance(type_object, Pointer):
        type_object = type_object.ptr_to

    return type_object

def map_type(type_object):
    type_object = extract_type(type_object)

    last_segment = type_object.typename.segments[-1]
    name = last_segment.name

    if name == 'RC' and last_segment.specialization:
        return map_type(last_segment.specialization.args[0].arg)
    
    if name == 'Handle' and last_segment.specialization:
        return map_type(last_segment.specialization.args[0].arg)
    
    if name == "ID":
        return "IDBase"

    return name