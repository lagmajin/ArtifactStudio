# M-USD-INTERCHANGE-1 OpenUSD Interchange Foundation

**最終更新:** 2026-09-22

**ステータス:** Not Started

## 目的

OpenUSD を「対応拡張子」ではなく、読込可能範囲・失敗理由・asset lifecycle が明確な
3D interchange 経路として導入する。初期版は読み取り専用に限定する。

## 現状の根拠

- OBJ / FBX / glTF / GLB は既存 `MeshImporter` 本線で扱う。
- USDA は限定的な ASCII 読み込みに留まり、USDC / USDZ は OpenUSD runtime 未接続である。
- 現行 3D layer は基本的に単一 mesh を保持し、完全な scene hierarchy、skinning、animation、
  node material を保持する契約にはなっていない。

## 範囲

1. OpenUSD dependency / licensing / packaging / Windows and macOS 配布の実現性を判定する。
2. `usd` / `usda` / `usdc` / `usdz` を recognized と importable に分ける。
3. read-only stage から static mesh、階層、basic PBR material、texture asset path を抽出する。
4. partial import、unsupported prim、texture resolve failure を Asset Browser と diagnostics に出す。
5. Diligent の既存 backend-neutral 3D renderer に変換済み mesh / material を渡す。

## 非対象

- USD scene authoring、variant edit、USD export。
- rigging、skinning、blend shape、Alembic、Hydra / renderer 全置換。
- UI filter だけを先行させて import 成功と表示すること。

## フェーズ

### Phase 0 — Adoption decision

- representative USD assets、supported prim list、配布サイズ、license を用意し、OpenUSD runtime を
  導入するかを product decision として確定する。

### Phase 1 — Capability contract and diagnostics

- format capability table: recognized / importable / previewable / layerable。
- loader の backend name、unsupported content、fallback を structured diagnostic として残す。

### Phase 2 — Static scene import

- geometry hierarchy、basic material、texture path を immutable import result に変換する。
- `Artifact3DLayer` への明示的 layer creation と保存／再読込を接続する。

### Phase 3 — Production acceptance

- nested prim、material bindings、relative texture、missing reference、large scene の representative fixture
  で preview / render / restore を確認する。

## 完了条件

- 実際に読めるUSD形式と未対応範囲がUI / diagnosticsで区別される。
- import失敗が default cube や placeholder に隠れない。
- static geometry + basic PBR material が既存GPU経路で一貫して表示される。

## 関連

- `docs/analysis/MINOR_IMAGE_3D_FORMAT_IMPORT_INVESTIGATION_2026-08-13.md`
- `docs/analysis/TECH_ADOPTION_CANDIDATES_2026-07-27.md`

