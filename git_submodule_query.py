#!/usr/bin/env python
import subprocess
import sys

git_dir = r'X:\Dev\ArtifactStudio\.git\modules\ArtifactCore'
work_tree = r'X:\Dev\ArtifactStudio\ArtifactCore'
orig_head = '3d20a9f188136580a26911ef80a4fdee30b11cca'

# Extended list of commands to try
cmds = [
    ['git', f'--git-dir={git_dir}', f'--work-tree={work_tree}', '--no-pager', 'log', '--oneline', '-10'],
    ['git', f'--git-dir={git_dir}', f'--work-tree={work_tree}', '--no-pager', 'diff', '--name-only', 'HEAD~1', 'HEAD'],
    ['git', f'--git-dir={git_dir}', f'--work-tree={work_tree}', '--no-pager', 'show', '--stat', 'HEAD'],
    ['git', f'--git-dir={git_dir}', f'--work-tree={work_tree}', '--no-pager', 'diff', '--name-only', f'{orig_head}', 'HEAD'],
    ['git', f'--git-dir={git_dir}', f'--work-tree={work_tree}', '--no-pager', 'log', '--oneline', f'{orig_head}..HEAD'],
    ['git', f'--git-dir={git_dir}', f'--work-tree={work_tree}', '--no-pager', 'diff', '--name-only', f'{orig_head}', 'HEAD'],
]

print("="*80)
print("RUNNING GIT COMMANDS IN ARTIFACTCORE SUBMODULE")
print("="*80)
print(f"git-dir: {git_dir}")
print(f"work-tree: {work_tree}")
print(f"ORIG_HEAD: {orig_head}")
print("="*80)
print()

all_ixx_files = set()

for cmd in cmds:
    cmd_display = ' '.join(cmd[-4:])
    print(f"\n{'='*80}")
    print(f"COMMAND: {cmd_display}")
    print(f"{'='*80}")
    
    r = subprocess.run(cmd, capture_output=True, text=True, shell=False)
    
    print(f"Return Code: {r.returncode}")
    print()
    
    if r.stdout:
        print("STDOUT:")
        print(r.stdout)
        # Extract .ixx files from stdout
        for line in r.stdout.split('\n'):
            if line.strip().endswith('.ixx'):
                all_ixx_files.add(line.strip())
    
    if r.stderr:
        print("STDERR:")
        print(r.stderr)
    
    if not r.stdout and not r.stderr:
        print("(no output)")

print("\n" + "="*80)
print("SUMMARY: All .ixx files found in modified files")
print("="*80)
if all_ixx_files:
    for ixx_file in sorted(all_ixx_files):
        print(f"  {ixx_file}")
else:
    print("  (no .ixx files found)")
