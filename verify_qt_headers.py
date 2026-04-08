import re, os

files = [
  r'ArtifactCore\src\Color\XYZColor.cppm',
  r'ArtifactCore\src\Color\LabColor.cppm',
  r'ArtifactCore\src\Color\FloatColor.cppm',
  r'ArtifactCore\src\Color\ColorLUT.cppm',
  r'ArtifactCore\src\Color\ColorHarmonizer.cppm',
  r'ArtifactCore\src\Audio\WASAPIBackend.cppm',
  r'ArtifactCore\src\Audio\SimpleWav.cppm',
  r'ArtifactCore\src\Audio\QtAudioBackend.cppm',
  r'ArtifactCore\src\Audio\AudioWriter.cppm',
  r'ArtifactCore\src\Audio\AudioVolume.cppm',
  r'ArtifactCore\src\Audio\AudioRingBuffer.cppm',
  r'ArtifactCore\src\Audio\AudioRenderer.cppm',
  r'ArtifactCore\src\Audio\AudioRasterizer.cppm',
  r'ArtifactCore\src\Audio\AudioPanner.cppm',
  r'ArtifactCore\src\Audio\AudioMixer.cppm',
  r'ArtifactCore\src\Audio\AudioDownMixer.cppm',
  r'ArtifactCore\src\Audio\AudioCache.cppm',
  r'ArtifactCore\src\Audio\ASIOBackendStub.cppm',
  r'ArtifactCore\src\Audio\AudioBus.cppm',
  r'ArtifactCore\src\Audio\AudioAnalyzer.cppm',
  r'ArtifactCore\src\Audio\AudioCompressor.cppm',
  r'ArtifactCore\src\Audio\AudioDecibels.cppm',
]

MODULE_RE = re.compile(r'^\s*(export\s+)?module\s+')
QT_RE = re.compile(r'#include\s+<(Q[A-Za-z]|Qt)')

issues = []
for f in files:
    path = os.path.join(r'X:\dev\artifactstudio', f)
    lines = open(path).readlines()
    mod_line = next((i for i,l in enumerate(lines) if MODULE_RE.match(l)), None)
    if mod_line is None:
        issues.append(f'{f}: no module declaration')
        continue
    bad = [l.rstrip() for l in lines[:mod_line] if QT_RE.search(l)]
    if bad:
        issues.append(f'{f} (module@{mod_line+1}): Qt in GMF: {bad}')
    else:
        print(f'OK  {f} (module@{mod_line+1})')

if issues:
    print('ISSUES:')
    for i in issues: print(' ', i)
else:
    print('All files OK')
