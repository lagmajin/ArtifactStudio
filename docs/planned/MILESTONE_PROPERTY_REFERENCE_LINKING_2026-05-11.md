# Milestone: Property Reference Linking / Pick Whip

> Parent-side roadmap for AE-style property linking.

This milestone covers the "pick-whip" style workflow where one property can be linked to another property without typing a raw path by hand.

## Purpose

- Make property relationships first-class
- Reduce manual target-path entry
- Give expression and driven-property workflows a visual linking surface
- Keep the property source of truth aligned with `AbstractProperty`

## Boundary

- This milestone does not replace keyframe editing
- This milestone does not replace curve editing
- This milestone does not change the expression evaluator itself
- This milestone does not add a global signal bus

## Intended First Slice

1. expose referenceable properties in a stable catalog
2. resolve property targets by path / layer / comp context
3. show the selected target in the property row or inspector
4. allow a drag gesture to capture the target link

## Execution Phases

### Phase 1

- read-only target resolution
- stable target catalog
- inline target display

### Phase 2

- property row / inspector drag gesture
- hover preview for compatible targets
- local link capture without changing keyframe semantics

## Guardrails

- Do not auto-link unrelated properties
- Do not conflate keyframe values and reference links
- Keep the visual link surface local to property / inspector workflows
- Use existing property paths and context resolution

## Cross References

- [Property / Keyframe Integration Plan](/x:/Dev/ArtifactStudio/docs/planned/MILESTONE_PROPERTY_KEYFRAME_UNIFICATION_2026-03-25.md)
- [Expression System](/x:/Dev/ArtifactStudio/docs/planned/MILESTONE_EXPRESSION_SYSTEM_2026-03-29.md)
- [Inline Interaction Surfaces](/x:/Dev/ArtifactStudio/docs/planned/MILESTONE_INLINE_INTERACTION_SURFACES_2026-03-31.md)
- [Timeline Flat Keyframe View](/x:/Dev/ArtifactStudio/docs/planned/MILESTONE_TIMELINE_FLAT_KEYFRAME_VIEW_2026-04-03.md)

## Next Step

1. add a read-only target resolver for property paths
2. define which property types can be linked safely
3. start with the inspector / property row surface before timeline integration
4. use the Phase 2 section in this parent milestone for the surface-level work

## 2026-07-10 Source Audit

- 計画書が前提にしている `PropertyLinkManager` の実体は現行sourceに存在しない
- raw expression stringを直接生成する実装は見送り、stable layer/property resolverを先に追加する

### Phase 1 progress

- `artifact.property-reference.v1` tokenを導入
- composition ID / layer ID / property path / property typeをJSON参照としてcopy可能にした
- clipboard tokenをactive compositionでresolveし、layer/property/typeの存在を検証して対象layerを選択する
- 評価リンクやpick-whip UIはまだ作らず、read-only catalog / resolverに限定した

## Static Audit (2026-07-25)

Phase 1 の source audit と一致して、Composition Editor に stable reference token の copy／clipboard resolve 導線があり、composition ID・layer ID・property path・property type の存在確認と対象 layer の選択を行う。既存の ObjectReference／Layer matte 用 picker UI もあるが、これは object／matte 参照の導線であり、任意の `AbstractProperty` 間を結ぶ Property Link とは別責務である。

現行ソースには `PropertyLinkManager`、referenceable property の包括的 catalog、property row からの drag pick-whip、互換性判定付き hover preview、リンクを保存・評価する model は確認できない。clipboard の read-only resolver は Phase 1 の範囲を満たす足場だが、expression target との分離、keyframe と reference の同一 property source 上での共存、Undo／serialization、runtime のリンク評価は未実装または未検証である。

判定: **Phase 1 の read-only token／resolver は実装済み。** Phase 2 の visual pick-whip／local link capture と本格的な property catalog は未着手である。
