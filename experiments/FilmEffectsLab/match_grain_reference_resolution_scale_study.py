from __future__ import annotations
import hashlib,json,platform
from pathlib import Path
import cv2
import numpy as np
from PIL import Image,ImageDraw
ROOT=Path(__file__).resolve().parent; REPO=ROOT.parent.parent
PLAN=ROOT/'plans/match-grain-reference-resolution-scale-r1.json'
SOURCE=REPO/'Artifact/src/Effect/ArtifactCreativeEffects.cpp'
OUT=ROOT/'captures/match-grain-reference-resolution-scale-r1'; OUT.mkdir(parents=True,exist_ok=True)
TEXT=SOURCE.read_text(encoding='utf-8')
markers={
 'aux_same_size_clone':'output = wrapped.clone();' in TEXT,
 'aux_resize_inter_linear':'cv::resize(wrapped, output, cv::Size(width, height), 0.0, 0.0,\n                   cv::INTER_LINEAR);' in TEXT,
 'grain_residual_gaussian':'cv::GaussianBlur(channels[channel], lowFrequency, cv::Size(),\n                         std::max(0.1f, impl_->grainSize));' in TEXT,
 'residual_subtraction':'residual[channel] = channels[channel] - lowFrequency;' in TEXT,
 'source_alpha_copy':'output[offset + 3] = source[offset + 3];' in TEXT,
 'response_equation':'1.0f + (1.0f - std::clamp(sourceLuma, 0.0f, 1.0f))' in TEXT,
 'grain_additive_mix':'output[offset + channel] = source[offset + channel] +\n                                            grain * impl_->mix;' in TEXT,
}
if not all(markers.values()): raise RuntimeError(markers)
SEED=20261112; BASE=(1024,512); REF_SIZES=((256,128),(512,256),(1024,512)); TARGET_SIZES=((512,256),(1024,512)); SIGMAS=(1.15,3.0)
AMOUNT=np.float32(1); LUMA_AMOUNT=np.float32(1); CHROMA_AMOUNT=np.float32(.65); MIX=np.float32(1); SOURCE_VALUE=np.float32(.5)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def corr(a,b):
 a=np.asarray(a,np.float64); b=np.asarray(b,np.float64); a=a-a.mean(); b=b-b.mean(); d=np.sqrt(np.mean(a*a)*np.mean(b*b)); return float(np.mean(a*b)/d) if d else 0.0
def acf_lengths(field):
 h,w=field.shape; y0,y1=h//8,7*h//8; x0,x1=w//8,7*w//8; roi=field[y0:y1,x0:x1].astype(np.float64); roi-=roi.mean(); variance=np.mean(roi*roi)
 if variance==0:return {'lag1_x':0,'lag1_y':0,'first_1e_x_px':0,'first_1e_y_px':0,'x_curve':[],'y_curve':[]}
 maxlag=min(32,roi.shape[1]//3,roi.shape[0]//3); xc=[]; yc=[]
 for lag in range(maxlag+1):
  xc.append(float(np.mean(roi[:,:-lag or None]*roi[:,lag:])/variance) if lag else 1.0)
  yc.append(float(np.mean(roi[:-lag or None,:]*roi[lag:,:])/variance) if lag else 1.0)
 def crossing(v):
  for i,val in enumerate(v[1:],1):
   if val<=1/np.e:return i
  return None
 return {'lag1_x':xc[1] if len(xc)>1 else 0,'lag1_y':yc[1] if len(yc)>1 else 0,'first_1e_x_px':crossing(xc),'first_1e_y_px':crossing(yc),'x_curve':xc,'y_curve':yc}

rng=np.random.default_rng(SEED)
# One broad, bounded, neutral reference texture; downscaled native assets share the same source.
white=rng.uniform(-1.0,1.0,size=(BASE[1],BASE[0])).astype(np.float32)
base_gray=np.float32(.5)+white*np.float32(.18)
base=np.empty((BASE[1],BASE[0],4),np.float32); base[...,:3]=base_gray[...,None]; base[...,3]=1
refs={}
for w,h in REF_SIZES:
 if (w,h)==BASE: ref=base.copy()
 else:
  gray=cv2.resize(base_gray,(w,h),interpolation=cv2.INTER_AREA)
  ref=np.empty((h,w,4),np.float32); ref[...,:3]=gray[...,None]; ref[...,3]=1
 refs[(w,h)]=ref
np.savez_compressed(OUT/'reference_assets_float32.npz',**{f'ref_{w}x{h}':a for (w,h),a in refs.items()})
# Native-size preview images make the input identity inspectable; they are quantized display evidence only.
native_previews=[]
for (w,h),ref in refs.items():
 im=np.uint8(np.clip(ref[...,:3][...,::-1],0,1)*255+.5)
 Image.fromarray(im).save(OUT/f'reference_native_{w}x{h}.png')
 native_previews.append((f'native reference {w}x{h}',Image.fromarray(im).resize((192,96),Image.Resampling.NEAREST)))
ref_sheet=Image.new('RGB',(3*192,130),(25,27,30)); ref_draw=ImageDraw.Draw(ref_sheet)
for i,(label,img) in enumerate(native_previews):
 x=i*192; ref_sheet.paste(img,(x,0)); ref_draw.text((x+3,100),label,fill=(240,240,240))
ref_sheet.save(OUT/'native-reference-contact.png')
rows=[]; tiles=[]
for (rw,rh),reference in refs.items():
 for tw,th in TARGET_SIZES:
  if (rw,rh)==(tw,th): prepared=reference.copy()
  else: prepared=cv2.resize(reference,(tw,th),interpolation=cv2.INTER_LINEAR)
  source=np.full((th,tw,4),SOURCE_VALUE,np.float32); source[...,3]=1
  source_luma=np.float32(.2126*source[...,2]+.7152*source[...,1]+.0722*source[...,0])
  response=np.maximum(np.float32(0),np.float32(1)+(np.float32(1)-np.clip(source_luma,0,1))*np.float32(.25)+np.clip(source_luma,0,1)*np.float32(-.2))
  for sigma in SIGMAS:
   prepared_rgb=prepared[...,:3]
   blur=cv2.GaussianBlur(prepared_rgb,(0,0),sigmaX=max(.1,float(sigma)))
   residual=(prepared_rgb-blur).astype(np.float32)
   # Same grayscale source/reference means luma residual equals each channel residual, chroma residual is zero.
   residual_luma=(residual[...,0]*np.float32(.0722)+residual[...,1]*np.float32(.7152)+residual[...,2]*np.float32(.2126)).astype(np.float32)
   grain=(residual_luma*np.float32(LUMA_AMOUNT)*AMOUNT*response).astype(np.float32)
   output=source.copy(); output[...,:3]+=grain[...,None]*MIX; output[...,3]=source[...,3]
   delta=output[...,:3]-source[...,:3]
   zero_mix=source.copy(); zero_mix[...,:3]+=grain[...,None]*np.float32(0)
   constant=np.full_like(prepared_rgb,np.float32(.5))
   constant_residual=constant-cv2.GaussianBlur(constant,(0,0),sigmaX=max(.1,float(sigma)))
   zero_mix_identity=bool(np.array_equal(zero_mix,source))
   constant_reference_identity=bool(np.array_equal(constant_residual,np.zeros_like(constant_residual)))
   if not (zero_mix_identity and constant_reference_identity): raise RuntimeError('identity invariant failed')
   acf=acf_lengths(grain)
   ref_luma=prepared[...,0].astype(np.float64); residual_var=float(np.var(grain,dtype=np.float64)); prepared_var=float(np.var(ref_luma,dtype=np.float64))
   center=(slice(th//8,7*th//8),slice(tw//8,7*tw//8))
   record={'reference_wh':[rw,rh],'target_wh':[tw,th],'reference_to_target_scale':[tw/rw,th/rh],'grainSize':sigma,
    'parameters':{'amount':1,'lumaAmount':1,'chromaAmount':.65,'shadowResponse':.25,'highlightResponse':-.2,'temporalJitter':0,'mix':1},
    'prepared_reference_stddev':float(np.std(ref_luma)), 'prepared_reference_minmax':[float(ref_luma.min()),float(ref_luma.max())],
    'residual_stddev_bgr':[float(np.std(residual[...,i])) for i in range(3)],'output_delta_stddev':float(np.std(delta[...,1])),
    'residual_variance_fraction_of_prepared_texture':residual_var/prepared_var if prepared_var else 0.0,
    'output_minmax_bgr':[float(output[...,i].min()) for i in range(3)],'output_channels_lt_0':int(np.count_nonzero(output[...,:3]<0)),
    'output_channels_gt_1':int(np.count_nonzero(output[...,:3]>1)), 'alpha_exact':bool(np.array_equal(output[...,3],source[...,3])),
    'nonfinite':int(np.count_nonzero(~np.isfinite(output))), 'zero_mix_identity':zero_mix_identity, 'constant_reference_identity':constant_reference_identity, 'autocorrelation_center_roi':acf,
    'normalized_1e_lengths':[None if acf['first_1e_x_px'] is None else acf['first_1e_x_px']/tw,None if acf['first_1e_y_px'] is None else acf['first_1e_y_px']/th]}
   rows.append(record)
   # fixed residual gain x3 on .5 gray; same display mapping across every tile
   view=np.clip(np.float32(.5)+grain*np.float32(3),0,1)
   view_rgb=np.repeat(view[...,None],3,axis=2)
   img=Image.fromarray(np.uint8(view_rgb*255+.5)).resize((192,96),Image.Resampling.NEAREST)
   tiles.append((f'ref {rw}x{rh} -> out {tw}x{th}, sigma {sigma:g}',img))
# 3 columns x 4 rows of equal-scale amplified residuals.
cols=3; tilew=192; tileh=96; labelh=34
sheet=Image.new('RGB',(cols*tilew,((len(tiles)+cols-1)//cols)*(tileh+labelh)),(25,27,30)); dr=ImageDraw.Draw(sheet)
for i,(label,img) in enumerate(tiles):
 x=(i%cols)*tilew; y=(i//cols)*(tileh+labelh); sheet.paste(img,(x,y)); dr.text((x+3,y+tileh+3),label,fill=(240,240,240))
sheet.save(OUT/'reference-scale-residual-contact.png')
# Compare residual fields derived from one native reference but two output sizes after normalized linear resizing.
normalized=[]
for (rw,rh),ref in refs.items():
 residuals={}
 for tw,th in TARGET_SIZES:
  prep=ref.copy() if (rw,rh)==(tw,th) else cv2.resize(ref,(tw,th),interpolation=cv2.INTER_LINEAR)
  residuals[(tw,th)]=cv2.GaussianBlur(prep[...,:3],(0,0),sigmaX=1.15) # placeholder overwritten below
  residuals[(tw,th)]=(prep[...,:3]-cv2.GaussianBlur(prep[...,:3],(0,0),sigmaX=1.15))[...,0]
 a=residuals[(512,256)]; b=cv2.resize(residuals[(1024,512)],(512,256),interpolation=cv2.INTER_AREA)
 normalized.append({'reference_wh':[rw,rh],'target_low_wh':[512,256],'target_high_wh':[1024,512],'downsampled_high_vs_low_corr':corr(a,b),'downsampled_high_vs_low_rmse':float(np.sqrt(np.mean((a-b)**2)))})
PLAN_HASH=sha(PLAN); SOURCE_HASH=sha(SOURCE); RUNNER_HASH=sha(Path(__file__)); ASSET_HASH=sha(OUT/'reference_assets_float32.npz')
metrics={'study':'Match Grain reference-native-resolution / output-resolution scale response r1','environment':{'python':platform.python_version(),'numpy':np.__version__,'opencv':cv2.__version__},
 'input_sha256':{'plan':PLAN_HASH,'source':SOURCE_HASH,'runner':RUNNER_HASH,'reference_assets':ASSET_HASH},'source_markers':markers,
 'conditions':{'reference_sizes_wh':[list(x) for x in REF_SIZES],'target_sizes_wh':[list(x) for x in TARGET_SIZES],'grainSize':list(SIGMAS),'reference_seed':SEED,'dimensions':{'base':[1024,512]},'temporalJitter':0},
 'results':rows,'zero_mix_identity_all':all(r['zero_mix_identity'] for r in rows),'constant_reference_identity_all':all(r['constant_reference_identity'] for r in rows),'normalized_resolution_comparisons':normalized,
 'limitations':['Python/OpenCV source-equation model only; no compiled C++ or runtime named-input sampling.','Only neutral grayscale broadband reference and opaque neutral target; color, alpha and transfer metadata are not represented.','Native reference variants are synthetic area-resized descendants of one 1024x512 texture.','Contact sheet residuals are amplified 3x; raw float metrics are authoritative.','No temporal, GPU, preview/final, display-quality or performance evidence.']}
(OUT/'metrics.json').write_text(json.dumps(metrics,ensure_ascii=False,indent=2)+'\n',encoding='utf-8',newline='\n')
checks={p.name:sha(p) for p in sorted(OUT.iterdir()) if p.is_file() and p.name!='checksums.json'}
(OUT/'checksums.json').write_text(json.dumps(checks,ensure_ascii=False,indent=2)+'\n',encoding='utf-8',newline='\n')
print(json.dumps({'cases':len(rows),'normalized_comparisons':len(normalized),'markers':markers,'outputs':len(checks)},ensure_ascii=False))
