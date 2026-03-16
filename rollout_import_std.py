import os
import re

std_headers = {
    '<iostream>', '<vector>', '<string>', '<map>', '<unordered_map>',
    '<set>', '<unordered_set>', '<memory>', '<algorithm>', '<cmath>',
    '<functional>', '<optional>', '<utility>', '<array>', '<mutex>',
    '<thread>', '<chrono>', '<filesystem>', '<fstream>', '<sstream>',
    '<stdexcept>', '<type_traits>', '<variant>', '<any>', '<atomic>',
    '<condition_variable>', '<queue>', '<deque>', '<list>', '<tuple>',
    '<numeric>', '<regex>', '<random>', '<cstdint>', '<cassert>', '<future>'
}

def is_std_include(line):
    line = line.strip()
    if not line.startswith('#include'):
        return False
    match = re.search(r'#include\s*(<[^>]+>)', line)
    if match:
        header = match.group(1)
        if header in std_headers:
            return True
    return False

def process_file(file_path):
    with open(file_path, 'r', encoding='utf-8') as f:
        lines = f.readlines()
        
    has_module_decl = False
    module_line_index = -1
    for i, line in enumerate(lines):
        # Match "export module X;" or "module X;"
        if re.match(r'^\s*(export\s+)?module\s+[a-zA-Z0-9_.:]+;', line):
            has_module_decl = True
            module_line_index = i
            break
            
    if not has_module_decl:
        return False, "No module declaration found"

    # Check if 'import std;' is already present
    for line in lines:
        if re.match(r'^\s*import\s+std\s*;', line):
            return False, "Already has import std;"

    new_lines = []
    removed_std = False
    
    # Track the module; part
    in_module_prelude = False
    for i, line in enumerate(lines):
        if re.match(r'^\s*module\s*;', line):
            in_module_prelude = True
            
        if in_module_prelude and is_std_include(line):
            removed_std = True
            continue # Skip this line
            
        new_lines.append(line)
        
        # We need to find the module line index in the *new* lines
        if i == module_line_index:
            new_lines.append('import std;\n')

    if removed_std:
        with open(file_path, 'w', encoding='utf-8') as f:
            f.writelines(new_lines)
        return True, "Updated"
    else:
        # If no std headers were removed, we still might want to add import std?
        # Maybe skip to be safe.
        return False, "No std headers removed"

directories_to_scan = [
    'ArtifactCore/include/Utils',
    'ArtifactCore/include/Math',
    'ArtifactCore/include/Time'
]

if __name__ == "__main__":
    count = 0
    for d in directories_to_scan:
        for root, _, files in os.walk(d):
            for file in files:
                if file.endswith('.ixx') or file.endswith('.cppm'):
                    path = os.path.join(root, file)
                    try:
                        changed, msg = process_file(path)
                        if changed:
                            print(f"Updated: {path}")
                            count += 1
                    except Exception as e:
                        print(f"Error processing {path}: {e}")
    print(f"Total updated files: {count}")
