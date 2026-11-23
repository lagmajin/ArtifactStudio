from __future__ import annotations
import hashlib, json
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parent
PLAN = ROOT / 'plans/physical-halation-hdr-spread-intensity-response-r1.json'
OUT = ROOT / 'captures/physical-halation-hdr-spread-intensity-response-r1'
OUT.mkdir(parents=True, exist_ok=True)
WIDTH = HEIGHT = 256
THRESHOLD, SOFTNESS, RED_DIFFUSION = np.float32(.8), np.float32(.5), np.float32(2.0)
SPREADS = (0, 4, 15, 50)
INTENSITIES = (.5, 1.5, 5.0)
CONDITIONS = [(s, i) for s in (0, 4, 50) for i in (.5, 1.5)]
# Spread 15 carries the zero baseline and high-intensity stress probe.
CONDITIONS += [(15, i) for i in (0.0, .5, 1.5, 5.0)]

SRC = Path('ArtifactCore/include/ImageProcessing/Halation.ixx')
WRAPPER = Path('Artifact/src/Effects/Glow/PhysicalHalationEffect.cpp')
src_text = SRC.read_text(encoding='utf-8')
wrapper_text = WRAPPER.read_text(encoding='utf-8')
markers = {
 'rec601_luma': 'source[i].x * 0.114f + source[i].y * 0.587f + source[i].z * 0.299f' in src_text,
 'soft_knee': 'softContribution' in src_text,
 'red_wide_and_gb_narrow': 'settings.spread * settings.redDiffusion' in src_text and 'settings.spread, 0.0f, 0.1f, 0.05f' in src_text,
 'additive_composite': 'source[i].x + highlights[i].x * settings.intensity' in src_text,
 'alpha_restore': 'buffer[i].w = source[i].w' in src_text,
 'wrapper_clamps_threshold': 'threshold' in wrapper_text and '0.0f' in wrapper_text and '1.0f' in wrapper_text,
}
if not all(markers.values()):
 raise RuntimeError(f'Source marker mismatch: {markers}')

def digest(path: Path) -> str:
 return hashlib.sha256(path.read_bytes()).hexdigest()

def diffuse_box(field: np.ndarray, radius: int) -> np.ndarray:
 # Edge-replicated separable box. Float64 accumulation limits model-only summation drift.
 r = max(0, int(radius)); h, w = field.shape[:2]
 xp = np.pad(field.astype(np.float64), ((0,0),(r,r),(0,0)), mode='edge')
 cs = np.concatenate((np.zeros((h,1,field.shape[2]),np.float64), np.cumsum(xp,axis=1)),axis=1)
 horizontal = (cs[:,2*r+1:2*r+1+w]-cs[:,:w])/(2*r+1)
 yp = np.pad(horizontal, ((r,r),(0,0),(0,0)), mode='edge')
 cs = np.concatenate((np.zeros((1,w,field.shape[2]),np.float64), np.cumsum(yp,axis=0)),axis=0)
 result = (cs[2*r+1:2*r+1+h,:,:]-cs[:h,:,:])/(2*r+1)
 return result.astype(np.float32)

def process(source: np.ndarray, spread: int, intensity: float):
 b,g,r,a=(source[...,i] for i in range(4))
 lum=(b*np.float32(.114)+g*np.float32(.587)+r*np.float32(.299)).astype(np.float32)
 knee=SOFTNESS
 soft=np.clip(lum-THRESHOLD+knee,0,2*knee).astype(np.float32)
 soft_contrib=(soft*soft/np.float32(4*knee+1e-6)).astype(np.float32)
 contrib=np.maximum(np.maximum(lum-THRESHOLD,0),soft_contrib).astype(np.float32)
 weight=np.clip(contrib/np.float32(1-THRESHOLD+.001),0,1).astype(np.float32)
 hi=np.zeros((*lum.shape,3),np.float32)
 hi[...,0]=b*weight*np.float32(.1); hi[...,1]=g*weight*np.float32(.2); hi[...,2]=r*weight
 wide=diffuse_box(hi,int(spread*float(RED_DIFFUSION)))
 narrow=diffuse_box(hi,int(spread))
 out=source.copy()
 out[...,0]+=narrow[...,0]*np.float32(.05*intensity)
 out[...,1]+=narrow[...,1]*np.float32(.10*intensity)
 out[...,2]+=wide[...,2]*np.float32(intensity)
 out[...,3]=a
 return out, {'highlight':hi,'red':wide[...,2], 'green':narrow[...,1]*np.float32(.10), 'blue':narrow[...,0]*np.float32(.05)}

def make_uniform(level: float) -> np.ndarray:
 x=np.zeros((HEIGHT,WIDTH,4),np.float32); x[...,:3]=np.float32(level); x[...,3]=1
 return x

def make_point(level: float) -> np.ndarray:
 x=np.zeros((HEIGHT,WIDTH,4),np.float32); x[128,128,:3]=np.float32(level); x[...,3]=1
 return x

def profile_fwhm(v: np.ndarray) -> int:
 peak=float(v.max())
 if peak<=0: return 0
 return int(np.count_nonzero(v >= peak*.5))

def stats(image: np.ndarray, source: np.ndarray, layers: dict, assoc=(0,0)) -> dict:
 rgb=image[...,:3].astype(np.float64)
 delta=rgb-source[...,:3].astype(np.float64)
 out_luma=rgb[...,0]*.114+rgb[...,1]*.587+rgb[...,2]*.299
 src_luma=source[...,0]*.114+source[...,1]*.587+source[...,2]*.299
 return {
  'rgb_min_bgr':[float(rgb[...,i].min()) for i in range(3)],'rgb_max_bgr':[float(rgb[...,i].max()) for i in range(3)],
  'input_channels_gt_1_fraction':float(np.mean(source[...,:3]>1)),
  'channels_gt_1_fraction':float(np.mean(rgb>1)),
  'newly_clipped_channels_fraction':float(np.mean((source[...,:3]<=1)&(rgb>1))),
  'channels_lt_0_fraction':float(np.mean(rgb<0)),
  'nonfinite_rgb_channels':int(rgb.size-np.isfinite(rgb).sum()),
  'alpha_exact':bool(np.array_equal(image[...,3],source[...,3])),
  'delta_rgb_rmse':float(np.sqrt(np.mean(delta*delta))),'delta_luma_mean':float((out_luma-src_luma).mean()),
  'delta_luma_max':float((out_luma-src_luma).max()),
  'delta_channel_means_bgr':[float(delta[...,i].mean()) for i in range(3)],
  'highlight_support_pixels':int(np.count_nonzero(np.any(layers['highlight']>0,axis=-1))),
  'diffusion_support_pixels':int(np.count_nonzero(np.any(delta>1e-6,axis=-1))),
  'diffusion_support_gt_1e_4_pixels':int(np.count_nonzero(np.any(delta>1e-4,axis=-1))),
 }

rows=[]; point_previews=[]; diagnostic_previews=[]; support_previews=[]; patch_curves=[]
levels=(.25,.6,.8,.9,1.0,1.5,4.0)
# Test uniform plates across the full declared setting matrix to isolate threshold/highlight response.
for level in levels:
 source=make_uniform(level)
 for spread,intensity in CONDITIONS:
  output,layers=process(source,spread,intensity)
  record=stats(output,source,layers)
  patch_curves.append({'level':level,'spread':spread,'intensity':intensity,**record})
# Point-source response maps expose the box profile and normalized footprint.
for spread,intensity in CONDITIONS:
 source=make_point(4.0)
 output,layers=process(source,spread,intensity)
 record=stats(output,source,layers)
 center=128
 profiles={}
 for name,plane in [('red',layers['red']),('green',layers['green']),('blue',layers['blue'])]:
  row=plane[center,:].astype(np.float64); col=plane[:,center].astype(np.float64)
  profiles[name]={'horizontal_fwhm_px':profile_fwhm(row),'vertical_fwhm_px':profile_fwhm(col),
                  'horizontal_peak':float(row.max()),'vertical_peak':float(col.max()),
                  'horizontal_samples':[float(row[center+k]) for k in (0,1,2,4,8,16,32,64,100)]}
 tag=f'spread-{spread:02d}_intensity-{intensity:g}'.replace('.','p')
 rgb=np.clip(output[...,:3][...,::-1],0,1)
 img=Image.fromarray(np.rint(rgb*255).astype(np.uint8),'RGB')
 # Darken only for clear display of broad low-amplitude spill; raw Float32 metrics stay authoritative.
 img.save(OUT/f'point_{tag}.png')
 point_previews.append((tag,img))
 delta=np.maximum(output[...,:3]-source[...,:3],0)[...,::-1]
 diagnostic=Image.fromarray(np.rint(np.clip(delta*64.0,0,1)*255).astype(np.uint8),'RGB')
 diagnostic_previews.append((tag,diagnostic))
 support=np.stack((layers['red']>1e-6,layers['green']>1e-6,layers['blue']>1e-6),axis=-1)
 support_img=Image.fromarray((support.astype(np.uint8)*255),'RGB')
 support_previews.append((tag,support_img))
 np.savez_compressed(OUT/f'point_{tag}.npz',source=source,output=output,
                     red=layers['red'],green=layers['green'],blue=layers['blue'])
 rows.append({'spread':spread,'intensity':intensity,'point_source_level':4.0,'output':record,'profiles':profiles})
# Direct scalar curves from the uniform plates; no spatial effects are interpreted as app behavior.
for level in levels:
  variants=[p for p in patch_curves if p['level']==level]
  # Save all values for audit, while also recording response only where it is above threshold.
  for item in variants:
   item['source_luma']=float(level)
# Contact sheet, fixed clipping is solely for display.
cols=4; tw=256; th=256; lh=22
sheet=Image.new('RGB',(cols*tw,((len(point_previews)+cols-1)//cols)*(th+lh)),(25,27,30)); draw=ImageDraw.Draw(sheet)
for n,(label,img) in enumerate(point_previews):
 x=(n%cols)*tw; y=(n//cols)*(th+lh); sheet.paste(img,(x,y)); draw.text((x+4,y+th+3),label,fill=(240,240,240))
sheet.save(OUT/'point_contact_sheet.png')
# Diagnostic-only sheet: display 64x RGB gain on effect delta so low-amplitude tails can be inspected.
delta_sheet=Image.new('RGB',(cols*tw,((len(diagnostic_previews)+cols-1)//cols)*(th+lh)),(25,27,30)); draw=ImageDraw.Draw(delta_sheet)
for n,(label,img) in enumerate(diagnostic_previews):
 x=(n%cols)*tw; y=(n//cols)*(th+lh); delta_sheet.paste(img,(x,y)); draw.text((x+4,y+th+3),label+' delta x64',fill=(240,240,240))
delta_sheet.save(OUT/'point_delta_x64_contact_sheet.png')
support_sheet=Image.new('RGB',(cols*tw,((len(support_previews)+cols-1)//cols)*(th+lh)),(25,27,30)); draw=ImageDraw.Draw(support_sheet)
for n,(label,img) in enumerate(support_previews):
 x=(n%cols)*tw; y=(n//cols)*(th+lh); support_sheet.paste(img,(x,y)); draw.text((x+4,y+th+3),label+' support >1e-6',fill=(240,240,240))
support_sheet.save(OUT/'point_support_contact_sheet.png')

plan_hash=digest(PLAN); source_hash=digest(SRC); wrapper_hash=digest(WRAPPER); runner_hash=digest(Path(__file__))
metrics={'study':'Physical Halation HDR / spread / intensity source-equation response r1',
 'environment':{'python_numpy':np.__version__,'dimensions_wh':[WIDTH,HEIGHT],'date':'2026-11-12'},
 'input_sha256':{'plan':plan_hash,'core_source':source_hash,'wrapper_source':wrapper_hash,'runner':runner_hash},
 'source_markers':markers,'conditions':{'spread':list(SPREADS),'intensity':[0,.5,1.5,5],'threshold':float(THRESHOLD),'softness':float(SOFTNESS),'redDiffusion':float(RED_DIFFUSION),'unique_settings':len(CONDITIONS)},
 'uniform_patch_curves':patch_curves,'point_source_cases':rows,
 'limitations':['Offline equation model only; no compiled C++ / app / GPU execution.','Fixture values are direct numeric linear-like Float32, not a validated transfer function.','Opaque alpha only; alpha association and transparent edges are outside this study.','Preview PNGs clip to 0..1; un-clamped Float32 NPZ and metrics are authoritative.','No aesthetic acceptance, temporal, performance, or allocation evidence.']}
metric_path=OUT/'metrics.json'; metric_path.write_text(json.dumps(metrics,ensure_ascii=False,indent=2)+'\n',encoding='utf-8',newline='\n')
files=[p for p in OUT.iterdir() if p.is_file() and p.name!='checksums.json']
checks={str(p.relative_to(Path.cwd())).replace('\\','/'):digest(p) for p in sorted(files)}
(OUT/'checksums.json').write_text(json.dumps(checks,ensure_ascii=False,indent=2)+'\n',encoding='utf-8',newline='\n')
print(json.dumps({'conditions':len(CONDITIONS),'patch_measurements':len(patch_curves),'point_cases':len(rows),'markers':markers,'output_dir':str(OUT)},ensure_ascii=False))
