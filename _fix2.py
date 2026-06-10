# Fix: move ALL includes back to GMF (everything before module X;)
# Only exclude: class tst_QList; (already removed)

patches = [
    # LevelsCurves.cppm
    ('ArtifactCore/src/ImageProcessing/ColorTransform/LevelsCurves.cppm',
     'module;\n#include <tbb/parallel_for.h>\n#include <tbb/blocked_range.h>\n#include <tbb/parallel_reduce.h>\nmodule ImageProcessing.ColorTransform.LevelsCurves;\n#include <cmath>',
     'module;\n#include <tbb/parallel_for.h>\n#include <tbb/blocked_range.h>\n#include <tbb/parallel_reduce.h>\n#include <cmath>'),
    ('ArtifactCore/src/ImageProcessing/ColorTransform/ColorBalance.cppm',
     'module;\n#include <tbb/parallel_for.h>\n#include <tbb/blocked_range.h>\nmodule ImageProcessing.ColorTransform.ColorBalance;\n#include <algorithm>',
     'module;\n#include <tbb/parallel_for.h>\n#include <tbb/blocked_range.h>\n#include <algorithm>'),
    ('ArtifactCore/src/ImageProcessing/Halftone.cppm',
     'module;\n#include <tbb/parallel_for.h>\n#include <tbb/blocked_range.h>\nmodule ImageProcessing:Halftone;\n#include <algorithm>',
     'module;\n#include <tbb/parallel_for.h>\n#include <tbb/blocked_range.h>\n#include <algorithm>'),
    ('ArtifactCore/src/ImageProcessing/Distortion.cppm',
     'module;\n#include <tbb/parallel_for.h>\n#include <tbb/blocked_range.h>\nmodule ImageProcessing.Distortion;\n#include <utility>',
     'module;\n#include <tbb/parallel_for.h>\n#include <tbb/blocked_range.h>\n#include <utility>'),
    ('ArtifactCore/src/ImageProcessing/AnamorphicFlare.cppm',
     'module;\n#include <tbb/parallel_for.h>\n#include <tbb/blocked_range.h>\nmodule ImageProcessing:AnamorphicFlare;\n#include <algorithm>',
     'module;\n#include <tbb/parallel_for.h>\n#include <tbb/blocked_range.h>\n#include <algorithm>'),
]

for fpath, old, new in patches:
    with open(fpath, 'r', encoding='utf-8') as fh:
        content = fh.read()
    if old not in content:
        print(f'SKIP (not matched): {fpath.split("/")[-1]}')
        continue
    # Find the boundary between GMF-based includes and purview-based includes
    # The current structure is: module;\nTBB\nmodule X;\nrest...
    # We need to merge: module;\nTBB\nrest...\nmodule X;
    idx_module = content.find('module ImageProcessing', content.find('module;') + 1)
    idx_next_include = content.find('#include', idx_module)
    if idx_next_include < 0:
        print(f'SKIP (no purview includes): {fpath.split("/")[-1]}')
        continue
    # Find where the purview includes end
    idx_end_includes = idx_next_include
    while True:
        next_inc = content.find('#include', idx_end_includes + 1)
        next_import = content.find('import ', idx_end_includes + 1)
        # pick whichever comes first
        candidates = []
        if next_inc > 0 and next_inc < idx_end_includes + 500:
            candidates.append(next_inc)
        if next_import > 0 and next_import < idx_end_includes + 500:
            candidates.append(next_import)
        if not candidates:
            break
        earliest = min(candidates)
        if earliest - idx_end_includes > 5:  # gap too big, not a contiguous include block
            break
        idx_end_includes = earliest + 1

    # Find end of include block in purview
    # Simple approach: find the next blank line or namespace after includes
    import_purview_includes = content[idx_module:]
    # Extract the include block after module line
    lines_after = content[idx_module:].split('\n')
    include_lines = []
    rest_lines = []
    collecting = False
    for line in lines_after[1:]:
        if line.startswith('#include ') or line.startswith('#include <'):
            include_lines.append(line)
        elif not line.strip():
            continue
        else:
            rest_lines.append(line)
            break
    rest = '\n'.join(lines_after[len(include_lines)+2:])  # +2 for module line and the blank line gap

    # Reconstruct: module;\nall_includes\nmodule X;\nrest
    all_includes = []
    for line in content[:idx_module].split('\n'):
        if line.startswith('#include') or line.startswith('module;') or not line.strip():
            if line.strip():
                all_includes.append(line)
    all_includes.extend(include_lines)
    # deduplicate by include path
    seen = set()
    unique = []
    for line in all_includes:
        if line.startswith('#include'):
            key = line.strip()
            if key in seen:
                continue
            seen.add(key)
        unique.append(line)
    
    new_content = '\n'.join(unique) + '\nmodule ' + content[idx_module:].split('\n')[0].strip() + '\n' + rest
    with open(fpath, 'w', encoding='utf-8') as fh:
        fh.write(new_content)
    print(f'OK: {fpath.split("/")[-1]}')
