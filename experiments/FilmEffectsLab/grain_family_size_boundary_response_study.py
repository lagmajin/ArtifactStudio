from __future__ import annotations
import hashlib,json,math,platform
from pathlib import Path
import numpy as np
from PIL import Image,ImageDraw
ROOT=Path(__file__).resolve().parent; REPO=ROOT.parent.parent
PLAN=ROOT/'plans/grain-family-size-boundary-response-r1.json'
GRUNGE=REPO/'Artifact/src/Effect/ArtifactCreativeEffects.cpp'
ADDNOISE=REPO/'Artifact/src/Effects/AddNoise/AddNoiseEffect.cpp'
OUT=ROOT/'captures/grain-family-size-boundary-response-r1'; OUT.mkdir(parents=True,exist_ok=True)
GT=GRUNGE.read_text(encoding='utf-8'); AT=ADDNOISE.read_text(encoding='utf-8')
markers={
 'grunge_rounds_size':'static_cast<int>(std::round(impl_->grainSize))' in GT,
 'grunge_frame_seed':'1664525ull' in GT and '1013904223ull' in GT,
 'grunge_spatial_hash':'73856093' in GT and '19349663' in GT and '0xffffu' in GT,
 'grunge_amount_and_channels':'* impl_->grainAmount' in GT and 'channel == 2 ? 1.05f : channel == 0 ? 0.92f' in GT,
 'addnoise_floors_coordinates':'std::floor(static_cast<float>(x) / size_)' in AT and 'std::floor(static_cast<float>(y) / size_)' in AT,
 'addnoise_hash_constants':'0x7feb352du' in AT and '0x846ca68bu' in AT,
 'addnoise_low24_and_amount':'0x00ffffffu' in AT and 'noiseAt(ux, uy, 1u)' in AT,
 'addnoise_monochrome_clamps':'p[0] + n, 0.0f, 1.0f' in AT and 'p[1] + n' in AT,
}
if not all(markers.values()): raise RuntimeError(markers)
W=H=28; SEED=1977; FRAME=0; AMOUNT=np.float32(.075)
SIZES=(1.0,1.49,1.5,1.51,2.0,2.49,2.5,2.51,3.0)
MASK=0xffffffff
def u32(v): return int(v)&MASK
def round_half_away(v): return int(math.floor(v+.5)) if v>=0 else int(math.ceil(v-.5))
def grunge_field(size,amount=AMOUNT):
 block=max(1,round_half_away(float(size)))
 frame_seed=u32(SEED*1664525+FRAME*1013904223)
 f=np.zeros((H,W,3),np.float32)
 for y in range(H):
  for x in range(W):
   # Coordinates are bounded so both signed source multiplications remain representable.
   bh=frame_seed ^ u32((x//block)*73856093) ^ u32((y//block)*19349663)
   g=np.float32((np.float32(bh&0xffff)/np.float32(32767.5)-np.float32(1))*np.float32(amount))
   f[y,x,0]=g*np.float32(.92); f[y,x,1]=g; f[y,x,2]=g*np.float32(1.05)
 return f,block,frame_seed
def add_hash(v):
 v=u32(v); v^=v>>16; v=u32(v*0x7feb352d); v^=v>>15; v=u32(v*0x846ca68b); v^=v>>16
 return u32(v)
def add_noise_field(size,amount=AMOUNT):
 f=np.zeros((H,W,3),np.float32)
 for y in range(H):
  for x in range(W):
   sx=int(math.floor(float(np.float32(x)/np.float32(size))))
   sy=int(math.floor(float(np.float32(y)/np.float32(size))))
   value=u32(u32(sx*73856093)^u32(sy*19349663)^u32(1*83492791)^u32(SEED))
   noise=np.float32((np.float32(add_hash(value)&0x00ffffff)/np.float32(16777215)*np.float32(2))-np.float32(1))
   n=np.float32(noise*np.float32(amount))
   f[y,x,:]=n
 return f,None,None
def corr(a,b=None):
 if b is None: b=a
 av=np.asarray(a,np.float64).ravel(); bv=np.asarray(b,np.float64).ravel(); av=av-av.mean(); bv=bv-bv.mean(); d=math.sqrt(float(np.mean(av*av)*np.mean(bv*bv)))
 return float(np.mean(av*bv)/d) if d else 0.0
def run_stats(f):
 g=f[...,1].astype(np.float64)
 x0,x1=g[:,:-1].ravel(),g[:,1:].ravel(); y0,y1=g[:-1,:].ravel(),g[1:,:].ravel()
 # Run lengths of exactly identical consecutive green residuals, separately by row/column.
 def runs(a):
  lengths=[]
  for seq in a:
   n=1
   for p,q in zip(seq,seq[1:]):
    if p==q:n+=1
    else:lengths.append(n);n=1
   lengths.append(n)
  return lengths
 xr=runs(g); yr=runs(g.T)
 return {'mean':float(g.mean()),'stddev':float(g.std()),'unique_levels':int(np.unique(g).size),
  'lag1_x_correlation':corr(x0,x1),'lag1_y_correlation':corr(y0,y1),
  'identical_neighbor_fraction_x':float(np.mean(x0==x1)),'identical_neighbor_fraction_y':float(np.mean(y0==y1)),
  'run_length_x_mean':float(np.mean(xr)),'run_length_x_max':int(max(xr)),
  'run_length_y_mean':float(np.mean(yr)),'run_length_y_max':int(max(yr)),
  'rgb_channel_stddev':[float(f[...,i].std()) for i in range(3)],
  'rgb_min':[float(f[...,i].min()) for i in range(3)],'rgb_max':[float(f[...,i].max()) for i in range(3)],
  'output_min':float(np.min(np.float32(.5)+f)),'output_max':float(np.max(np.float32(.5)+f)),
  'alpha_exact':True,'nonfinite':int(np.count_nonzero(~np.isfinite(f)))}

def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
results=[]; fields={}; contact=Image.new('RGB',(len(SIZES)*142,2*142+48),(28,30,34)); draw=ImageDraw.Draw(contact)
for i,size in enumerate(SIZES):
 for effect,fn in [('grunge',grunge_field),('addnoise',add_noise_field)]:
  field,block,key=fn(size); fields[(effect,size)]=field
  stats=run_stats(field)
  zero_field,_,_=fn(size,amount=np.float32(0))
  zero_amount_identity=bool(np.count_nonzero(zero_field)==0 and np.array_equal(np.full((H,W),np.float32(.5))+zero_field[...,1],np.full((H,W),np.float32(.5))))
  if not zero_amount_identity: raise RuntimeError(f'zero amount identity failed: {effect} size={size}')
  record={'effect':effect,'requested_size':size,'effective_grunge_block':block,'frame_seed':key,'amount':float(AMOUNT),'seed':SEED,'frame':FRAME,'stats':stats,'zero_amount_identity':zero_amount_identity}
  results.append(record)
  # common display: amplified residual around .5; raw values remain authoritative.
  view=np.clip(np.float32(.5)+field*np.float32(4),0,1)
  image=Image.fromarray(np.uint8(view[...,::-1]*255+.5),'RGB').resize((112,112),Image.Resampling.NEAREST)
  x=i*142; y=0 if effect=='grunge' else 142; contact.paste(image,(x,y)); draw.text((x+2,y+114),f'{effect} size={size:g}',fill=(245,245,245))
contact.save(OUT/'grain-size-boundary-contact.png')
# Paired similarity across half-integer boundaries and between algorithms at same nominal controls.
comparisons=[]
for effect in ('grunge','addnoise'):
 for low,high in ((1.49,1.5),(1.5,1.51),(2.49,2.5),(2.5,2.51)):
  a=fields[(effect,low)][...,1]; b=fields[(effect,high)][...,1]
  comparisons.append({'kind':'adjacent_size','effect':effect,'low':low,'high':high,'correlation':corr(a,b),'rmse':float(np.sqrt(np.mean((a-b)**2)))})
for size in SIZES:
 a=fields[('grunge',size)][...,1]; b=fields[('addnoise',size)][...,1]
 comparisons.append({'kind':'cross_effect_same_size','size':size,'correlation':corr(a,b),'rmse':float(np.sqrt(np.mean((a-b)**2)))})
plan_hash=sha(PLAN); grunge_hash=sha(GRUNGE); add_hash_src=sha(ADDNOISE); runner_hash=sha(Path(__file__))
metrics={'study':'Film Grunge / Add Noise grain size-boundary source-equation comparison','environment':{'python':platform.python_version(),'numpy':np.__version__},
 'input_sha256':{'plan':plan_hash,'grunge_source':grunge_hash,'addnoise_source':add_hash_src,'runner':runner_hash},'source_markers':markers,
 'conditions':{'size_values':list(SIZES),'amount':float(AMOUNT),'seed':SEED,'frame':FRAME,'dimensions_wh':[W,H],'source_rgb':[.5,.5,.5],'mode_addnoise':'monochrome'},
 'results':results,'comparisons':comparisons,
 'coordinate_safety':{'x_product_max':(W-1)*73856093,'y_product_max':(H-1)*19349663,'signed_int_safe':(W-1)*73856093<=2147483647 and (H-1)*19349663<=2147483647},
 'zero_amount_identity_all_sizes':all(r['zero_amount_identity'] for r in results),
 'limitations':['Source-equation fields only; no C++ or application output.','The 28x28 domain avoids signed overflow for Film Grunge coordinate products; results cannot be extrapolated to product dimensions.','The same numeric seed does not imply the algorithms generate paired random samples.','Add Noise is CPU monochrome only; GPU routes are not compared.','Small sample statistics are descriptive, not proof of a general distribution or perceptual grain quality.','Contact sheet residual is amplified 4x for visibility.']}
(OUT/'metrics.json').write_text(json.dumps(metrics,ensure_ascii=False,indent=2)+'\n',encoding='utf-8',newline='\n')
checks={p.name:sha(p) for p in sorted(OUT.iterdir()) if p.is_file() and p.name!='checksums.json'}
(OUT/'checksums.json').write_text(json.dumps(checks,ensure_ascii=False,indent=2)+'\n',encoding='utf-8',newline='\n')
print(json.dumps({'field_cases':len(results),'comparisons':len(comparisons),'source_markers':markers},ensure_ascii=False))
