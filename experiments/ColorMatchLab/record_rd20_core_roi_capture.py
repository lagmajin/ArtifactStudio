from __future__ import annotations
import csv, hashlib, json
from pathlib import Path

ROOT = Path(__file__).resolve().parent
REPO = ROOT.parent.parent
LOG = REPO / 'temp' / 'rd24_core_full.log'
OUT = ROOT / 'captures' / 'rd20_core_skin_white'
CORE = REPO / 'ArtifactCore' / 'src' / 'Color' / 'AutoColorMatch.cpp'
TEST = REPO / 'tests' / 'ArtifactCore' / 'AutoColorMatcherContractTest.cpp'
ASSET = ROOT / 'assets' / 'WikimediaCC0' / 'Pexels-photo-707448-1280.jpg'
PREP = ROOT / 'captures' / 'rd9_real_skin_white_scene'
SCORED = ROOT / 'captures' / 'rd9_real_skin_white_scored'
METHODS = {'Reinhard','MeanStddev','Histogram'}
INTENSITIES = {round(i / 10.0, 7) for i in range(11)}
ROIS = {'face_skin','arm_skin','skin_combined','light_blouse_candidate','full_frame'}

def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

rows=[]
for line in LOG.read_text(encoding='utf-8',errors='replace').splitlines():
    if 'RD20_CORE_ROI,' not in line:
        continue
    line=line[line.index('RD20_CORE_ROI,'):]
    fields=next(csv.reader([line]))
    if len(fields)!=20:
        raise RuntimeError(f'Unexpected RD21 row with {len(fields)} fields: {line}')
    (_, method, intensity, roi, pixels, d_input, d_target, src_c, out_c, target_c,
     src_pixel_c, out_pixel_c, target_pixel_c, out_hue, p95_input, max_input,
     pct_input_gt2, p95_target, max_target, pct_target_gt2)=fields
    rows.append({'method':method,'intensity':float(intensity),'roi':roi,'pixels':int(pixels),
        'mean_deltaE76_to_input':float(d_input),'mean_deltaE76_to_synthetic_reference':float(d_target),
        'p95_deltaE76_to_input':float(p95_input),'max_deltaE76_to_input':float(max_input),
        'percent_pixels_deltaE76_gt2_to_input':float(pct_input_gt2),
        'p95_deltaE76_to_synthetic_reference':float(p95_target),'max_deltaE76_to_synthetic_reference':float(max_target),
        'percent_pixels_deltaE76_gt2_to_synthetic_reference':float(pct_target_gt2),
        'mean_vector_Cstar_input':float(src_c),'mean_vector_Cstar_output':float(out_c),
        'mean_vector_Cstar_reference':float(target_c),'pixel_mean_Cstar_input':float(src_pixel_c),
        'pixel_mean_Cstar_output':float(out_pixel_c),'pixel_mean_Cstar_reference':float(target_pixel_c),
        'output_hue_circular_mean_degrees':float(out_hue)})
if len(rows)!=len(METHODS)*len(INTENSITIES)*len(ROIS) or {(r['method'],round(r['intensity'],1)) for r in rows}!={(m,i) for m in METHODS for i in INTENSITIES} or {r['roi'] for r in rows}!=ROIS:
    raise RuntimeError(f'Incomplete RD20 matrix: {len(rows)} records')
manifest_source=json.loads((PREP/'manifest.json').read_text(encoding='utf-8'))
scored=json.loads((SCORED/'manifest.json').read_text(encoding='utf-8'))
OUT.mkdir(parents=True,exist_ok=True)
log_copy=OUT/'core_roi_test_output.log'
log_copy.write_bytes(LOG.read_bytes())
metrics={'rd':'rd22-core-portrait-synthetic-cast-strength-and-tail-sweep','status':'measured Core output; exploratory, no acceptance gate','conditions':{'methods':sorted(METHODS),'intensities':sorted(INTENSITIES),'regions':sorted(ROIS),'condition_count':len(METHODS)*len(INTENSITIES),'roi_measurement_count':len(rows),'resolution':manifest_source['source']['dimensions'],'test_route':'ArtifactCore::AutoColorMatcher ImageF32x4_RGBA overload; Core measurement emitted by the authorized Debug contract test'},'measurements':{'color_space':'CIE Lab D65/2-degree computed from clamped display-sRGB values','deltaE76':'mean, nearest-rank p95, max, and fraction above 2 ΔE76 per pixel, separately to unchanged source and constructed synthetic reference; 2 is descriptive only and not a quality threshold','Cstar':'C*ab of ROI mean Lab vector and mean of per-pixel C*ab','hue':'circular mean of output per-pixel Lab hue; low-chroma fabric hue is unstable and descriptive only','alpha':'preservation checked by test against unchanged source alpha'},'data':rows,'interpretation_limits':['same CC0 portrait was used to construct its reference with a fixed per-channel cast; not a photographic WB pair or physical target','face/arm/blouse ROI masks were analyst-reviewed before previous source-mirror scoring, not independently annotated','light blouse is patterned/shaded and not measured neutral white','2 ΔE76 exceedance is a descriptive reporting cutoff only; no quality threshold, blind review, or method winner assigned','no ICC/profile-aware conversion or app/render route/performance assessment']}
metrics_path=OUT/'metrics.json'
metrics_path.write_text(json.dumps(metrics,indent=2)+'\n',encoding='utf-8',newline='\n')
files={'core_roi_test_output.log':sha(log_copy),'metrics.json':sha(metrics_path)}
curve_path=OUT/'strength_sweep_skin_curves.png'
if curve_path.exists(): files[curve_path.name]=sha(curve_path)
manifest={'rd':metrics['rd'],'status':metrics['status'],'test':'AutoColorMatcherContractTest.CoreImageOverloadMeasuresRealPortraitSyntheticCast','source':{'path':'experiments/ColorMatchLab/assets/WikimediaCC0/Pexels-photo-707448-1280.jpg','sha256':sha(ASSET),'license':manifest_source['source']['license'],'dimensions':manifest_source['source']['dimensions']},'reference':{'generation':manifest_source['synthetic_reference']['operation'],'scale':manifest_source['synthetic_reference']['scale'],'offset':manifest_source['synthetic_reference']['offset'],'role':manifest_source['synthetic_reference']['status']},'regions':{name:{'pixels':item['pixels'],'mask_sha256':item['mask_sha256']} for name,item in scored['regions'].items() if name!='full_frame'},'inputs':{'prep_manifest_sha256':sha(PREP/'manifest.json'),'scored_mirror_manifest_sha256':sha(SCORED/'manifest.json'),'core_source_sha256':sha(CORE),'test_source_sha256':sha(TEST)},'evidence_scope':'actual Core image-overload output measurements on the loaded CC0 portrait and an in-memory synthetic cast reference; not a C++/Python numerical differential; visualization plots metrics only','files':files,'metrics_sha256':sha(metrics_path),'core_png_outputs':{path.name:sha(path) for path in sorted((ROOT/'captures'/'rd22_core_outputs').glob('*.png'))},'visual_review_protocol':{'script_sha256':sha(ROOT/'make_rd22_visual_review.py')}}
(OUT/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8',newline='\n')
print(json.dumps({'capture':str(OUT),'rows':len(rows),'files':files,'pins':manifest['inputs']},indent=2))
