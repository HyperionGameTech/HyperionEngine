import glob
import os
from pathlib import Path
import inspect

license_text = """Copyright (c) 2024 No Tomorrow Games. All rights reserved."""

def apply_license(base_path, exclude=[]):
    global license_text

    headers = glob.glob('{0}/**/*.hpp'.format(base_path), recursive = True)
    sources = glob.glob('{0}/**/*.cpp'.format(base_path), recursive = True)
    all_sources = headers + sources

    for item in all_sources:
        if any([ex in item for ex in exclude]):
            continue

        f = open(item, 'r+')
        
        # match for existing license by checking for /* \license and * \endlicense */
        license_start = -1
        license_end = -1

        for i, line in enumerate(f):
            # if '/*** ' in line:
            #     license_start = i
            # if ' ***/' in line:
            #     license_end = i
            if license_text in line:
                license_start = i
                license_end = i
                break
                
        f.seek(0)
        lines = f.readlines()
        f.seek(0)
        f.truncate()

        f.seek(0)
        f.write("/* " + license_text.replace('\n', '\n *** ') + " */")

        if license_start != -1 and license_end != -1:
            f.writelines(lines[:license_start] + lines[license_end+1:])
        else:
            f.write("\n\n")
            f.writelines(lines)

        f.close()

apply_license('src/', exclude=['stb_image', 'VmaUsage'])