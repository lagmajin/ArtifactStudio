import os
import re
import urllib.request
import sys
import xml.etree.ElementTree as ET

def find_used_icons(src_dir):
    used_icons = set()
    pattern = re.compile(r'([a-zA-Z0-9_-]+\.svg)')
    
    for root, dirs, files in os.walk(src_dir):
        for f in files:
            if f.endswith(('.cpp', '.cppm', '.h', '.ixx', '.ui', '.qrc')):
                path = os.path.join(root, f)
                try:
                    with open(path, 'r', encoding='utf-8') as file:
                        for line in file:
                            matches = pattern.findall(line)
                            for match in matches:
                                used_icons.add(match)
                except Exception:
                    pass
    return used_icons

def list_and_cleanup_unused_icons(icon_dir, used_icons, delete=False):
    unused = []
    for root, dirs, files in os.walk(icon_dir):
        for f in files:
            if f.endswith('.svg'):
                if f not in used_icons:
                    unused.append(os.path.join(root, f))
    
    print(f"Found {len(unused)} unused icons.")
    for u in unused:
        print(f" - Unused: {u}")
        if delete:
            os.remove(u)
            print(f"   -> Deleted!")

def download_icon(icon_name, color_hex, output_name, output_dir):
    url = f"https://fonts.gstatic.com/s/i/short-term/release/materialsymbolsoutlined/{icon_name}/default/24px.svg"
    
    print(f"Downloading {icon_name} as {output_name}...")
    try:
        req = urllib.request.Request(url, headers={'User-Agent': 'Mozilla/5.0'})
        with urllib.request.urlopen(req) as response:
            svg_data = response.read().decode('utf-8')
            
        svg_data = svg_data.replace('<svg ', f'<svg fill="{color_hex}" ')
        
        output_path = os.path.join(output_dir, f"{output_name}.svg")
        os.makedirs(output_dir, exist_ok=True)
        with open(output_path, 'w', encoding='utf-8') as f:
            f.write(svg_data)
        print(f"Saved: {output_path}")
    except Exception as e:
        print(f"Failed to download {icon_name}: {e}")

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage:")
        print("  python manage_icons.py cleanup [--delete]")
        print("  python manage_icons.py download <icon_name> <#RRGGBB> [output_name]")
        sys.exit(1)
        
    cmd = sys.argv[1]
    
    project_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    src_dir = os.path.join(project_root, "Artifact")
    icon_dir = os.path.join(project_root, "Artifact", "App", "Icon")
    
    if cmd == "cleanup":
        delete = len(sys.argv) > 2 and sys.argv[2] == "--delete"
        used = find_used_icons(src_dir)
        used.update(find_used_icons(os.path.join(project_root, "ArtifactCore")))
        used.update(find_used_icons(os.path.join(project_root, "ArtifactWidgets")))
        list_and_cleanup_unused_icons(icon_dir, used, delete)
        
    elif cmd == "download":
        if len(sys.argv) < 4:
            print("Missing params.")
            sys.exit(1)
        icon_name = sys.argv[2]
        color = sys.argv[3]
        out_name = sys.argv[4] if len(sys.argv) > 4 else icon_name
        
        # Color specific directories inside materialVS
        out_dir_name = color.replace('#', '')
        download_icon(icon_name, color, out_name, os.path.join(icon_dir, "MaterialVS", "colored", out_dir_name))
