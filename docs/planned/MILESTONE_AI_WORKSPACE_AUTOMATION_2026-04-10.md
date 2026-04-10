# AI Workspace Automation Milestone

## Goal

Build a structured AI tool surface for workspace-level editing in ArtifactStudio.

The first target is read-oriented inspection, then safe write actions that map to existing project and render-queue services:

- inspect current project, active composition, selection, and render queue
- create projects and compositions
- import assets
- edit layers by renaming, moving, duplicating, and removing them
- queue and start renders

## Scope

- `WorkspaceAutomation` describable registered in the AI tool registry
- stable JSON snapshots for project, composition, selection, and render queue
- safe wrapper methods for common workspace actions
- tests for schema exposure and basic invocation paths

## Phases

### Phase 1

- register the workspace automation tool host
- expose snapshot methods
- expose project create/import helpers

### Phase 2

- expose layer editing helpers
- expose render queue queueing and start helpers
- add regression coverage for mutation paths

### Phase 3

- add dry-run/confirmation metadata for destructive operations
- add richer payloads for asset and composition selection

