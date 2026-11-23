from __future__ import annotations
import hashlib,json,platform,math
from pathlib import Path
import numpy as np
from PIL import Image,ImageDraw
ROOT=Path(__file__).resolve().parent; REPO=ROOT.parent.parent
PLAN=ROOT/'plans/physical-halation-frame-edge-response-r1.json'
SOURCE=REPO/'ArtifactCore/include/ImageProcessing/Halation.ixx'
WRAPPER=REPO/'Artifact/src/Effects/Glow/PhysicalHalationEffect.cpp'
WRAPPER_TEXT=WRAPPER.read_text(encoding='utf-8')
OUT=ROOT/'captures/physical-halation-frame-edge-response-r1'; OUT.mkdir(parents=True,exist_ok=True)
T=SOURCE.read_text(encoding='utf-8')
markers={
 'source_rec601':'source[i].x * 0.114f + source[i].y * 0.587f + source[i].z * 0.299f' in T,
 'soft_knee':'softContribution' in T,
 'wide_red_radius':'settings.spread * settings.redDiffusion' in T,
 'narrow_gb_radius':'settings.spread, 0.0f, 0.1f, 0.05f' in T,
 'edge_replicated_horizontal':'std::clamp(x + dx, 0, w - 1)' in T,
 'edge_replicated_vertical':'std::clamp(y + dy, 0, h - 1)' in T,
 'red_channel_mapping':'output[index].z = (sum.z / count) * rM;' in T,
 'alpha_restored':'buffer[i].w = source[i].w' in T,
 'wrapper_copies_source':'dst = src;' in WRAPPER_TEXT,
}
if not all(markers.values()): raise RuntimeError(markers)
W=H=256; INTENSITY=np.float32(.5); SPREADS=(15,50)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def box1d(position:int,radius:int,length:int)->np.ndarray:
 field=np.zeros(length,np.float32); field[position]=1
 result=np.zeros(length,np.float32); count=2*radius+1
 for dx in range(-radius,radius+1): result+=field[np.clip(np.arange(length)+dx,0,length-1)]
 result/=np.float32(count)
 return result
def fwhm(a):
 peak=float(np.max(a)); return 0 if peak<=0 else int(np.count_nonzero(a>=peak*.5))
def first_threshold_width(a,fraction):
 peak=float(np.max(a)); return 0 if peak<=0 else int(np.count_nonzero(a>=peak*fraction))
def bbox(mask):
 ys,xs=np.nonzero(mask)
 return None if len(xs)==0 else [int(xs.min()),int(ys.min()),int(xs.max()+1),int(ys.max()+1)]
def positions_for(spread):
 r=spread*2; ds=sorted(set([0,1,max(0,r-1),r,r+1,2*r+1,128]))
 positions={('center',128,128)}
 for d in ds:
  if d<128: positions.add((f'left_d{d}',d,128)); positions.add((f'right_d{d}',W-1-d,128))
  if d<128: positions.add((f'top_d{d}',128,d)); positions.add((f'bottom_d{d}',128,H-1-d))
 for name,x,y in [('corner_tl',0,0),('corner_tr',W-1,0),('corner_bl',0,H-1),('corner_br',W-1,H-1)]: positions.add((name,x,y))
 return sorted(positions,key=lambda p:(p[1],p[2],p[0]))

rows=[]; maps={}; selected=[]
for spread in SPREADS:
 gb_radius=spread; red_radius=spread*2
 for label,sx,sy in positions_for(spread):
  px=box1d(sx,red_radius,W); py=box1d(sy,red_radius,H); red_profile=np.outer(py,px).astype(np.float32)*np.float32(4.0)
  pxg=box1d(sx,gb_radius,W); pyg=box1d(sy,gb_radius,H); gb_profile=np.outer(pyg,pxg).astype(np.float32)
  # Highlights are {B*.1,G*.2,R}; diffuse assigns green .1, blue .05 and red 1.
  blue=gb_profile*np.float32(.4*.05); green=gb_profile*np.float32(.8*.10); red=red_profile
  fields={'blue':blue,'green':green,'red':red}
  source=np.zeros((H,W,4),np.float32); source[...,3]=1; source[sy,sx,:3]=4
  output=source.copy(); output[...,0]+=blue*INTENSITY; output[...,1]+=green*INTENSITY; output[...,2]+=red*INTENSITY; output[...,3]=source[...,3]
  band={}
  yy,xx=np.mgrid[0:H,0:W]
  edge_dist=np.minimum.reduce([xx,yy,W-1-xx,H-1-yy])
  for b in (1,5,15,30,50,100): band[str(b)]=float(red[edge_dist<b].sum()/red.sum()) if red.sum() else 0
  rbbox=bbox(red>1e-6); gbbbox=bbox((green>1e-6)|(blue>1e-6))
  # The separable model yields source position as the first moment when boundary replication conserves energy.
  def channel_stats(field):
   total=float(field.sum()); mass=field.astype(np.float64)
   cx=float((mass*xx).sum()/total) if total else 0; cy=float((mass*yy).sum()/total) if total else 0
   xprof=field[sy,:]; yprof=field[:,sx]
   return {'sum':total,'peak':float(field.max()),'peak_to_sum':float(field.max()/total) if total else 0,
           'centroid_xy':[cx,cy],'centroid_shift_xy':[cx-sx,cy-sy],
           'support_gt_1e_6_pixels':int(np.count_nonzero(field>1e-6)),'bbox_xyxy':bbox(field>1e-6),
           'profile_x_fwhm':fwhm(xprof),'profile_y_fwhm':fwhm(yprof),
           'profile_x_1e':first_threshold_width(xprof,1/math.e),'profile_y_1e':first_threshold_width(yprof,1/math.e),
           'profile_x': [float(xprof[sx+off]) if 0<=sx+off<W else None for off in (-100,-50,-30,-15,-5,-1,0,1,5,15,30,50,100)],
           'profile_y': [float(yprof[sy+off]) if 0<=sy+off<H else None for off in (-100,-50,-30,-15,-5,-1,0,1,5,15,30,50,100)]}
  stats={k:channel_stats(v) for k,v in fields.items()}
  rows.append({'spread':spread,'red_radius':red_radius,'gb_radius':gb_radius,'position':label,'source_xy':[sx,sy],
   'distance_to_edges':[sx,sy,W-1-sx,H-1-sy],'layers':stats,'red_border_energy_fraction_by_band':band,
   'output_alpha_exact':bool(np.array_equal(output[...,3],source[...,3])),'output_nonfinite':int(np.count_nonzero(~np.isfinite(output))),
   'output_bgr_min':[float(output[...,i].min()) for i in range(3)],'output_bgr_max':[float(output[...,i].max()) for i in range(3)]})
  maps[(spread,label)]=output
  if label in ('center','left_d0','left_d1',f'left_d{red_radius-1}',f'left_d{red_radius}',f'left_d{red_radius+1}','corner_tl','top_d0'):
   selected.append((spread,label,sx,sy,output))
# Common amplified RGB halo mapping; source pixel and broad tails remain inspectable.
for spread in SPREADS:
 subset=[x for x in selected if x[0]==spread]
 tile=96; labelh=28; cols=4; sheet=Image.new('RGB',(cols*tile,((len(subset)+cols-1)//cols)*(tile+labelh)),(25,27,30)); dr=ImageDraw.Draw(sheet)
 for i,(_,label,sx,sy,out) in enumerate(subset):
  delta=np.stack((out[...,2],out[...,1],out[...,0]),axis=-1)
  view=np.uint8(np.clip(delta*256,0,1)*255+.5)
  im=Image.fromarray(view).resize((tile,tile),Image.Resampling.NEAREST)
  x=(i%cols)*tile; y=(i//cols)*(tile+labelh); sheet.paste(im,(x,y)); dr.text((x+2,y+tile+3),label,fill=(240,240,240))
 sheet.save(OUT/f'frame-edge-contact-spread-{spread}.png')
# Opposite edges should mirror in this clamped, axis-symmetric equation.
symmetry=[]
for spread in SPREADS:
 pairs=[('left_d0','right_d0'),('top_d0','bottom_d0'),('corner_tl','corner_tr'),('corner_tl','corner_bl'),('corner_tl','corner_br')]
 for a,b in pairs:
  if (spread,a) not in maps or (spread,b) not in maps: continue
  aa=maps[(spread,a)][...,:3]; bb=maps[(spread,b)][...,:3]
  if b in ('right_d0','corner_tr','corner_br'): bb=bb[:,::-1,:]
  if b in ('bottom_d0','corner_bl','corner_br'): bb=bb[::-1,:,:]
  symmetry.append({'spread':spread,'a':a,'b':b,'mirrored_rgb_rmse':float(np.sqrt(np.mean((aa-bb)**2)))})
PLAN_HASH=sha(PLAN); SOURCE_HASH=sha(SOURCE); WRAPPER_HASH=sha(WRAPPER); RUNNER_HASH=sha(Path(__file__))
metrics={'study':'Physical Halation edge-replicated frame-boundary response r1','environment':{'python':platform.python_version(),'numpy':np.__version__},
 'input_sha256':{'plan':PLAN_HASH,'core_source':SOURCE_HASH,'wrapper_source':WRAPPER_HASH,'runner':RUNNER_HASH},'source_markers':markers,
 'conditions':{'dimensions_wh':[W,H],'spreads':list(SPREADS),'source_rgb':[4,4,4],'source_alpha':1,'intensity':float(INTENSITY),'positions_total':len(rows)},
 'results':rows,'opposite_edge_symmetry':symmetry,
 'limitations':['Python Float32 separable impulse-response model; no compiled C++ or app output.','Opaque alpha and neutral point source only; no transparent frame edges, color cast, transfer function or real photographic inputs.','The 1/e profile and energy metrics characterize the box equation, not optical film scattering.','No temporal, display acceptance, GPU, performance or allocation evidence.']}
(OUT/'metrics.json').write_text(json.dumps(metrics,ensure_ascii=False,indent=2)+'\n',encoding='utf-8',newline='\n')
checks={p.name:sha(p) for p in sorted(OUT.iterdir()) if p.is_file() and p.name!='checksums.json'}
(OUT/'checksums.json').write_text(json.dumps(checks,ensure_ascii=False,indent=2)+'\n',encoding='utf-8',newline='\n')
print(json.dumps({'positions':len(rows),'markers':markers,'symmetry_cases':len(symmetry),'outputs':len(checks)},ensure_ascii=False))
