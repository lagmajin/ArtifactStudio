#!/usr/bin/env python3
import os

files = [
    r'X:\Dev\ArtifactStudio\Artifact\src\Render\PrimitiveRenderer2D.cppm',
    r'X:\Dev\ArtifactStudio\Artifact\src\Render\PrimitiveRenderer3D.cppm'
]

for file_path in files:
    with open(file_path, 'r', encoding='utf-8') as f:
        content = f.read()
    
    # Replace DRAW_FLAG_VERIFY_ALL with DRAW_FLAG_NONE
    new_content = content.replace('DRAW_FLAG_VERIFY_ALL', 'DRAW_FLAG_NONE')
    
    with open(file_path, 'w', encoding='utf-8') as f:
        f.write(new_content)
    
    print(f'Successfully replaced in: {file_path}')
