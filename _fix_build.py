# ================================================================
# 1. LevelsCurves.cppm: fix histogram redef, slim GMF
# ================================================================
f = 'ArtifactCore/src/ImageProcessing/ColorTransform/LevelsCurves.cppm'
with open(f, 'r', encoding='utf-8') as fh:
    content = fh.read()

# Remove the ORIGINAL QVector<int> histogram(256, 0) that was shadowed by parallel_reduce
# It's right after "QVector<int> LevelsEffect::calculateHistogram("
old = '''QVector<int> LevelsEffect::calculateHistogram(const QImage& image, int channel) {
    QVector<int> histogram(256, 0);
    QImage converted = image.convertToFormat(QImage::Format_ARGB32);'''
new = '''QVector<int> LevelsEffect::calculateHistogram(const QImage& image, int channel) {
    QImage converted = image.convertToFormat(QImage::Format_ARGB32);'''
assert old in content, 'LevelsCurves: calculateHistogram header not found!'
content = content.replace(old, new, 1)
print('1a. LevelsCurves: removed redundant histogram init')

# Move GMF to minimal form: only TBB in GMF, rest in purview
old = '''module;
#include <cmath>
#include <algorithm>
#include <vector>

#include <iostream>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
#include <QString>
#include <QImage>
#include <QPointF>
#include <QVector>
#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>
#include <tbb/parallel_reduce.h>
module ImageProcessing.ColorTransform.LevelsCurves;'''

new = '''module;
#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>
#include <tbb/parallel_reduce.h>
module ImageProcessing.ColorTransform.LevelsCurves;
#include <cmath>
#include <algorithm>
#include <vector>
#include <iostream>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
#include <QString>
#include <QImage>
#include <QPointF>
#include <QVector>'''

assert old in content, 'LevelsCurves: GMF pattern not found!'
content = content.replace(old, new, 1)
print('1b. LevelsCurves: slim GMF (TBB only), rest in purview')
with open(f, 'w', encoding='utf-8') as fh:
    fh.write(content)


# ================================================================
# 2. ColorBalance.cppm: slim GMF
# ================================================================
f = 'ArtifactCore/src/ImageProcessing/ColorTransform/ColorBalance.cppm'
with open(f, 'r', encoding='utf-8') as fh:
    content = fh.read()

old = '''module;
#include <algorithm>
#include <cmath>
#include <QColor>
#include <QImage>
#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>

module ImageProcessing.ColorTransform.ColorBalance;'''

new = '''module;
#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>
module ImageProcessing.ColorTransform.ColorBalance;
#include <algorithm>
#include <cmath>
#include <QColor>
#include <QImage>'''

assert old in content, 'ColorBalance: GMF pattern not found!'
content = content.replace(old, new, 1)
print('2. ColorBalance: slim GMF')

with open(f, 'w', encoding='utf-8') as fh:
    fh.write(content)


# ================================================================
# 3. Camera.ixx: fix operator* ambiguity - remove duplicate Color operator*
#    The issue: Color = Vec3, so there are 3 operator* candidates for Vec3 * float
#    Solution: remove the redundant `Color operator*(const Color& c, float t)` at line 71
#    since it's identical to `operator*(float t, const Vec3& v)` which already covers it
# ================================================================
f = 'ArtifactCore/include/Render/Vector3D.ixx'
with open(f, 'r', encoding='utf-8-sig') as fh:
    content = fh.read()

old = '''inline Color operator*(const Color& c, float t) { return t * c; }

} // namespace ArtifactCore::RayTrace'''
new = '''} // namespace ArtifactCore::RayTrace'''
assert old in content, 'Render/Vector3D: Color operator* not found!'
content = content.replace(old, new, 1)
print('3. Render/Vector3D: removed redundant Color operator*')

# Also fix the Graphics/Vector3D.ixx similarly - same alias, same problem likely
with open(f, 'r', encoding='utf-8-sig') as fh:
    content = fh.read()
# Actually re-read the correct file - this is the Render one already

with open(f, 'w', encoding='utf-8') as fh:
    fh.write(content)


# ================================================================
# 4. Graphics/Vector3D.ixx: same Color operator* cleanup
# ================================================================
f2 = 'ArtifactCore/include/Graphics/Vector3D.ixx'
with open(f2, 'r', encoding='utf-8-sig') as fh:
    content = fh.read()

old = '''inline Color operator*(const Color& c, float t) { return t * c; }

} // namespace ArtifactCore'''
new = '''} // namespace ArtifactCore'''
assert old in content, 'Graphics/Vector3D: Color operator* not found!'
content = content.replace(old, new, 1)
print('4. Graphics/Vector3D: removed redundant Color operator*')

with open(f2, 'w', encoding='utf-8') as fh:
    fh.write(content)


# ================================================================
# 5. Sphere.ixx: add missing #include <cmath>
# ================================================================
f = 'ArtifactCore/include/Render/Sphere.ixx'
with open(f, 'r', encoding='utf-8-sig') as fh:
    content = fh.read()

old = 'module;\n#include <utility>\n#include <memory>\n\nexport module Render.Sphere;'
new = 'module;\n#include <utility>\n#include <memory>\n#include <cmath>\n\nexport module Render.Sphere;'
assert old in content, 'Sphere.ixx: header not found!'
content = content.replace(old, new, 1)
print('5. Sphere.ixx: added #include <cmath>')

with open(f, 'w', encoding='utf-8') as fh:
    fh.write(content)


print('\nAll fixes done!')
