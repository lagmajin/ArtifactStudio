import os
import re

directories = ['ArtifactCore/src', 'ArtifactCore/include', 'ArtifactWidgets/src', 'ArtifactWidgets/include', 'Artifact/src', 'Artifact/include']

for directory in directories:
    for root, _, files in os.walk(directory):
        for file in files:
            if file.endswith('.cpp') or file.endswith('.cppm') or file.endswith('.ixx'):
                filepath = os.path.join(root, file)
                with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
                    content = f.read()
                
                lines = content.split('\n')
                
                module_idx = -1
                for i, line in enumerate(lines):
                    if line.startswith('export module') or (line.startswith('module ') and not line.startswith('module;')):
                        module_idx = i
                        break
                        
                if module_idx != -1:
                    has_includes_after = False
                    for i in range(module_idx + 1, len(lines)):
                        if lines[i].startswith('#include'):
                            has_includes_after = True
                            break
                            
                    if has_includes_after:
                        # Extract all includes that appear AFTER the module declaration
                        pre_module = lines[:module_idx]
                        module_decl = [lines[module_idx]]
                        includes = []
                        post_module = []
                        
                        for line in lines[module_idx + 1:]:
                            if line.startswith('#include') or line.startswith('#pragma'):
                                includes.append(line)
                            else:
                                post_module.append(line)
                                
                        # Determine if there is a global module fragment 'module;'
                        gmf_idx = -1
                        for i, line in enumerate(pre_module):
                            if line.strip() == 'module;':
                                gmf_idx = i
                                break
                                
                        if gmf_idx != -1:
                            # Insert includes right after 'module;'
                            new_lines = pre_module[:gmf_idx + 1] + includes + pre_module[gmf_idx + 1:] + module_decl + post_module
                        else:
                            # If no 'module;' exists but we have includes, we MUST add 'module;' at the top
                            new_lines = ['module;'] + includes + pre_module + module_decl + post_module
                            
                        with open(filepath, 'w', encoding='utf-8') as f:
                            f.write('\n'.join(new_lines))
                        print(f"Fixed include order in {filepath}")

