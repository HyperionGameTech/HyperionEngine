"""Narrows an engine asset list recorded with --record-engine-assets to one shader target.

    python FilterEngineAssetList.py <recorded list> <engine content dir> <backend> <target> <output list>

Loading a shader bundle loads every variant of it that is on disk, for every backend, so a recorded list names all of
them. A cook for one platform only wants its own: for the browser, backend WEBGPU and target WEB.
"""

import os
import re
import sys

recorded_path, engine_content_dir, backend, target, output_path = sys.argv[1:6]

property_pattern = re.compile(r"ShaderProperty (BACKEND|TARGET) \{\s*Flags = \w+\s*Value = (\w+)")

kept = []
seen = set()
num_shaders_dropped = 0

with open(recorded_path, "r", encoding="utf-8") as file:
    for line in file:
        entry = line.strip()

        if not entry or entry in seen:
            continue

        seen.add(entry)

        bucket, _, name = entry.partition("/")

        if bucket == "Shaders":
            manifest_path = os.path.join(engine_content_dir, "Shaders", name + ".hmf")

            if not os.path.isfile(manifest_path):
                num_shaders_dropped += 1
                continue

            with open(manifest_path, "r", encoding="utf-8", errors="replace") as manifest:
                properties = dict(property_pattern.findall(manifest.read(4000)))

            if properties.get("BACKEND") != backend or properties.get("TARGET") != target:
                num_shaders_dropped += 1
                continue

        kept.append(entry)

with open(output_path, "w", encoding="utf-8", newline="\n") as file:
    file.write("\n".join(kept) + "\n")

print("kept %d of %d entries, dropped %d shader variants for other targets" % (len(kept), len(seen), num_shaders_dropped))
