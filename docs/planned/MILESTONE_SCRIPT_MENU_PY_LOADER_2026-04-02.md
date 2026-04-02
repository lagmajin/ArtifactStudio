# Milestone: Script Menu / menu.py Loader (M-PY-2)

## Goal
Build a script entry point that starts as a stable built-in menu and later grows into a Nuke-like `menu.py` system.

## Why
- The current `Script` menu gives us a clean UI entry now.
- A `menu.py` loader lets user scripts add commands without touching core code.
- The same path can later host custom tools, hook toggles, and workspace-specific actions.

## Scope
- Keep the built-in `Script` menu as the primary entry point.
- Load startup scripts from `scripts/menu.py` when present.
- Provide a small Python-side menu API for adding commands and submenus.
- Keep `ArtifactPythonHookManager` as the hook execution backend.
- Allow scripted commands to live alongside fixed menu items.

## Non-Goals
- Full Python plugin marketplace.
- Arbitrary UI object mutation from scripts.
- Replacing all built-in menus with scripts.

## Phases

### Phase 1: Loader Skeleton
- Read `scripts/menu.py` at startup.
- Add a safe import path for script menu helpers.
- Log script load success / failure clearly.

### Phase 2: Menu API
- Expose a minimal `menu.addCommand(...)` style API.
- Allow separators and nested submenus.
- Make menu registration idempotent.

### Phase 3: Built-in + Script Merge
- Merge built-in `Script` menu items with dynamically registered entries.
- Surface hook commands, utility actions, and workspace-specific tools.
- Keep the menu readable even if scripts add many items.

### Phase 4: Guardrails
- Add error reporting for broken script menus.
- Add enable/disable controls for script loading.
- Define a reload workflow for developers.

## First Targets
- `Artifact/src/Widgets/Menu/ArtifactScriptMenu.cppm`
- `Artifact/src/AppMain.cppm`
- `Artifact/src/Script/ArtifactPythonHookManager.cppm`
- `Artifact/include/Script/ArtifactPythonHookManager.ixx`

## Recommended Order
1. Add the loader skeleton.
2. Expose a tiny menu registration API.
3. Merge dynamic commands into the built-in `Script` menu.
4. Add reload / diagnostics support.
