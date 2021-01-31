import glob
import os
import re
import subprocess

LENGTH_LOOKUP = {
    "VEH_COMPANY_END": 4,
    "ZOOM_LVL_COUNT": 6,
    "NETLANG_COUNT": 36,
    "MAX_COMPANIES": 15,
    "WID_TN_END": 31,
}

strings_defined = []
strings_found = set()

skip = 0
length = 0
prefix = ""
with open("src/lang/english.txt") as fp:
    for line in fp.readlines():
        if not line.strip():
            continue

        if line[0] == "#":
            comment = line.strip()

            if comment.startswith("##length "):
                length = comment.split(" ")[1].strip()

                if length.isnumeric():
                    length = int(length)
                else:
                    length = LENGTH_LOOKUP[length]

                skip = 1
        else:
            name = line.split(":")[0].strip()
            strings_defined.append(name)

            if skip == 1:
                skip = 0
                length -= 1
                prefix = name
            elif length > 0:
                strings_found.add(name)
                length -= 1

                # Find the common prefix of these strings
                for i in range(len(prefix)):
                    if prefix[0:i+1] != name[0:i+1]:
                        prefix = prefix[0:i]
                        break

                if length == 0:
                    if len(prefix) < 6:
                        print(f"WARNING: prefix of block including {name} was reduced to {prefix}")
            elif prefix:
                if name.startswith(prefix):
                    print(f"WARNING: {name} looks a lot like block above with prefix {prefix}")
                prefix = ""


strings_defined = sorted(strings_defined)


def scan_files(path):
    for new_path in glob.glob(f"{path}/*"):
        if os.path.isdir(new_path):
            scan_files(new_path)
            continue

        if not new_path.endswith((".c", ".h", ".cpp", ".hpp", ".ini")):
            continue

        if new_path == "src/table/cargo_const.h":
            p = subprocess.run(["g++", "-E", new_path], stdout=subprocess.PIPE)
            output = p.stdout.decode()
        else:
            with open(new_path) as fp:
                output = fp.read()

        matches = re.findall(r"[^A-Z_](STR_[A-Z0-9_]*)", output)
        strings_found.update(matches)


scan_files("src")
# STR_LAST_STRINGID is special, and not really a string.
strings_found.remove("STR_LAST_STRINGID")
# This is the include protector in misc/str.hpp
strings_found.remove("STR_HPP")
# These are mentioned in comments, not really a string.
strings_found.remove("STR_XXX")
strings_found.remove("STR_NEWS")
strings_found.remove("STR_CONTENT_TYPE_")

strings_found = sorted(list(strings_found))

for string in strings_found:
    if string not in strings_defined:
        print(f"ERROR: {string} found but never defined.")

for string in strings_defined:
    if string not in strings_found:
        print(f"INFO: {string} is possibly no longer needed.")
