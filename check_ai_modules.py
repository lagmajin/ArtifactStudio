import os
import re

directory = 'ArtifactCore/src/AI'

for root, _, files in os.walk(directory):
    for file in files:
        if file.endswith('.cppm'):
            filepath = os.path.join(root, file)
            with open(filepath, 'r', encoding='utf-8') as f:
                content = f.read()
            
            # Find the line that looks like 'export module Core.AI.XXX;'
            # and if the file is .cppm, change it back to 'export module Core.AI.XXX;'
            # Wait, the problem is that CMake doesn't know it's a module interface if it's named .cppm sometimes?
            # No, the error is C2230: could not find module 'Core.AI.AIAnalysisDescriptions'.
            # MSVC C2230 on an 'export module' line means that the compiler thinks this file is an implementation unit
            # trying to export a module that it hasn't seen the interface for.
            # But 'export module M;' IS the interface declaration!
            # Let's check the exact syntax of the module declaration in these files.
            print(f"Checking {filepath}...")
            lines = content.split('\n')
            for line in lines:
                if 'module Core.AI' in line:
                    print(line)
