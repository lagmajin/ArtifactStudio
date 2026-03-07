import os
import re

directory = 'ArtifactCore/src/AI'

for root, _, files in os.walk(directory):
    for file in files:
        if file.endswith('.cppm'):
            filepath = os.path.join(root, file)
            with open(filepath, 'r', encoding='utf-8') as f:
                content = f.read()
            
            # Change 'module Core.AI.XXX;' to 'export module Core.AI.XXX;'
            # but only if it doesn't already have 'export'
            if 'export module' not in content:
                new_content = re.sub(r'(?m)^module (Core\.AI\.[a-zA-Z0-9_]+);', r'export module \1;', content)
                
                with open(filepath, 'w', encoding='utf-8') as f:
                    f.write(new_content)
                print(f"Exported module in {filepath}")
