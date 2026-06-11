# Repository Audit: Layer Lifecycle

## Purpose

Trace layer creation, activation, mutation, and destruction to find lifecycle mismatches.

## Scope

- layer base classes
- composition add/remove flows
- selection, undo, and render invalidation hooks

## Questions to Answer

- Which layer types have special lifecycle rules?
- Where does a layer change without invalidating the right caches?
- Which flows depend on selection state being already synchronized?

## Deliverable

- A lifecycle diagram and a list of fragile handoff points.

