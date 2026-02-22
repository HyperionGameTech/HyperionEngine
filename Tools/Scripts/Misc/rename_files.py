import glob
import os
from pathlib import Path
import inspect
import re

def find_all_files(in_path):
    # map from original filename -> array of tuple(line num, what to change to)

    include_changes: dict[str, dict[int, tuple[str, str]]] = {}
    to_rename: dict[str, Path] = {}
    all_not_found: set[str] = set()

    headers = list(glob.glob('{}/**/*.h'.format(in_path), recursive=True)) + list(glob.glob('{}/**/*.hpp'.format(in_path), recursive=True))
    sources = list(glob.glob('{}/**/*.cpp'.format(in_path), recursive=True))

    all_files = headers + sources

    for f in all_files:
        print(f)
        # if not 'descriptor_set' in f:
        #     continue

        base = os.path.basename(f)
        d = os.path.dirname(f)

        # if base[0].islower():

        #just add it anyway, if there are include changes we need this file in the list
        parts = base.split('_')
        upper_camel_case = ''

        for part in parts:
            upper_camel_case = upper_camel_case + (part[0].upper() + part[1:])

        if upper_camel_case.endswith('.h'):
            upper_camel_case = upper_camel_case.replace('.h', '.hpp')

        p = Path(d) / upper_camel_case
        # print("{}  ->  {}".format(f, p))

        to_rename[f] = p

        print("Find all includes...")

        opened_file = open(f, 'r')

        line_num = 0

        for line in opened_file.readlines():
            line_num += 1

            if not line.startswith('#include'):
                continue
            
            x = re.search("#include [<\"](.*)[>\"]", line)

            if not x:
                continue

            include_path = x[1]

            print("\tLook for include {}...".format(include_path))

            try_paths = [
                Path(d) / include_path, # relative include
                Path('src') / include_path # root level
            ]

            found_path = None
            already_exists = False
            
            for try_path in try_paths:
                print("\t\tTry path {}".format(try_path))

                if try_path.exists():
                    found_path = try_path

                    break
                # if str(try_path) in to_rename:
                #     print("\t\tAlready found path {} in list to rename".format(try_path))
                #     already_exists = True

                #     break

            if not found_path:
                all_not_found.add(include_path)

                continue

            sub_base = os.path.basename(found_path)
            sub_d = os.path.dirname(found_path)

            # if not sub_base[0].islower():
            #     continue

            parts = sub_base.split('_')
            upper_camel_case = ''

            for part in parts:
                upper_camel_case = upper_camel_case + (part[0].upper() + part[1:])

            if upper_camel_case.endswith('.h'):
                upper_camel_case = upper_camel_case.replace('.h', '.hpp')

            sub_p = Path(sub_d) / upper_camel_case

            # if not already_exists:
            #     to_rename[str(found_path)] = sub_p

            if not f in include_changes:
                include_changes[f] = {}

            new_path = include_path.replace(sub_base, upper_camel_case)#str(sub_p) #sub_p.relative_to(sub_d))

            replaced_str = re.sub(r'(#include [<\"])([\./\\A-Za-z0-9_-]*)([>\"])', '\\1{}\\3'.format(new_path), line)
            print(replaced_str)
            print(sub_p)

            include_changes[f][line_num - 1] = (include_path, replaced_str)

    return (to_rename, include_changes, all_not_found)

def perform_include_changes(to_rename: 'dict[str, Path]', include_changes: 'dict[str, dict[int, tuple[str, str]]]'):
    for key in to_rename:
        print("rename {}  -> {}".format(key, to_rename[key]))
        f = open(key, 'r')

        out_file = open(str(to_rename[key]) + '.tmp', 'w')

        if key in include_changes:
            # make all include changes

            line_num = 0

            for line in f.readlines():
                if line_num in include_changes[key]:
                    out_file.write(include_changes[key][line_num][1])
                else:
                    out_file.write(line)

                line_num += 1

        else:
            for line in f.readlines():
                out_file.write(line)

        f.close()
        os.remove(key)

        os.rename(str(to_rename[key]) + '.tmp', to_rename[key])



to_rename, include_changes, all_not_found = find_all_files('src')


print("Include changes:\n====================")

for key in include_changes:
    print("In file: {}:".format(key))

    for item in include_changes[key]:
        print("Line: {}    change:   {}   ->   {}".format(item, include_changes[key][item][0], include_changes[key][item][1]))

print("To rename:\n====================")

for k in to_rename:
    print("{}  ->  {}".format(k, to_rename[k]))

print("\nNot found:\n====================")

for item in all_not_found:
    print("{}".format(item))

if input("Perform changes? (Y/n)").lower() == 'y':
    perform_include_changes(to_rename, include_changes)