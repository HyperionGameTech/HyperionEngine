import os
import re
import sys
import datetime
import json
from mako.template import Template

from cxxheaderparser.cxxheaderparser.simple import parse_string as parse_cxx_header, ParsedData
from cxxheaderparser.cxxheaderparser.preprocessor import make_gcc_preprocessor, make_msvc_preprocessor
from cxxheaderparser.cxxheaderparser.options import ParserOptions

from util import parse_cxx_field_name, get_generated_path, map_type, parse_attributes_string, format_attribute_value, extract_class_name, extract_base_classes, to_pascal_case

CXX_CLASS_TEMPLATE = Template(filename=os.path.join(os.path.dirname(__file__), 'templates', 'class_template.hpp'))
CXX_MODULE_TEMPLATE = Template(filename=os.path.join(os.path.dirname(__file__), 'templates', 'module_template.hpp'))
CXX_DUMMY_CLASS_TEMPLATE = Template(filename=os.path.join(os.path.dirname(__file__), 'templates', 'dummy_class_template.hpp'))
CSHARP_CLASS_TEMPLATE = Template(filename=os.path.join(os.path.dirname(__file__), 'templates', 'class_template.cs'))
CSHARP_MODULE_TEMPLATE = Template(filename=os.path.join(os.path.dirname(__file__), 'templates', 'module_template.cs'))

CXX_PREAMBLE = """
#define HYP_API
#define HYP_CLASS(...)
#define HYP_STRUCT(...)
#define HYP_ENUM(...)
#define HYP_FIELD(...)
#define HYP_PROPERTY(...)
#define HYP_METHOD(...)
#define HYP_OBJECT_BODY(...)
#define HYP_FORCE_INLINE inline
#define HYP_NODISCARD
#define HYP_DEPRECATED
""".rstrip()

def make_preprocessor(**kwargs):
    if os.name == 'nt':
        return make_msvc_preprocessor(**kwargs)
    else:
        return make_gcc_preprocessor(**kwargs)

class SourceLocation:
    def __init__(self, file, index):
        self.file = file
        self.index = index

class HypClassType:
    CLASS = 0
    STRUCT = 1,
    ENUM = 2

class HypMemberType:
    FIELD = 0
    PROPERTY = 1
    METHOD = 2,
    ENUMERATOR = 3

class GeneratedSource:
    def __init__(self, source_location, content=""):
        self.source_location = source_location
        self.content = content

class CodegenState:
    def __init__(self, src_dir: str, output_dir: str):
        self.src_dir = src_dir
        self.output_dir = output_dir

        self.class_builder = None

        self.errors = []

    @property
    def is_success(self):
        return len(self.errors) == 0
    
    @property
    def dotnet_output_dir(self):
        return os.path.join(self.src_dir, 'dotnet', 'runtime', 'gen')

    @property
    def metadata_path(self):
        return os.path.join(self.output_dir, 'metadata.json')

    def add_error(self, error):
        self.errors.append(error)

class CodegenClassBuilder:
    def __init__(self, state: CodegenState):
        self.state = state

        self.header_files = []
        self.source_files = []
        self.hyp_classes = []

        self.metadata = {}

        self.cxx_generated_sources = {}
        self.csharp_generated_sources = {}

    def run(self):
        self.load_metadata()

        self.build_hyp_classes()

        for hyp_class in self.hyp_classes:
            self.build_hyp_class(hyp_class)
            
        if self.state.is_success:
            self.update_metadata()
            self.write_generated_sources()
            self.write_metadata()

    def load_metadata(self):
        metadata_path = self.state.metadata_path
        metadata_dir = os.path.dirname(metadata_path)

        if not os.path.exists(metadata_dir):
            os.path.makedirs(metadata_dir)

        if os.path.isfile(metadata_path):
            try:
                metadata_json = json.load(open(metadata_path, 'r'))

                self.metadata = metadata_json

                return
            except Exception as e:
                sys.stderr.write(f"Failed to load metadata json file at {metadata_path}: {repr(e)}\n")

        self.metadata = {}

    def write_metadata(self):
        metadata_path = self.state.metadata_path
        metadata_dir = os.path.dirname(metadata_path)

        if not os.path.exists(metadata_dir):
            os.path.makedirs(metadata_dir)

        with open(metadata_path, 'w') as f:
            json.dump(self.metadata, f)

    def get_hyp_class(self, class_name: str) -> 'HypClassDefinition | None':
        return next((x for x in self.hyp_classes if x.name == class_name), None)

    def map_type_with_context(self, typename: str):
        map_type_result = map_type(typename)

        # try to find a class matching that name
        hyp_class = self.get_hyp_class(map_type_result)

        if hyp_class is not None:
            return hyp_class.name
            
        return map_type_result

    def build_hyp_classes(self):
        for file, last_modified in self.header_files:
            with open(os.path.join(self.state.src_dir, file), 'r') as f:
                content = f.read()

                for class_match in re.finditer(r'HYP_(?:CLASS|STRUCT|ENUM)\s*\(\s*(.*?)\s*\)', content):
                    class_type = HypClassType.CLASS

                    if 'HYP_STRUCT' in class_match.group(0):
                        class_type = HypClassType.STRUCT
                    elif 'HYP_ENUM' in class_match.group(0):
                        class_type = HypClassType.ENUM

                    source_location = SourceLocation(file, class_match.start(0))

                    # grab most immediate opening brace after class definition
                    opening_brace = content.find('{', class_match.start(0))

                    if opening_brace == -1:
                        self.state.add_error(f'Error: Missing opening brace for class definition at char {class_match.start(0)} in file {file}')
                        continue
                    
                    class_content = content[class_match.start(0)-1:opening_brace + 1]

                    # get class name, base classes from class content
                    class_name = extract_class_name(class_content)
                    base_classes = extract_base_classes(class_content)

                    remaining_content = content[opening_brace+1:]

                    brace_depth = 1
                    
                    in_string = False
                    in_comment = False
                    escaped = False

                    for i in range(len(remaining_content)):
                        char = remaining_content[i]

                        class_content += char

                        if escaped:
                            escaped = False
                            continue

                        if char == '\\':
                            escaped = True
                            continue

                        if char == '"' and not in_comment:
                            in_string = not in_string

                        if char == '/' and not in_string:
                            if i+1 < len(remaining_content) and remaining_content[i+1] == '/':
                                in_comment = True
                            elif i+1 < len(remaining_content) and remaining_content[i+1] == '*':
                                in_comment = True
                                i += 1

                        if char == '\n' and in_comment:
                            in_comment = False

                        if not in_string and not in_comment:
                            if char == '{':
                                brace_depth += 1
                            elif char == '}':
                                brace_depth -= 1

                            if brace_depth <= 0:
                                class_content += ';'

                                break

                    if self.get_hyp_class(class_name) is not None:
                        self.state.add_error(f'Error: Class {class_name} already defined in file {file}')
                    else:
                        attributes = parse_attributes_string(class_match.group(1))

                        try:
                            parsed_data = parse_cxx_header(content=CXX_PREAMBLE + '\n' + class_content, options=ParserOptions(preprocessor=make_preprocessor()))

                            hyp_class = HypClassDefinition(class_type, source_location, class_name, base_classes, attributes, parsed_data, class_content)
                            hyp_class.last_modified = last_modified
                            
                            self.hyp_classes.append(hyp_class)
                        except Exception as e:
                            self.state.add_error(f'Error: Failed to parse class definition for {class_name} in file {file} - {repr(e)}')
    
    def build_hyp_class(self, hyp_class: 'HypClassDefinition'):
        if hyp_class.is_built:
            return
        
        hyp_class.parse_members(self.state)
        
        base_class = None

        # build base classes first to initialize dependencies
        for base_class_name in hyp_class.base_classes:
            existing_base_class = None

            for existing_hyp_class in self.hyp_classes:
                if existing_hyp_class.name == base_class_name:
                    existing_base_class = existing_hyp_class
                    break

            if existing_base_class is None:
                continue

            self.build_hyp_class(existing_base_class)

            if base_class is not None:
                self.state.add_error(f'Error: Multiple base classes for {hyp_class.name} - {base_class.name}, {base_class_name}')
                return

            base_class = existing_base_class

        hyp_class.base_class = base_class

        # get generated path - should match structure of hyp class
        # get extension of hyp class file
        
        cxx_generated_path = get_generated_path(hyp_class.location.file, self.state.output_dir, extension='generated.cpp')
        cxx_generated_dir = os.path.dirname(cxx_generated_path)

        if not os.path.exists(cxx_generated_dir):
            os.makedirs(cxx_generated_dir)

        if hyp_class.last_modified is None or self.metadata.get(hyp_class.name, {}).get("last_modified", 0) < hyp_class.last_modified:
            cxx_source = self.cxx_generated_sources.get(cxx_generated_path, GeneratedSource(hyp_class.location))
            cxx_source.content += CXX_CLASS_TEMPLATE.render(hyp_class=hyp_class, HypMemberType=HypMemberType, HypClassType=HypClassType, format_attribute_value=format_attribute_value, to_pascal_case=to_pascal_case)
            self.cxx_generated_sources.update({cxx_generated_path: cxx_source})
        else:
            sys.stdout.write(f'Skipping C++ codegen for {hyp_class.name} - up to date\n')

        csharp_generated_path = get_generated_path(hyp_class.location.file, self.state.dotnet_output_dir, extension='cs')
        csharp_generated_dir = os.path.dirname(csharp_generated_path)

        if not os.path.exists(csharp_generated_dir):
            os.makedirs(csharp_generated_dir)
        
        if hyp_class.last_modified is None or self.metadata.get(hyp_class.name, {}).get("last_modified", 0) < hyp_class.last_modified:
            csharp_source = self.csharp_generated_sources.get(csharp_generated_path, GeneratedSource(hyp_class.location))
            csharp_source.content += CSHARP_CLASS_TEMPLATE.render(hyp_class=hyp_class, HypMemberType=HypMemberType, HypClassType=HypClassType)
            self.csharp_generated_sources.update({csharp_generated_path: csharp_source})
        else:
            sys.stdout.write(f'Skipping C# codegen for {hyp_class.name} - up to date\n')

        hyp_class.is_built = True
    
    def write_generated_sources(self):
        for path, generated_source in self.cxx_generated_sources.items():
            module_content = CXX_MODULE_TEMPLATE.render(generated_source=generated_source)

            with open(path, 'w') as f:
                f.write(module_content)

        for path, generated_source in self.csharp_generated_sources.items():
            module_content = CSHARP_MODULE_TEMPLATE.render(content=generated_source.content)

            with open(path, 'w') as f:
                f.write(module_content)

    def update_metadata(self):
        for hyp_class in self.hyp_classes:
            self.metadata.update({ hyp_class.name: self.metadata.get(hyp_class.name, {}) })
            self.metadata.get(hyp_class.name).update({ "last_modified": hyp_class.last_modified })

class HypMemberDefinition:
    def __init__(self, member_type: int, name: str, attributes: 'list[tuple[str, str, int]]'=[], method_return_type: 'str | None'=None, method_args: 'list[tuple[str, str, str]]'=None, is_const_method: bool=False, property_args: 'list[str] | None'=None):
        self.member_type = member_type
        self.name = name
        self.attributes = attributes
        self.method_return_type = method_return_type
        self.method_args = method_args
        self.is_const_method = is_const_method
        self.property_args = property_args

        if self.is_method:
            if self.method_return_type is None:
                raise Exception('Method return type is required')
            
            if self.method_args is None:
                raise Exception('Method arguments are required')
        else:
            if self.method_return_type is not None or self.method_args is not None:
                raise Exception('Method return type and arguments are only allowed for methods')
            
        if self.property_args is not None and not self.is_property:
            raise Exception('Property arguments are only allowed for properties')

    @property
    def is_field(self):
        return self.member_type == HypMemberType.FIELD
    
    @property
    def is_property(self):
        return self.member_type == HypMemberType.PROPERTY
    
    @property
    def is_method(self):
        return self.member_type == HypMemberType.METHOD
    
    @property
    def is_enumerator(self):
        return self.member_type == HypMemberType.ENUMERATOR
    
    @property
    def return_type(self):
        if self.is_method:
            if self.method_return_type is None:
                return 'void'
            
            return self.method_return_type
        
        raise Exception('Member is not a method')
            
    @property
    def args(self) -> 'list[tuple[str, str, str]]':
        if self.is_method:
            if self.method_args is None:
                return []
            
            return self.method_args
        elif self.is_property:
            if self.property_args is None:
                return []
            
            return self.property_args
        
        raise Exception('Member is not a method')
    
    def get_attribute(self, attribute_name: str):
        for name, value, attr_type in self.attributes:
            if name.lower() == attribute_name.lower():
                return value

        return None

    def has_attribute(self, attribute_name):
        return self.get_attribute(attribute_name) is not None

class HypClassDefinition:
    def __init__(self, class_type: int, location: SourceLocation, name: str, base_classes: 'list[str]', attributes: 'list[tuple[str, str, int]]', parsed_data: ParsedData, content: str):
        self.class_type = class_type
        self.location = location
        self.name = name
        self.base_classes = base_classes
        self.attributes = attributes
        self.parsed_data = parsed_data
        self.content = content
        self.members = []

        self.base_class = None

        self.last_modified = None

        self.is_built = False

    @property
    def is_class(self):
        return self.class_type == HypClassType.CLASS
    
    @property
    def is_struct(self):
        return self.class_type == HypClassType.STRUCT

    @property
    def is_class_or_struct(self):
        return self.is_class or self.is_struct
    
    @property
    def is_enum(self):
        return self.class_type == HypClassType.ENUM
    
    @property
    def has_scriptable_methods(self):
        for member in self.members:
            if member.is_method and member.has_attribute('Scriptable'):
                return True
            
        return False

    def get_attribute(self, attribute_name: str):
        for name, value, attr_type in self.attributes:
            if name.lower() == attribute_name.lower():
                return value

        return None

    def has_attribute(self, attribute_name):
        return self.get_attribute(attribute_name) is not None

    def parse_members(self, state):
        class_data = None

        if self.is_enum:
            class_data = self.parsed_data.namespace.enums[0]

            for enum_value in class_data.values:
                self.members.append(HypMemberDefinition(HypMemberType.ENUMERATOR, enum_value.name, attributes=[]))

            return
        
        if self.is_class_or_struct:
            class_data = self.parsed_data.namespace.classes[0]

        if not class_data:
            state.add_error(f'Error: Failed to parse class data for {self.name}')
            return
        
        # for method in class_data.methods:
        #     method_name = method.name.segments[-1].name
            
        #     print(f'Method: {method_name} = {method}')
        
        # find matches for fields, properties, methods (via regex, HYP_FIELD, HYP_PROPERTY, HYP_METHOD)
        # print(f"Content: {self.content}")
        matches = re.finditer(r'HYP_(FIELD|PROPERTY|METHOD)(?:\((.*)\))?', self.content)

        for match in matches:
            inner_args: 'list[tuple[str, str, int]]' = []

            if match.group(2):
                inner_args = parse_attributes_string(match.group(2))

            if match.group(1) == 'PROPERTY':
                member_type = HypMemberType.PROPERTY

                # HYP_PROPERTY macro should contain property name, then (getter and setter (optional)) OR (a pointer to data member)

                if len(inner_args) == 0 or len(inner_args[0]) == 0:
                    state.add_error("Error: Missing property name")
                    continue

                property_name = inner_args[0][0]

                if not property_name:
                    state.add_error("Error: Missing property name")
                    continue

                property_args = [x.strip() for x in match.group(2).split(',')[1:]]

                print("Property args:", property_args)

                self.members.append(HypMemberDefinition(member_type, property_name, attributes=[], property_args=property_args))
            else:
                remaining_content = self.content[match.end(0):]
                member_content = ""
                
                member_type = None

                in_string = False
                in_comment = False
                brace_depth = 0
                escaped = False

                for i in range(len(remaining_content)):
                    char = remaining_content[i]

                    member_content += char

                    if escaped:
                        escaped = False
                        continue

                    if char == '\\':
                        escaped = True
                        continue

                    if char == '"' and not in_comment:
                        in_string = not in_string

                    if char == '/' and not in_string:
                        if i+1 < len(remaining_content) and remaining_content[i+1] == '/':
                            in_comment = True
                        elif i+1 < len(remaining_content) and remaining_content[i+1] == '*':
                            in_comment = True
                            i += 1

                    if char == '\n' and in_comment:
                        in_comment = False

                    if char == '{' and not in_string and not in_comment:
                        brace_depth += 1

                    if char == '}' and not in_string and not in_comment:
                        brace_depth -= 1

                        if brace_depth == 0:
                            break

                    if char == ';' and not in_string and not in_comment and brace_depth == 0:
                        break

                member_content = member_content.strip()

                if match.group(1) == 'FIELD':
                    member_type = HypMemberType.FIELD

                    member_name = parse_cxx_field_name(member_content)

                    if not member_name:
                        state.add_error(f'Error: Failed to parse field name for {member_content}')
                        continue

                    self.members.append(HypMemberDefinition(member_type, member_name, attributes=inner_args))
                elif match.group(1) == 'METHOD':
                    member_type = HypMemberType.METHOD

                dummy_content = CXX_DUMMY_CLASS_TEMPLATE.render(content=member_content)

                try:
                    parsed_data = parse_cxx_header(content=CXX_PREAMBLE + '\n' + dummy_content, options=ParserOptions(preprocessor=make_preprocessor()))

                    member = None
                    
                    if member_type == HypMemberType.FIELD:
                        member = parsed_data.namespace.classes[0].fields[0]

                        # TODO
                    elif member_type == HypMemberType.METHOD:
                        member = parsed_data.namespace.classes[0].methods[0]

                        member_name = member.name.segments[-1].name

                        print(f"Method {member_name} - {member}")

                        is_const_method = member.const

                        return_type = state.class_builder.map_type_with_context(member.return_type)

                        args: 'list[tuple[str, str, str]]' = []

                        for param in member.parameters:
                            param_type = state.class_builder.map_type_with_context(param.type)
                            param_name = param.name

                            if not param_name:
                                state.add_error(f'Error: Missing name for parameter {param_type}')
                                continue

                            args.append((param_type, param_name, param.type.format_decl(param.name)))

                        # if isinstance(return_type, Reference):
                        #     return_type = return_type.ref_to

                        # if isinstance(return_type, Pointer):
                        #     return_type = return_type.ptr_to

                        # return_type

                        self.members.append(HypMemberDefinition(member_type, member_name, attributes=inner_args, method_return_type=return_type, method_args=args, is_const_method=is_const_method))
                    else:
                        raise Exception(f'Invalid member type {member_type}')
                
                except Exception as e:
                    state.add_error(f'Error: Failed to parse member definition for {member_content} - {repr(e)}')

class Codegen:
    def __init__(self, state):
        self.state = state

        self.class_builder = CodegenClassBuilder(state)
        self.state.class_builder = self.class_builder

    def run(self):
        self.make_output_dir()

        self.read_input_files()

        self.class_builder.run()

    def make_output_dir(self):
        if not os.path.exists(self.state.output_dir):
            os.makedirs(self.state.output_dir)

    
    def read_input_files(self):
        ignore_paths = [
            os.path.join(self.state.src_dir, "generated"),
            os.path.join(self.state.src_dir, "core", "object"),
            os.path.join(self.state.src_dir, "core", "Defines.hpp"),
        ]

        for root, dirs, files in os.walk(self.state.src_dir):
            for file in files:
                filepath = os.path.relpath(os.path.join(root, file), self.state.src_dir)
                last_modified = os.path.getmtime(os.path.join(root, file))

                if file.endswith('.hpp') and all([(ignore_path not in filepath and filepath not in ignore_path) for ignore_path in ignore_paths]):
                    self.class_builder.header_files.append((filepath, last_modified))
                elif file.endswith('.cpp') and all([(ignore_path not in filepath and filepath not in ignore_path) for ignore_path in ignore_paths]):
                    self.class_builder.source_files.append((filepath, last_modified))