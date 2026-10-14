# MILESTONE: 3D Material System

**最終更新:** 2026-10-14
**ステータス:** Material core、3D layer assignment、PBR texture input、shader/render 接続、Inspector／JSON、Matte／Metal／Plastic／Glass の preset 適用、RT triangleごとのscalar material slot選択、RT UV伝搬、AO／base color／metallic-roughness／normal／independent coat-normal／clearcoat factor／clearcoat roughness／sheen color／sheen roughness／anisotropy／emission map samplingとray-coneによるtexture mip選択、linear float Base Color TextureのsRGB storage encoding、Raster複数slotのscalar Principled定数とslot別texture SRV切替、Masked材質のshadow alpha clip、material range別alpha pass、Principled specular tint、dielectric thin-film interference（RGB 3波長近似、反射と透過のエネルギー配分）、異方性direct／IBL反射、Disney/Burley diffuse、Cycles風 diffuse roughnessのdirect single／multi-scatter energy補償近似、subsurface weight／IOR／color/radius／anisotropyによるFresnel・散乱方向制御（厚みを追跡しない高速近似）、coat normal varianceによるroughness filtering、coatがsheenと基材反射・透過より上にあるdirect／IBL energy ordering、Emissionのcoat／sheen層下配置に対応する視線方向energy attenuation、Base Color／Metallic-Roughness／Normal／Coat Normal／Clearcoat factor／Clearcoat roughness／Sheen color／Sheen roughness／Anisotropy／Occlusion／Emission／OpacityのUV0／UV1選択、ufbx/glTFおよびOBJ/MTLのmaterial table単位Principled scalar／tint／texture importと、material partごとのRaster／RT slot mappingはコード実装済み。独立 Material Browser／asset 管理、高度 mapping、実制作mesh/HDRでのRaster／RT parityは pending。Raster／RT shader変更後build/runtime verificationも pending。

## 2026-10-14 Principled Anisotropy Texture

- Material、UFBX material table import、single／slot JSON、Inspector、render signature、Raster texture binding／shader、RT material packet／texture cache／closest-hit shaderへAnisotropy mapを接続する。UV0／UV1、独立 scale U/V、rotation、offsetを保持し、linear textureのB channelでsigned anisotropy量を変調する。RTは変換後UV edgeからray-cone mipを選ぶ。
- Mapのlinear RGを[-1,1] tangent-space directionへ展開・正規化し、Materialのanisotropy rotationを加算して接線基底へ適用。B channelは異方性strengthへ乗算する。異方性mapのUV transformはマップ参照位置のみを変え、directionを適用するshading tangent frameは変えない。
- **未確認／制限:** mesh入力にauthored TANGENT属性がなく、基底はnormal map UVから画素／triangle単位で再構築する。glTF仕様はanisotropyを使うprimitiveでNORMALとTANGENTの提供を推奨し、導出アルゴリズムは規定していない。ufbxはUV setごとのtangent/bitangentを提供するが、Mesh RenderData／meshlet／Raster／RTにはまだ伝搬しない。glTF `AnisotropyRotationTest` のauthored tangent／texture compensationケースとは完全一致しない可能性がある。build、shader compile、KHR fixture、Inspector、save/reload、slot texture rebinding、Raster／RT pixel parityは未確認。

## 2026-10-14 RT Anisotropy Map IBL

- RT closest-hitでAnisotropy mapをヒットごとに一度だけサンプルし、同じsigned scalarをdirect BRDF、bent reflection、anisotropic environment lookupへ共有する。
- **未確認:** shader compile、texture sampling descriptorの実行時挙動、およびanisotropy map有無によるRaster／RT visual parityは未確認。

## 2026-10-14 Anisotropy map direction and strength channels

- Raster／RTともAnisotropy mapのlinear RGをtangent-space direction、Bをstrengthとして使う。RGは[-1,1]へ展開・正規化し、Materialのrotationを加算してnormal map UVから作る接線基底へ適用する。方向基底と異方性textureのUV／transformは分離する。公式glTF定義ではRGが方向、Bが強度。
- `.gltf`／`.glb`のJSONから`KHR_materials_anisotropy` strength／rotationをmaterial name／table順で照合し、外部URIのanisotropy imageと`KHR_texture_transform`のT*R*S offset／scale／rotation／texCoordをImportedPbrMaterialへ渡す。UV中心回転の既存Material transformへoffsetを換算する。テクスチャ画像はLinear texture pathからRaster／RTへ流す。
- **未確認／制限:** data URI、bufferView埋め込み画像、`KHR_texture_basisu`などtexture source拡張は未対応。negative／zero scaleはMaterialの正スケール契約へ収まらない。glTF authored TANGENTはMesh attributeからGPU vertex input／RT shading dataへ伝搬しない。Mesh generic copyは属性を保持するが、subdivisionはposition／normal等を個別補間し、RendererとRTも固定attribute layoutであるため、対応には全経路の一貫更新が必要。material table照合、URI image、texture transform、shader compileおよび画像応答は未確認。

## 2026-10-14 Principled Sheen Roughness Texture

- Material、UFBX material table import、single／slot JSON、Inspector、render signature、Raster texture binding／shader、RT material packet／texture cache／closest-hit shaderへSheen Roughness mapを接続する。UV0／UV1、独立 scale U/V、rotation、offsetを保持し、linear textureのR channelをSheen Roughnessへ乗算する。RTは変換後UV edgeからray-cone mipを選ぶ。
- **未確認／制限:** build、shader compile、UFBX fixture、Inspector、save/reload、single／slot texture rebinding、Raster／RT pixel parityは未確認。

## 2026-10-14 Principled Sheen Color Texture

- Material、UFBX material table import、single／slot JSON、Inspector、render signature、Raster texture binding／shader、RT material packet／texture cache／closest-hit shaderへSheen Color mapを接続する。各materialのUV0／UV1、独立 scale U/V、rotation、offsetを保持し、Raster／RTでsRGB colorとしてSheen Tintへ乗算する。RTは変換後UV edgeからray-cone mipを選ぶ。
- **未確認／制限:** build、shader compile、UFBX fixture、Inspector、save/reload、single／slot texture rebinding、Raster／RT pixel parityは未確認。

## 2026-10-14 OBJ/MTL multi-material import

- tinyobjloaderのMTL material tableを既存Principled slot tableへ変換し、OBJ面の`material_ids`に対応する連続triangle index rangeを`Mesh::MaterialSlot`へ割り当てる。Diffuse/opacity、metallic、roughness、Phong shininess由来roughness、clearcoat、sheen、anisotropy／rotation、specular tint、IOR、emissionと、対応するtexture path／MTLのscale・offsetをslotへ渡す。MTL parserが未指定`Kd`と明示的な黒を区別できないため、全0 diffuseはwhite fallbackにする。
- Phong shininessからroughnessへの変換は `sqrt(2 / (shininess + 2))` の近似で、MTLのPBR roughness値が正ならそちらを優先する。
- **変更:** material tableが1件だけでmaterial rangeがあるOBJも、slot tableを作らずlayer本体のMaterialへ同じ変換を適用する。複数material tableは従来どおりslotごとのMaterialへ適用する。
- **未確認／制限:** ビルド、OBJ/MTL fixture、single／multi slotのRaster／RT描画は未確認。任意texture mapごとのUV rotation／独立map表現は未対応。

## 2026-10-14 Independent OBJ/MTL metallic and roughness maps on Raster

- `map_Pm`／`map_Pr` pathを別Material値として保持し、Raster single materialとmaterial slotへ別linear SRVをbindする。各mapのR channelをmetallic／roughness factorへ乗算し、独立mapがある側ではpacked Metallic-Roughness textureの同じchannelを使わない。
- 3D layerのsingle／slot JSON保存・復元、Inspector texture path、render material signatureへ接続する。
- MTL mapごとのscale／offset／UV setをMaterialに保持し、Raster single／slotの各map sampleへ独立して適用する。Materialのrotationにも対応し、layerのsingle／slot JSONとInspectorへ保存・表示する。
- RT PBR material packet、texture cache、material buffer、closest-hit shaderへmetallic／roughness別texture indexとscale／offset／rotation／UV setを渡す。変換後のUV edgeで各mapのray-cone mipを選び、各R channelを対応factorへ適用する。独立mapが存在するときはpacked map samplingを無効にし、MTL個別mapのpacked channel誤読を避ける。
- **未確認／制限:** shader compile、build、pixel/runtime表示とRaster／RT visual parityは未確認。

## 2026-10-14 Principled Thin Wall Transmission

- Add a `Thin Wall` boolean to Material, the 3D layer Inspector, single-material JSON and material-slot JSON. It is false by default, preserving the existing refractive transmission mode.
- Raster and RT transmission use a straight-through environment direction for thin walls. The interface Fresnel term is converted to the zero-thickness two-interface reflectance `2F/(1+F)`, and the remaining energy is used for transmission; the same effective Fresnel is used for dielectric reflection and thin-film reflectance.
- **未確認／制限:** This is a zero-thickness sheet approximation; it does not model absorption through a finite volume or internal geometry. Shader compile, build, runtime appearance and Raster／RT parity are unverified.

## 2026-10-14 UFBX / glTF multi-material import

- ufbx material tableからPrincipledのbase color、metallic／roughness、coat、transmission、IOR、sheen、specular／sheen tintと各material texture pathを保持する。meshのmaterial part順にfaceを並べ、`Mesh::MaterialSlot`のindex rangeへglobal material table indexを割り当てる。3D layerはslot別Materialを作り、既存Raster range drawとRT triangle material indexへ渡す。
- Base color／tintのRGBはufbx linear値から既存MaterialのsRGB QColor表現へ変換する。texture-only color mapは係数未指定として既定色で上書きしない。明示alphaが1未満ならslotをBlendedにする。
- **未確認／制限:** ビルド・fixture・runtime描画は未実行。per-node material override、任意node transform、alpha MASK cutoff、layerへのmaterial table保存／再読込は対象外。Importerの単一material getterは従来互換で、複数material用getterが新たに追加された。

## 2026-10-14 Multi-material layer persistence

- `Artifact3DLayer` serializes each loaded multi-material slot's source name, Principled scalars, tints, alpha state, emission and texture paths into `material.slots`; loading matches unused slots by name first, then falls back to the saved position for legacy or duplicate-name cases. Older project JSON without this array continues to use freshly imported source materials.
- **未確認／制限:** build and save/reload round-trip were not run. At this point duplicate source material names remain position-dependent, and the main layer material remains the current property editing surface. Per-slot UV import is tracked below.

## 2026-10-14 UFBX per-texture UV transforms

- Capture UFBX texture UV set by matching its name against mesh UV sets, plus UV offset, rotation, and scale for base color, metallic-roughness, normal, clearcoat normal, emission, occlusion, and opacity maps. Apply the values through each existing Material map's UV settings and persist them in `material.slots`.
- **未確認／制限:** only one UV set index (0/1) is supported by Material; lookup uses the first matching mesh UV-set name in the scene. Existing map APIs support uniform positive scale, so nonuniform or negative UFBX scale is not applied. Transform convention and rendered parity are unverified without a build/runtime check.

## 2026-10-14 Imported material slot editing

- Add per-slot inspector groups for UFBX/glTF material slots, covering base color, metallic/roughness, specular and tint, IOR/transmission, clearcoat, sheen, thin film, subsurface, anisotropy, emission, alpha, normal/occlusion, environment, and shared UV controls. Edits update the slot Material consumed by the existing multi-material Raster/RT paths.
- Serialize and restore these slot values in `material.slots`, alongside the existing texture paths and per-map UV transforms. Numeric values are clamped to the Material controls' supported ranges.
- Expose supported texture paths and their per-map scale, rotation, offset, and UV0/UV1 selection in each material-slot property group. A present but empty saved path now clears the imported path on restore.
- Extend UFBX multi-material import to transfer authored diffuse roughness, coat IOR, specular anisotropy, and subsurface factor/color/radius/anisotropy into the existing slot Material properties.
- Also transfer authored opacity, coat color, emission color, and emission factor; opacity below 1 selects the existing Blended material mode. Alpha mask cutoff/mode detection remains unavailable in this UFBX material-table path.
- Transfer authored thin-film thickness and IOR into the existing Material iridescence approximation.
- Route material-slot texture path properties through the existing Path editor and image file picker.
- **未確認／制限:** interactive editing, save/reload, texture rebinding, and rendered output are not runtime-verified. Renderer-side texture resolution follows the existing Material path behavior.

## 2026-10-14 Principled anisotropy + Burley diffuse

- ufbx single-material import now carries authored `coat_factor`, `coat_roughness`, `transmission_factor`, `specular_ior`, `sheen_factor`, and `sheen_roughness` values into the 3D layer. Existing layer values are preserved unless they are still at their defaults. This is scalar import only; extension texture maps, sheen color, and per-material propagation across a multi-material glTF mesh remain pending.
- **未確認:** glTF fixtures using `KHR_materials_clearcoat`, `KHR_materials_transmission`, `KHR_materials_ior`, and `KHR_materials_sheen` have not been imported and rendered after this change. The implementation captures the first specified scalar value for each field from the ufbx material list, consistent with the existing single-material texture/factor path.

- Mesh `RenderData`へUV1配列を追加し、ufbxの2番目のUV setを読み込み、triangle flattenとmeshletの頂点remap後も保持する。対象meshにUV1がなければUV0をRaster／RT入力へ複製する。Base Color／Metallic-Roughness／Normal／Coat Normal／Clearcoat factor／Occlusion／Emission／OpacityのUV Set（0／1）をMaterial／3D Layer JSON／Propertyへ追加し、Raster／RTサンプリングへ適用する。Masked shadow alpha clip／RT Any HitにもBase Color／Opacityの選択UVを通す。Normal／Coat Normalは各mapの接線基底を作り、RT ray-cone mip密度は各mapのUV微分から計算する。
- **未確認:** importer fixture、meshlet remap後の値、UV1 shader compilationと画素応答、masked shadow境界、Raster／RT parityは未検証（ビルド／テスト未実行）。
- Clearcoatをsheenより上の層として扱うよう、Raster／RTのdirect sheenと両経路のIBL sheenをclearcoatのview/light transmission energyで減衰。環境透過もcoat energyを通す。Raster／RTとも裏面光の早期continueより先にdirect transmissionを評価するよう変更し、RT shadow ray originも裏面側へ移して自己交差を避ける。RTはPrincipled specular IOR由来Fresnelと薄膜Fresnelをdirect transmissionに適用し、view-ray TIRではdirect／IBL透過を遮断する。
- **未確認:** shader compilation、clearcoat／sheen／transmissionの画素responseおよびRaster／RT parityは未検証（ビルド／テスト未実行）。RTのshadowed／area-light transmission responseは受け入れ確認が必要。
- Raster異方性の接線／bitangentをNormal Map適用後のUV微分から構築するよう修正。Ray TracingはNormal Map専用rotationを接線基底へ反映しており、Rasterだけ通常UV基準だった差をそろえる。Normal UVの正の一様scaleは向きを変えず、rotationを反映する。
- RasterのNormal／Coat Normal用微分基底を、補間法線へ直交化したtangentと、符号付きcross積で再構成したbitangentに変更。RTは同じ投影・handedness方式を使う。接線が退化するfragmentではnormal map適用前の法線を保つ。
- **未確認:** shader compilation、Normal Map rotationと異方性rotationの組合せ、およびRaster／RT画像parityは未検証（ビルド／テスト未実行）。

- `Thin Film Thickness`（0〜1000 nm、0で無効）と `Thin Film IOR`（1〜2.5、既定1.33）をMaterial／Inspector／JSON／signatureとsingle・multi-material Raster／RTへ接続。誘電体反射を610/550/460 nmの3波長干渉で近似し、directはmicrofacet half-vector角、IBLはview angleで評価する。同じRGB反射率から1-Rの色別透過率を作り、environment transmissionとRasterのback-light transmissionにも適用する。屈折方向は基材IORのみで計算し、metallic成分は薄膜未適用。Blender Cyclesの完全なスペクトル・層モデルではない。
- `Specular Tint`をColor入力として追加し、JSON／Inspector／material signature／single・multi-material Raster／RTへ接続。IORとSpecular量から得た誘電体F0をTint色で変調し、金属成分は従来どおりBase ColorでF0を決める。白Tintは既定の反射を維持する。

- `Material`に符号付き異方性を追加し、3D layer JSON、Property、material signature、Rasterのsingle／multi-material、RT material bufferへ接続。Raster／RTのdirect GGXはUV由来の接線基底と相関Smith可視性を使う。
- `Anisotropy Rotation`（-180〜180度）をlayer property／JSON／material signatureへ追加し、Raster／RTともnormal map適用後のUV接線基底を回転してdirect／IBL異方性へ適用する。
- `Subsurface Weight`（0〜1）をlayer property／JSON／material signature、Raster／RT material packetへ追加。direct diffuseはDisneyの簡易subsurface近似とBurley diffuseを混合し、IBLのdiffuse directional albedoをLambert値へ近づける。厚み・散乱色・閉じた体積を持つBSSRDFではない。
- `Subsurface Color`をProperty／JSON／material signatureとRaster／RTへ接続し、weightに応じてBase Colorから散乱色へ拡散色を混合する。色はRGB値をsRGBからlinearへ変換してshaderで使用する。
- `Subsurface Radius`（RGB、既定1.0/0.2/0.1）をProperty／JSON／material signature、single／multi-material Raster、RTへ接続。チャンネル別の散乱色強度として働く軽量な近似で、物理単位・メッシュ厚み・実BSSRDF積分を扱わない。
- `Subsurface IOR`（1〜2、既定1.4）をProperty／JSON／material signature、single／multi-material Raster、RTへ接続し、SSS diffuseのFresnel energyだけに適用する。厚みを追跡する内部屈折やRandom Walkは実装していない。
- `Subsurface Anisotropy`（-0.9〜0.9、既定0）をProperty／JSON／material signature、single／multi-material Raster、RTへ接続し、直接光の散乱位相をHenyey–Greenstein型係数の異方性なし応答に対する比で補正する。g=0では従来の薄面近似を保つ設計。両経路は散乱前後方向の内積を使う。体積内の方向輸送やBSSRDFではなく、shader compile／画素応答は未確認。
- `Clearcoat IOR`（1〜2、default 1.5）をProperty／JSON／material signatureとRaster／RTへ接続。direct coat Fresnel、coat energy attenuation、IBL coat F0にIOR由来のSchlick F0を使う。default 1.5は従来のF0=0.04を維持する。
- `Clearcoat Tint`をColor property／JSON／material signatureとRaster／RTへ接続。linear化したTintでcoat reflectionと下地エネルギーを吸収着色し、IORとview/light角から算出する内部光路長で角度依存性を付ける。IBLは入射方向をview方向で近似し、白Tintでは従来結果を維持する。
- `Coat Normal Strength`（0〜1、default 1）でnormal detailの強さを調整する。独立 `Coat Normal Texture` があればそれを使い、未設定なら通常材質のNormal Mapをcoat法線へ混ぜる。Raster／RTのdirect coat GGXとcoat IBLへ適用し、選択されたnormal texture mip alphaからcoat roughnessをfilterする。
- `Diffuse Roughness`（0〜1、default 0）をMaterial／JSON／Property／single・multi-material Raster／RTへ追加。direct diffuseへOren–Nayar single-scatter係数とalbedo依存multi-scatter energy補償をBurley diffuseに重ね、IBLは既存のBurley LUTを維持する。Cyclesと同一のdiffuse closureではなく、画素検証も未実施。
- Base Color Textureにlinear float画像を指定した場合、Raster／RTともsRGB GPU formatへ格納する前にsRGB encodingを行う。transfer/colorSpaceが明示sRGBなら二重encodeしない。ACES／Rec.2020／P3／XYZ／Adobe RGB／ProPhoto文字列を含むwide-gamut変換は未対応。
- Raster／RTのIBL base-specularは、bent reflection vectorに加えて異方性の長軸方向へ対称な2 tapを使う。等方prefilterを使った近似で、各tapのroughness mipに対応するBRDF LUT係数を個別に適用し、異方性0では従来の単一sampleを維持する。clearcoat／sheen／transmissionは等方方向のまま。
- **未確認:** 追加tapとroughness別LUT参照のshader compilation、HDR上の異方性ハイライト形状、回転／符号／roughness別のRaster／RT pixel parityとGPU costは未検証。
- Direct diffuseは両経路でDisney/Burleyのlight/view scatter係数を使う。共有BRDF LUTは64x64から128x64へ拡張し、左半分はsplit-sum specularとCharlie sheenを保持、右半分は各view／roughnessのcosine-weighted Burley diffuse directional albedoを格納する。Raster／RTは係数をdiffuse irradianceへ適用し、RTのGGX multiple-scattering IBLには未変調のcosine irradianceを使う。
- **未確認:** shader compilation、Burley LUT readback、GPU runtime／frame cost、Raster／RT parity、異方性 ±1 の方向と normal map併用は未検証。IBL diffuseはdirectional scalarをcosine irradianceへ乗算する近似で、方向性HDRに対する完全なBurley convolutionではない。

## 2026-10-14 Opacity Texture UV scale

- Opacity Textureに0.01〜10の独立UV scaleを追加し、Material、Inspector、JSON保存／読込、material signatureへ接続。
- Raster通常描画とshadow alpha clip、RT masked alpha clipで共通UV変換を適用。RT alpha texture mip選択も変換後のUV密度を使う。
- **未確認:** shader compilation、GPU runtime、Raster／RT pixel parityは未検証。

## 2026-10-14 Coat Normal Texture UV offset

- Coat Normal Textureに独立U／V offset（-10〜10）を追加し、Material、Inspector、JSON保存／読込、material signatureへ接続。
- Rasterのsingle／multi-material shaderとRT material packetで共通UV transformの後にoffsetを加える。RTのmip密度はoffsetで変化しないため、既存のcoat rotation／scale由来のray-cone LODを維持する。
- **未確認:** shader compilation、GPU runtime、Raster／RT pixel parityは未検証。

## 2026-10-14 Clearcoat factor and roughness textures

- UFBX/glTF `coat_factor.texture`をMaterialへ取り込み、R channelでclearcoat scalarを乗算する。Material API、単一／slot JSON、Inspector、material signature、Raster single／slot texture binding、RT material packet／texture cache／closest-hit samplingへ接続する。
- Texture UV0／UV1、uniform scale、rotation、offsetを保持・反映する。RasterとRTはlinear textureとして扱い、RTでは変換済みUVとray-cone mipを使う。
- **未確認:** shader compile、ビルド、GPU runtime、per-slot再bind、およびRaster／RT parityは未検証。対応するUV setは0／1に限る。

## 2026-10-14 Clearcoat roughness texture

- `ufbx pbr.coat_roughness.texture`をMaterial importへ接続し、個別UV scale／rotation／offset／UV setと一緒にsingle／multi-material JSON、Inspector、render signatureへ保持する。
- Rasterはlinear textureのG channelをClearcoat Roughnessへ乗算し、RTも同じG channelをmap別UVとray-cone mipで読む。CPU／HLSLのRaster constant bufferとRT structured bufferに専用texture stateを追加した。
- **未確認／制限:** Shader compile、build/runtime描画、slot切替、Raster／RT parityは未確認。
## 2026-10-14 Metallic-Roughness Texture UV offset

- Metallic-Roughness Textureに独立U／V offset（-10〜10）を追加し、Material、Inspector、JSON保存／読込、material signatureへ接続。
- Rasterのsingle／multi-material shaderとRT material packetで個別offsetを適用。RT ray-cone mip密度はscaleを維持し、translationはtexture sample位置だけを変える。
- **未確認:** shader compilation、GPU runtime、Raster／RT pixel parityは未検証。

## 2026-10-14 Emission Texture UV offset

- Emission Textureに独立U／V offset（-10〜10）を追加し、Material、Inspector、JSON保存／読込、material signatureへ接続。
- Rasterのsingle／multi-material shaderとRT material packetでsample位置にoffsetを加える。RTのray-cone mip密度はoffsetに依存しないため、scale由来の選択を保つ。
- **未確認:** shader compilation、GPU runtime、Raster／RT pixel parityは未検証。

## 2026-10-14 Occlusion Texture UV offset

- Occlusion Textureに独立U／V offset（-10〜10）を追加し、Material、Inspector、JSON保存／読込、material signatureへ接続。
- Rasterのsingle／multi-material shaderとRT material packetでsample位置へoffsetを適用。RT ray-cone mip密度はtranslationの影響を受けない。
- **未確認:** shader compilation、GPU runtime、Raster／RT pixel parityは未検証。

## 2026-10-14 Base Color Texture UV offset

- Base Color Textureに独立U／V offset（-10〜10）を追加し、Material、Inspector、JSON保存／読込、material signatureへ接続。
- Rasterのsingle／multi-material描画、masked shadow alpha clip、RT closest-hitとany-hit alpha clipで同じoffsetを使う。RT ray-cone mip選択はtranslationに影響されない。
- **未確認:** shader compilation、GPU runtime、Raster／RT pixel parityは未検証。

## 2026-10-14 Opacity Texture UV offset

- Opacity Textureに独立U／V offset（-10〜10）を追加し、Material、Inspector、JSON保存／読込、material signatureへ接続。
- Raster通常描画とshadow alpha clip、RT masked alpha clipでsample位置に同じoffsetを適用。RT mip密度はoffsetの影響を受けない。
- **未確認:** shader compilation、GPU runtime、Raster／RT pixel parityは未検証。

## 2026-10-14 Base Color Texture UV rotation

- Base Color Textureに独立UV rotation（-360〜360°）を追加し、Material、Inspector、JSON保存／読込、material signatureへ接続。
- Rasterのsingle／multi-material描画とshadow alpha clip、RT closest-hitとany-hit alpha clipに適用。RT ray-cone mip選択は回転後のUV edgeから評価する。
- **未確認:** shader compilation、GPU runtime、Raster／RT pixel parityは未検証。

## 2026-10-14 Emission Texture UV rotation

- Emission mapに独立UV rotation（-360〜360度）を追加し、Material、Inspector、layer JSON、material signature、single／multi-material Raster、RTへ接続。
- RasterとRTはscale後・offset前のUV中心を回転する。RT ray-cone mip選択も回転後のtriangle UV edgeから評価する。
- 既定0度では従来のscale／offset応答を保つ。
- **未確認:** shader compilation、GPU runtime、Raster／RT pixel parityは未検証。

## 2026-10-14 Metallic/Roughness Texture UV rotation

- Metallic/Roughness mapに独立UV rotation（-360〜360度）を追加し、Material、Inspector、layer JSON、material signature、single／multi-material Raster、RTへ接続。
- RasterとRTはscale後・offset前のUV中心を回転する。RT ray-cone mip選択も回転後のtriangle UV edgeから評価する。
- 既定0度では従来のscale／offset応答を保つ。
- **未確認:** shader compilation、GPU runtime、Raster／RT pixel parityは未検証。

## 2026-10-14 Opacity Texture UV rotation

- Opacity mapに独立UV rotation（-360〜360度）を追加し、Material、Inspector、layer JSON、material signature、single／multi-material Raster、shadow alpha clip、RT any-hit alpha clipへ接続。
- RasterとRTはscale後・offset前のUV中心を回転する。RT any-hitのray-cone mip選択も回転後のtriangle UV edgeを使う。
- 既定0度では従来のscale／offset応答を保つ。
- **未確認:** shader compilation、GPU runtime、shadow／RT alpha silhouette parityは未検証。

## 2026-10-14 Occlusion Texture UV rotation

- Occlusion mapに独立UV rotation（-360〜360度）を追加し、Material、Inspector、layer JSON、material signature、single／multi-material Raster、RT closest-hitへ接続。
- RasterとRTはscale後・offset前のUV中心を回転する。RTのray-cone mip選択にも回転後のtriangle UV edgeを使う。
- 既定0度では従来のscale／offset応答を保つ。
- **未確認:** shader compilation、GPU runtime、Raster／RT pixel parityは未検証。

## 2026-10-14 Raster MaterialState ABI size audit

- `MeshRenderer::MaterialState` とRaster main／shadowの `MaterialParams` cbufferは、それぞれ27個のfloat4で同じ順序であることを静的に数えた。
- 以前の72／76-float static assertionは構造体サイズと矛盾していたため、108-float（27×4）へ修正した。
- **未確認:** build／shader compileは未実行。リポジトリのビルド実行ルールに従っている。

## 2026-10-14 Raster MaterialState ABI size audit

- `MeshRenderer::MaterialState` とRaster main／shadowの `MaterialParams` cbufferは、それぞれ27個のfloat4で同じ順序であることを静的に数えた。
- 以前の72／76-float static assertionは構造体サイズと矛盾していたため、108-float（27×4）へ修正した。
- **未確認:** build／shader compileは未実行。リポジトリのビルド実行ルールに従っている。

## 2026-10-14 Raster multi-material scalar state and maps

- `MeshRenderer::MaterialRange`がshader `MaterialParams`と同じ並びの`MaterialState`を保持する。PSOの`MaterialParams`をdynamic SRB variableへ変更し、material rangeごとのGPU bufferを割り当てる。
- material stateが変わった場合だけ既存slot bufferへ`UpdateBuffer`し、drawごとにはSRBのconstant-buffer参照を切り替える。通常のsingle-material drawはrenderer共有bufferを引き続き使う。
- Artifact rendererからbaseColor／opacity／emission／metallic／roughness／normal strength／AO strength／specular／IOR／transmission／clearcoat／sheen／alpha cutoff／UV transform／environment intensityをslot bufferへ渡す。
- QColor由来のBase Color slot factorをshader境界でsRGBからlinearへdecodeし、sRGB SRVでlinearizedされるBase Color textureとlinear空間で乗算する。RT material pathの既存変換と同じ契約。
- Base Color／Emission mapはsRGB viewとlinear-light mip chain、Metallic-Roughness／Normal／AO mapはlinear view、Opacity mapはRGB luminanceをalphaへ展開したlinear RGBA viewで読み込む。normal mipでは各levelのvectorを再正規化する。
- 各map pathをmaterial slotごとに解決し、primary materialのSRVや先行slotの同一pathを再利用する。slot draw時にMaterialParamsと6種のtexture SRVを切り替える。空pathおよびload失敗は各mapの白fallbackへ戻る。
- material rangeのalpha modeを個別に扱い、Opaque／Maskedを不透明passで先に描画し、Blendedまたはalpha付きOpaque rangeを後段のtransparent passで描画する。共有object／instance opacityはnon-masked rangeに適用する。
- Opaque PSOではblendを無効化し、transparent PSOだけsource-over blendを有効にする。Maskedはmaterial graph override後のalphaをcutoffし、通過sampleの出力alphaを1に固定する。
- Raster／Ray TracingのPBR texture samplerはadapterが報告する最大anisotropyを参照し、最大16xのanisotropic minification／mip filteringを使う。非対応adapterではlinear filteringへfallbackする。
- **Masked shadow:** shadow専用vertex/pixel shaderでUV・頂点alpha・instance alphaを伝え、slot別MaterialParams／Base Color／Opacity SRVを使って同じUV transformとalpha cutoffを評価する。
- **未確認:** Opaque／transparent runtime、Vulkan／D3D12 buildとpixel parityは未確認。slotごとのalpha modeが透明pass選択で正しく分離されるか、masked cutoutのshadow silhouetteが実画素で一致するか要検証。

## 2026-10-14 RT AO texture + mesh UV

- RT custom shading poolにmesh UVを追加し、triangle hitのbarycentric座標で補間する。
- AO mapを読み込み、R8 mip chainをcold pathで作成し、RT texture descriptor arrayへ登録する。materialごとのUV scale／rotation／offsetとocclusion strengthを適用し、拡散IBL／ambient／specular IBLへAOを反映する。
- texture配列のshader容量は必要時に2の累乗で増やし、PSO／SBTを再構築する。Material slotごとのmapを保持し、descriptor indexをflatten済みRT materialへ格納する。
- Vulkan acceptance fixtureで左右に異なるAO texelを配置したmapをUV付きquadへ適用。left/rightの画素差を検証し、fixture単独1/1、RayTracingAcceptanceTest executable全体を実行して成功（25.20秒）。`ArtifactRender` targetもビルド成功。
- **当時の未完了項目:** RT base color／metallic-roughness／normal textureのresource bindingは、後続の「RT base color + metallic-roughness maps」でbase colorとmetallic-roughnessを実装した。AO用R8 loaderは8-bit／16-bit／float画像に対応するが、float値は0..1へclampして格納する。

## 2026-10-14 RT base color + metallic-roughness maps

- AO／base color／metallic-roughnessを共通のRT texture descriptor arrayへ登録し、slotごとのtexture indexでclosest-hit shaderから参照する。
- Base color mapはRGBA8 sRGB viewとしてサンプルし、scalar base color（sRGB→linear）へ乗算。metallic-roughness mapはlinear RGBA8で保持し、Bをmetallic、Gをroughnessのscalar値へ乗算する。Materialの共通UV transformは3種すべてへ適用する。
- テクスチャを追加した時に配列容量を拡大し、RT PSO／SBTを再構築する。異なるmap数への変更時も参照を更新する。
- Vulkan acceptanceのUV quadに左右の赤／青base colorと金属／誘電体metallic mapを与え、色領域とmetallic map変更による画素差を確認。fixture単独1/1、RayTracingAcceptanceTest executable全体が成功（28.44秒）。`ArtifactRender` targetもビルド成功。
- **当時の未完了項目:** normal mapのtangent frame／mappingは後続の「RT normal map + triangle tangent basis」で実装した。他texture map、実制作mesh/HDRでのRaster／RT parity、texture LOD選択は未完了。現状RT texture sampleは明示LOD 0を使う。

## 2026-10-14 RT normal map + triangle tangent basis

- Material slotごとのnormal mapとnormal strengthをRT material packetへ追加し、Rasterと同じlinear UNORM texture扱いにする。縮小mipはnormal vectorを再正規化する。
- Hit triangleのposition／UVからtangent basisを再構成し、Object-to-Worldで接線を、inverse-transpose経路でnormalを変換する。共通UV scale／rotation／offsetを接線計算にも適用し、mirrored UVのhandednessを保持する。
- Vulkan GPU fixtureでquad左側にflat normal、右側に+X tangent normalを与え、左右の照明差を確認。normal strength 0ではmapなしの結果へ戻る。fixture単独1/1、RayTracingAcceptanceTest executable全体が成功（26.22秒）。`ArtifactRender` targetもビルド成功。
- **未完了:** 大きなscreen-space縮小時のtexture LOD／aliasing、滑らかなtangent smoothingのRasterとの一致、normal mapを含む実制作mesh/HDRでのRaster／RT比較。現状texture sampleは明示LOD 0。

## 2026-10-14 RT ray-cone texture mip selection

- Ray-generation shaderがvertical FOVとdispatch heightからpixel単位のray-cone spreadをpayloadへ渡し、closest-hitでhit距離を掛けてworld-space footprintを推定する。
- triangleのworld-space edgeとmaterial UV transform後のedgeからtexture-space最大密度を計算し、各base-color／metallic-roughness／normal／AO／emission textureのサイズに合わせてLODを選択する。LODは各textureの有効mip範囲へclampし、mipなしまたは退化triangleはLOD 0を使う。
- Vulkan GPU acceptanceで128×128の1-pixel checkerを小さなUV quadへ割り当て、縮小後のpixelがlinear-light平均値のuniform mapと一致することを5箇所で検証。専用fixtureと全RayTracingAcceptanceTestが成功し、後者は29.30秒。`ArtifactRender` targetもビルド済み。
- **未完了:** ray-coneはpixel footprintの近似で、極端な斜面・off-axis・非一様instance scaleにおけるraster derivativeとの完全一致は未検証。実制作mesh/HDRでaliasingと画質を比較し、smooth tangent interpolationも評価する。

## 2026-10-14 RT Principled emission texture

- `Material::emissionTexture`を `RTPbrMaterial` 経由でRendererからRT material slotへ渡す。
- Emission mapはbase colorと同じsRGB decode／linear-light mip生成で共通PBR texture配列へ登録する。closest-hitはbase emission colorとstrengthへtextureのlinear RGBを乗算し、ray-coneから選んだLODでサンプルする。
- Vulkan GPU acceptanceで左右の異なる赤／緑emission texelがそれぞれのquad領域へ反映されることを確認。fixture単独成功、全RayTracingAcceptanceTestは29.76秒で成功。`ArtifactRender` と受け入れテストtargetをビルド。
- **未完了:** opacity mapとalpha/transparent geometryのRT交差、emission強度が1を超えるHDR emission mapの入力・mip契約、実制作mesh/HDR上でのRaster／RT parityは未確認。

## 2026-10-14 Ray Tracing material slots

- `Mesh::MaterialSlot` のindex rangeをtriangleごとのmaterial indexへ展開し、RT custom shading primitiveへ保持する。
- RT material structured bufferを複数slot分flattenし、closest-hit shaderがhit primitiveのmaterial indexからPrincipled scalar値を選択する。slot別にbase color／metallic／roughness／specular／IOR／transmission／clearcoat／sheen／emissionを反映。
- Vulkan acceptanceの2 triangle fixtureで赤／青emissionを左右別に描画し、pixel selectionを確認。既存を含むRayTracingAcceptanceTestは7/7成功。
- **当時の未完了項目:** UV／AO mapは後続セクションで実装済み。base color／metallic-roughness mapも実装済み。normal mapのRT対応と実制作シーンでのRaster／RT比較は引き続き未完了。

## 2026-10-14 Roughness-aware specular occlusion

- Raster MeshRenderer のIBL specular occlusionを、NdotVとAOだけを使うroughness非依存の式からLagarde specular AOへ変更。perceptual roughnessが高いほど反射ローブが広がる性質を加味し、同じAOでもrough材の環境反射を過剰に消さない。
- `ArtifactCoreRayTracingAcceptanceTest` を再ビルド。GPU画素検査は1×1 grayscale AO mapをAO 0/64/128/192/255、roughness 0.04/0.2/0.5/0.9で掃引し、全20組の反射残存率がLagarde式の予測値に合うことを確認。AO応答は単調で、roughness方向は `NdotV + AO` が1のどちら側かに応じた式の傾向と一致。加えてUV付き三角形へ4 texelのAO gradientを適用し、左右pixelで強いAO側の反射保持率が低いこと、U offset 0.5でサンプリング位置が移り左右の大小が反転することを確認。Vulkan acceptance 6件が成功。
- **当時のRT経路監査:** `Mesh::generateRenderData()` はUVとmaterial slotを返す一方、RT custom shading geometryはposition／normalのみでUV textureをsampleしていなかった。後続の「RT AO texture + mesh UV」節でAO textureとUVの伝搬を実装した。
- **当時の次段階:** AO mapの実画素反映をproofとし、base color／metallic-roughness／normalへ同じresource契約を拡張する計画だった。単一平均AO値での代用は空間的なmaterial応答を失うため採用しなかった。
- **現状:** RTのAO texture／UV sampleとroughness-aware specular occlusionは実装済み。実制作mesh／HDRによるRaster／RT parityと、texture更新・再構築寿命のruntime受入れは未検証。

> 2026-03-31 作成

## 2026-10-02 現行コード再監査

- `ArtifactLayerPanelWidget.cppm` の 3D Material menu は `applyMaterialPreset` を通し、Matte／Metal／Plastic／Glass の色・metallic・roughness・specular／transmission／IOR 値を既存 Undo command で適用する。
- よって Material preset 適用はコード実装済みとして記録する。独立 Material Browser／asset 管理は確認できず未実装。preset 適用、透明境界、texture更新の runtime 検証も未実施。
- [x] **実装済み（2026-10-02）:** 同名preset値の分岐を解消するため、CoreにLayer Panel既存値を基準とする`Material::makeStudioPreset()`を追加。Layer PanelとAI `MaterialAutomation`のMatte／Metal／Plastic／Glass適用を同じfactoryへ集約し、AI適用はspecular／transmission／IORもlayer propertyへ反映する。旧汎用`makeMetal`等の挙動は維持。
- [x] **実装済み（2026-10-02）:** Layer Panelの3D Material context menuからpreset browserを開き、共有factory由来のroughness／metallic／transmission／IORを表示して選択適用できる。適用は既存`applyLayerPropertyValues()`でUndoへ通す。
- **未完了:** 独立した保存可能Material assetのBrowser／asset管理、高度mapping、およびpresetの実行時shading確認。

## 2026-10-13 Principled Sheen roughness / tint

- `ArtifactCore::Material` に sheen roughness（default 0.5）と sheen tint（default white）を追加し、3D layer JSON、Inspector、material signatureから編集・保存する。
- MeshRenderer と Ray Tracing の両方で Charlie sheen の直接光分布／visibilityを使い、Charlie importance samplingで生成したsheen専用prefilter cubemapとdirectional-albedo LUTでIBLを評価する。LUTは64x64、各点128サンプルでCharlie BRDFの方向反射率を積分し、既存GGX BRDF LUTのB channelへ格納する。sheen環境キューブは既存GGX環境キューブとは独立してroughness mipを生成する。
- 基底層のenergy compensationはKhronosの方向反射率albedo scalingに合わせ、directはview/light側の大きいE、IBLはview Eを使う。coatはsheenの上層としてsheen寄与を減衰する。Vulkan acceptanceではroughnessとtintを変えた実画素、Raster/RT parity、sheen専用環境prefilter接続を確認した。
- **未完了:** 有限解像度／サンプル数による近似誤差と環境ロード時間の計測、実制作HDR・異なるview角度・布／サテン相当材の視覚レビュー、Material Browser／asset管理。Blender Cyclesのmicrofiber sheenと同一の散乱モデルを実装したものではない。

## 2026-10-14 GGX split-sum LUT integration correction

- GGX half-vector importance samplingのPDF変換に必要な `VdotH / (NdotH * NdotV)` をBRDF LUT積分へ追加し、LUTのSmith-Schlick geometry termを直接光の式と一致させた。
- 以前の積分はこのPDF補正を欠き、環境specularの強さとview-angle応答を偏らせていた。RasterとRTは同じLUTを使うため、生成側の修正を両経路へ共有する。
- 生成した64×64 RGBA32F LUTをVulkanからreadbackし、roughness row 4／22／54とNdotV column 4／32／60の9点でfinite値、single-scatter energy上限、grazing応答、roughnessによるenergy差を確認。row 22／54の6係数は256×512 uniform-hemisphere reference、低roughness row 4の3係数はGGX slope空間の独立Gauss-Legendre 96×512積分で照合した。48×256から96×512への最大収束差は0.000002。9点の最大係数誤差は0.005048、許容値は0.015。Stinson HDRのRaster／RT parityも確認した。
- **未完了:** Stinson HDR以外の複数HDR、別形状／複数カメラ角度、製品cameraと共通tone mappingでの視覚レビュー。固定fixtureの数値検査とdiagnostic captureだけでは製品上の一般的な見た目を証明しない。

## 2026-10-14 Clearcoat IBL split-sum補正

- RT経路のclearcoat環境反射は、BRDF LUTの `A` 項に固定F0=0.04ではなく角度依存Schlick Fresnelを渡していた。split-sum係数 `F0*A+B` の前提に従い、Rasterと同じ固定coat F0で評価するよう修正。
- NdotV≈0.5のHDR material probeへclearcoat有効ケースを追加し、Raster／RT画素比較でこの経路を監視する。
- **検証:** 変更後の受け入れターゲットをビルド。初回実行は出力DLLのある`out/build/x64-Debug/bin/Debug`がPATHになく停止した。PATHを補って追加probe単独を実行し1/1成功、同じ環境でVulkan `ArtifactCoreRayTracingAcceptanceTest` 5/5成功（13.64秒）。
- clearcoat有効のNdotV≈0.5 probeはRaster／RT画素比較を通過し、低roughness row 4は独立slope積分で照合済み。
- **未完了:** clearcoat roughness／view angleを広く振った実制作HDR上の視覚レビュー。

## 2026-10-14 Stinson HDR shader-ball visual review

- Vulkan acceptanceに24×48分割のsphereを追加し、OpenUSD同梱Stinson Beach HDR（1024×512）下でdielectric roughness 0.04／0.20／0.50／0.85とmetal roughness 0.24をRaster／RT各256×256で保存する。両経路は同じ48° perspective cameraを使う。PNGは診断用ACES tone mapを通し、HDR radianceを1で直接clipしない。
- 画像を目視し、dielectric roughnessが上がるほど環境highlightが広がり、metal probeでは反射色が金色側へ変わることを確認。中心pixelのlinear RGB最大絶対差はdielectric 4条件で0.003283以下、metalで0.042740。全5条件が0.05以内。新fixture単独1/1、受け入れ全6件がVulkanで通過（21.31秒）。
- **比較上の制約:** raster clearは暗色、RT missはHDR environment背景なので全画面の背景は異なる。center pixel parityとsurfaceの視覚比較に限る。照明は実HDR IBL中心で、製品Compositionの露出・tone mapping・studio lighting設定を再現していない。
- **未完了:** 実制作HDRの複数枚、複数面／view angle、面全体のnormal/mapping/clearcoat/sheen、および製品Compositionの露出／tone mappingを含む確認。

## 2026-10-14 Ray Tracing HDR world background and rotation parity

- RT miss shaderは環境照明が有効でも固定の青いsky gradientを背景に返していた。HDR environment cubemap、同じrotation、intensityを使って背景をサンプルし、environment無効時のみ従来のgradientへfallbackする。
- RT reflection／diffuse／transmissionのyawは、forward shaderと同じ正の角度を渡したときHDR反射を揃えるため、ray方向側で逆向きのXZ回転行列を使う。forward側の行列をそのまま使うと31度でmetal center差が0.056733となり既存閾値0.05を超えた。RT式を逆向きにすると同じ31度で5材質すべて0.05以内、最大差0.014566になった。
- shader-ball fixtureはRasterとRTへ同じ31度を渡してcenter pixel parityを確認する。material sweep後はRTのrotationを0度へ更新し、球体外周のHDR world backgroundが0.02以上変化するassertionでmiss-stage settingsの更新を確認する。
- 修正後に`ArtifactCoreRayTracingAcceptanceTest` targetをbuildし、Vulkan acceptance全6件が成功（21.43秒）。roughness 0.04／0.20／0.50／0.85のcenter最大RGB差は0.014566以下、metal roughness 0.24は0.011357。
- 更新したRT captureでStinson Beach HDR背景を目視確認した。画像は`ray-tracing-frames/pbr-stinson-sphere-*-rt.png`に保存される。
- **未完了:** 回転したsurface reflectionのRaster／RT数値parity、Raster側もHDR backgroundを描く全画面比較、製品Compositionのcamera／露出／tone mapping／studio lightingを通した確認。保存済みRaster captureのclearは暗色のまま。

## 2026-10-14 HDR environment generation cost

- `rawEnvironmentChannel()` のformat判定／bounds check／decodeをbilinear tapごとに繰り返さないよう、入力HDRを一度だけcontiguous RGBA floatへ展開。Cubemap、GGX／Charlie prefilter、diffuse irradianceの独立pixel生成を既存`Core.Parallel`共有arenaへ分配した。
- Vulkan test上のStinson Beach HDR（1024×512）`setEnvironmentMap()` CPU callは最初の最適化前12.373秒、64-sample LUT時5.658秒と5.633秒（平均5.645秒）、128-sample LUT時6.252秒と6.434秒（平均6.343秒）。128 samplesは64 samplesより約12%長いが、初期値より約49%短い。GPU completion waitは含まず、各構成2回なので性能保証値ではない。
- RGBA float stagingはこのHDRで約8 MiB。環境のロード時だけに確保し、フレーム処理には持ち込まない。変更後のRay Tracing acceptanceは5件成功。

## 現行コード監査 (2026-08-20)

- `ArtifactCore::Material` は PBR の base color／emission／metallic／roughness／alpha、normal strength、occlusion、opacity、sheen、各種 texture path を保持する。
- `Artifact3DModelLayer` は material を所有し、Inspector の Material group、JSON 保存／復元、importer 由来の metallic-roughness／normal／occlusion texture、透明材質判定、material signature を提供する。
- CompositionRenderController／Diligent render window 側には PBR parameter、metallic-roughness／normal texture、opacity／depth 契約、Light Layer 連携が存在する。
- したがって旧 Phase 1〜4 の「作成予定」は現状と一致しない。Layer Panel の 3D Material メニューに Matte／Metal／Plastic／Glass のプリセット適用を追加した。Sheen は Material／Inspector／JSON／MeshRenderer の直接光・IBL 経路まで接続済み。残りは Material Browser／asset 管理の独立導線、高度な mapping、実機での shading／透明境界／texture 更新受入れ。

## Update 2026-08-20

現行コードを追加確認した。`ArtifactCore::Material` は base color／emission／metallic／roughness／alpha、normal strength、occlusion、opacity、sheen、texture path を保持し、`Artifact3DModelLayer` の Inspector／JSON／import material 情報と接続されている。CompositionRenderController／Diligent renderer にはPBR parameter、metallic-roughness／normal texture、opacity／depth、Light Layer連携がある。Sheen は直接光・IBL の shader 経路へ接続されている。

未完了・未検証なのは、Material Browser／asset管理の独立導線、高度なmapping、実機でのshading、透明境界、texture更新受入れである。Layer Panel の Material menu から Matte／Metal／Plastic／Glass を適用する preset は実装済み。基本Material／PBR接続と preset 適用はコード上で確認済み、拡張UXとruntime検証は pending とする。

## 目的

3D objects の material を定義し、適切な shading と appearance を実現する。

## 背景

3D rendering の質を決める重要な要素が material system。
Primitive 3D Render Path の solid shading の続きとして、material による見た目の制御が必要。
基本的な diffuse/specular/reflection のサポートから始め、texture mapping も含む。

## 対象

- Basic material properties (diffuse color, specular, roughness)
- Texture mapping (diffuse map, normal map)
- Material assignment to 3D objects
- Simple PBR-like shading
- Material inspector integration

## 実装方針

### 原則

1. Material を独立した asset として扱う
2. 3D layer に material を assign
3. GPU shader での material application
4. シンプルな UI から始める

### 対象 API

- `ArtifactCore::Material3D` class
- `Artifact3DLayer::setMaterial()`
- Shader の material parameter

## Phase 1: Basic Material Properties

### 目的

Material の基本 properties を定義する。

### 作業項目

- Material class の作成 (diffuse, specular, roughness)
- Color picker integration
- Material asset management

### 完了条件

- Material の color properties を設定できる

## Phase 2: Texture Support

### 目的

Texture mapping を追加する。

### 作業項目

- Diffuse texture assignment
- Texture coordinate generation
- UV mapping basics

### 完了条件

- 3D objects に texture を適用できる

## Phase 3: Shader Integration

### 目的

Material を GPU shader で使用する。

### 作業項目

- Material parameter を shader に渡す
- Basic lighting calculation
- Material switching in renderer

### 完了条件

- Material が rendering に反映される

## Phase 4: Material Assignment

### 目的

3D layer に material を assign する。

### 作業項目

- 3D layer の material property
- Material browser integration
- Default material handling

### 完了条件

- 各 3D object に異なる material を設定できる

## Phase 5: Advanced Features

### 目的

高度な material features を追加する。

### 作業項目

- Normal mapping
- Specular mapping
- Material presets

### 完了条件

- 基本的な PBR material が使用可能

## 連携先

- `ArtifactCore/include/Material/Material3D.ixx`
- `ArtifactCore/src/Material/Material3D.cppm`
- `Artifact/src/Layer/Artifact3DModelLayer.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`

## Recommended Order

1. Phase 1
2. Phase 2
3. Phase 3
4. Phase 4
5. Phase 5

---

## Next Execution Slice

Phase 1 は material の器を先に作り、見た目の自由度を少しずつ上げる。

### Phase 1A の着手点

1. `Material3D` の基本 property を切り出す
2. diffuse / specular / roughness を最小セットとして定義する
3. material assignment の前に inspector 側の編集導線を確認する
4. 既存の solid 表示を壊さない範囲で導入する

### Phase 1 完了条件

- material の色属性を設定できる
- material の基本値が 1 つの構造にまとまる
- solid 表示の既存挙動を壊さない

### Phase 2 の前提

- texture support は material の基本属性が安定してから入れる
- shader integration は parameter 契約が固まってから進める
- material assignment は editor 側の編集導線が見えてから重ねる

### Phase 3 への波及

- renderer に渡す material parameter の形を先に揃える
- camera / projection と material の責務を混ぜない
- simple PBR-like shading は後段で検討する

## Implementation Status (2026-07-25)

- `ArtifactCore::Material` が base color、metallic、roughness、opacity、emission、normal/occlusion を保持する。
- `Artifact3DModelLayer` が material を保存・復元し、Property Editor の編集値を反映する。
- base-color、metallic-roughness、normal、emission、occlusion、opacity texture の読み込み経路がある。
- `MeshRenderer` が material constant buffer と PBR-like shader を使用して描画する。
- Phase 5 の高度な normal/specular mapping、独立した Material Browser／asset 管理 UI、および実機確認は未完了。Matte／Metal／Plastic／Glass の Layer Panel preset 適用は実装済み。
- Principled互換の独立Coat Normal TextureをMaterial JSON／Property／single・multi-material Raster／RTへ追加。未ビルドのためshader compileと画質確認は未完了。
- Mesh vertex colorをArtifactIRendererからRT shading geometryへ渡し、RT closest-hitのbase colorとany-hitのMasked alphaに反映。Rasterと同じRGB sRGB→linear変換を適用する。shader compileと画像parityは未確認。
- Base Color Texture専用のUV uniform scale（0.01〜10）をMaterial／Property／JSON／signature／single・multi-material Raster／RTへ追加。Rasterの通常描画・shadow alpha clipとRTのclosest-hit／any-hitで同じscaleを使い、RT ray-cone mip選択にも反映する。offset／rotationおよび他textureの個別mappingは未対応、shader compileと画像parityは未確認。
- Metallic/Roughness Texture専用のUV uniform scale（0.01〜10）をMaterial／Property／JSON／signature／single・multi-material Raster／RTへ追加。Rasterのimplicit texture derivativesとRT ray-cone mip選択にscaleを反映する。offset／rotationおよび他textureの個別mappingは未対応、shader compileと画像parityは未確認。
- Normal Texture専用のU／V offset（-10〜10）をMaterial／Property／JSON／signature／single・multi-material Raster／RTへ追加。共通UV transform／Normal専用rotation・scaleの後にtranslationを適用し、RTのnormal map mipはrotation／scaleによるtexture密度を維持する。Coat Normalには適用せず、shader compileと画像parityは未確認。
- Occlusion Texture専用のuniform UV scale（0.01〜10）をMaterial／Property／JSON／signature／single・multi-material Raster／RTへ追加。RTはmapごとのray-cone mip選択へscaleを反映し、Rasterはimplicit derivativesで追従する。offset／rotation、shader compileと画像parityは未確認。
- Emission Texture専用のuniform UV scale（0.01〜10）をMaterial／Property／JSON／signature／single・multi-material Raster／RTへ追加。Rasterはimplicit derivatives、RTはmapごとのray-cone mip選択へscaleを反映する。offset／rotation、shader compileと画像parityは未確認。

## Static audit follow-up (2026-07-25)

- Base Color Textureへ独立UV rotation（-360〜360度）を追加し、Material／Inspector／JSON／signature／single・multi Raster／shadow alpha clip／RT closest-hit・any-hitへ接続。RT ray-cone mip選択にも回転後UVを使う。中心0.5まわりの回転で既定0度では従来応答を維持する。ビルドと画像parityは未確認。

- The implemented core type is `ArtifactCore::Material` (not the originally proposed `Material3D` name). It is assigned by `Artifact3DModelLayer`, persisted in JSON, exposed through the existing Property Editor, and included in the layer material signature/cache boundary.
- `MeshRenderer` binds base-color, opacity, metallic-roughness, normal, emission, and occlusion textures and sends the material factors through a GPU constant buffer. The shader applies tangent-space normal mapping, metallic/roughness response, occlusion, emission, alpha, and scene/studio lighting.
- Phases 1-4 and Matte／Metal／Plastic／Glass preset application are supported by static source evidence (`ArtifactLayerPanelWidget.cppm`, `applyMaterialPreset`). The remaining scope is an independent Material Browser/asset-management UI, advanced mapping, and runtime verification; no build or runtime execution was performed under the repository policy.
- Base Color mipの生成はsource／destination texel矩形の面積重みで縮小し、奇数寸法の端texelも含める。透明縁の色bleedを避けるためRGBはalpha重み付きlinear-light平均、alphaは面積平均とする。alphaをopacity用途にしない素材への影響とRaster／RT画像差は未確認。
- AO mapをsheen IBLにもroughness-aware specular-occlusion近似で適用し、Raster／RTで揃える。AO=1では不変。Charlie sheenに対する近似の画素検証は未実施。
- Normal Mapへ共通UV transform後の追加回転（-360〜360度）を追加し、Material／Layer JSON／Property／material signature／single・multi-material Raster／RTへ接続。Coat Normalも同じ追加回転を共有する。既定0度では従来応答を保つ。
- 高度なmappingの一段としてNormal Map専用追加回転をMaterial／Property／JSONとRaster／RTへ接続した。後続段階で各PBR mapのUV0／UV1選択も追加した。全map独立scale／offset／UV投影の拡張は引き続き未対応。
- Normal Map専用UV mappingへ一様scale（0.01〜10）を追加。Rasterはimplicit derivatives、RTはray-cone texture LODへscaleを反映する。Coat Normalは別のrotation／scaleを持つ。
- Coat Normalに専用UV rotation（-360〜360度）、uniform scale（0.01〜10）、UV0／UV1 selectorを追加し、Material／Inspector／JSON／signature／single・multi Raster／RTへ接続。RTはcoat tangent basisとray-cone LODも専用変換へ追従し、default UV0／0度／1倍では既存挙動を維持する。
- Coat Normal専用rotation／uniform scaleをMaterial・Inspector・JSON・single／multi Raster／RTへ追加。RTはcoat tangent basisとray-cone LODへ変換を反映し、Normal Mapとは独立してcoat detailを調整できる。画像検証は未実施。
