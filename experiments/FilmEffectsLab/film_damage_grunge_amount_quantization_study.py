from __future__ import annotations
import hashlib,json,platform
from pathlib import Path
import numpy as np
from PIL import Image,ImageDraw
ROOT=Path(__file__).resolve().parent
REPO=ROOT.parent.parent
PLAN=ROOT/'plans/film-damage-grunge-amount-quantization-r1.json'
GRUNGE=REPO/'Artifact/src/Effect/ArtifactCreativeEffects.cpp'
DAMAGE=REPO/'Artifact/src/Effects/Rasterizer/FilmDamageEffect.cpp'
OUT=ROOT/'captures/film-damage-grunge-amount-quantization-r1'
OUT.mkdir(parents=True,exist_ok=True)
GT=GRUNGE.read_text(encoding='utf-8'); DT=DAMAGE.read_text(encoding='utf-8')
markers={
 'grunge_scratch_scale_integer_width_division':'width / 18' in GT and 'scratchAmount *' in GT,
 'grunge_dust_scale_integer_area_division':'width * height / 900' in GT and 'dustAmount *' in GT,
 'damage_scratch_count_times_18':'static_cast<int>(scratches * 18.0f)' in DT,
 'damage_dust_area_div_180_float':'dust * width * height / 180.0f' in DT,
 'grunge_amounts_clamped_0_1':'std::clamp(number, 0.0f, 1.0f)' in GT,
 'damage_amounts_clamped_0_1':'std::clamp(v, 0.0f, 1.0f)' in DT,
}
if not all(markers.values()): raise RuntimeError(markers)
SIZES=((512,256),(1024,512),(1920,1080),(3840,2160))
CONTROLS=[np.float32(i/100.0) for i in range(101)]
DEFAULTS={'grunge_scratch':.22,'grunge_dust':.18,'damage_scratch':.10,'damage_dust':.08}
def trunc(x): return int(float(x))
def count(component,f,w,h):
 f=np.float32(f)
 if component=='grunge_scratch':
  scale=max(1,w//18); return trunc(np.float32(f*np.float32(scale)))
 if component=='grunge_dust':
  scale=max(1,(w*h)//900); return trunc(np.float32(f*np.float32(scale)))
 if component=='damage_scratch': return trunc(np.float32(f*np.float32(18.0)))
 if component=='damage_dust':
  x=np.float32(f*np.float32(w)); x=np.float32(x*np.float32(h)); x=np.float32(x/np.float32(180.0)); return trunc(x)
 raise ValueError(component)
def count_scale(comp,w,h):
 if comp=='grunge_scratch':return max(1,w//18)
 if comp=='grunge_dust':return max(1,(w*h)//900)
 if comp=='damage_scratch':return 18
 return (w*h)/180.0

rows=[]
for w,h in SIZES:
 for comp in DEFAULTS:
  vals=[count(comp,f,w,h) for f in CONTROLS]
  deltas=[b-a for a,b in zip(vals,vals[1:])]
  default=DEFAULTS[comp]; default_count=count(comp,np.float32(default),w,h)
  scale=count_scale(comp,w,h)
  rows.append({'width':w,'height':h,'component':comp,'default_control':default,
   'count_scale':scale,'default_count':default_count,'count_at_0':vals[0],'count_at_1':vals[-1],
   'default_count_per_megapixel':(default_count/(w*h)*1e6 if 'dust' in comp else None),
   'default_scratch_per_1000px_width':(default_count/w*1000 if 'scratch' in comp else None),
   'ideal_control_interval_per_mark':(1.0/scale if scale else None),
   'slider_controls': [float(v) for v in CONTROLS], 'counts':vals,
   'zero_change_intervals':sum(d==0 for d in deltas),'zero_change_fraction':sum(d==0 for d in deltas)/len(deltas),
   'delta_per_0p01':{'min':min(deltas),'median':float(np.median(deltas)),'max':max(deltas)},
   'observed_transition_count':sum(d>0 for d in deltas),'monotonic':all(a<=b for a,b in zip(vals,vals[1:]))})
# Charts show integer mark counts only. No spatial field or rendered pixel response is implied.
colors={'grunge':(35,105,190),'damage':(210,75,55)}
for w,h in SIZES:
 im=Image.new('RGB',(1120,680),(250,250,250)); dr=ImageDraw.Draw(im)
 dr.text((30,15),f'Film Damage / Film Grunge mark-count response — {w}x{h}',fill=(0,0,0))
 for panel,kind in enumerate(('scratch','dust')):
  x0=85+panel*540; y0=80; x1=x0+475; y1=560
  dr.text((x0,y0-28),kind.title()+' count',fill=(0,0,0)); dr.rectangle((x0,y0,x1,y1),outline=(50,50,50))
  subset=[r for r in rows if r['width']==w and kind in r['component']]
  ymax=max(max(r['counts']) for r in subset) or 1
  for t in range(5):
   y=y1-t*(y1-y0)/4; dr.line((x0,y,x1,y),fill=(225,225,225)); dr.text((x0-45,y-7),str(round(ymax*t/4)),fill=(0,0,0))
  for r in subset:
   which='grunge' if r['component'].startswith('grunge') else 'damage'; pts=[]
   for i,c in enumerate(r['counts']): pts.append((x0+i*(x1-x0)/100,y1-c/ymax*(y1-y0)))
   dr.line(pts,fill=colors[which],width=3)
  dr.text((x0,y1+12),'Control 0.00 → 1.00',fill=(0,0,0))
  dr.text((x0+4,y1+35),'Grunge',fill=colors['grunge']); dr.text((x0+100,y1+35),'Damage',fill=colors['damage'])
 dr.text((40,640),'Formula-only counts; mark placement, opacity, footprint, and perceived strength are not shown.',fill=(30,30,30))
 im.save(OUT/f'count-response-{w}x{h}.png')
plan_hash=hashlib.sha256(PLAN.read_bytes()).hexdigest(); grunge_hash=hashlib.sha256(GRUNGE.read_bytes()).hexdigest(); damage_hash=hashlib.sha256(DAMAGE.read_bytes()).hexdigest(); runner_hash=hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
metrics={'study':'Film Damage / Film Grunge dust and scratch control quantization r1','environment':{'python':platform.python_version(),'numpy':np.__version__},'input_sha256':{'plan':plan_hash,'grunge_source':grunge_hash,'damage_source':damage_hash,'runner':runner_hash},'source_markers':markers,'dimensions_wh':[list(s) for s in SIZES],'control_grid':[float(x) for x in CONTROLS],'results':rows,'limitations':['No random numbers, positions, marks, rasterization or effect output are simulated.','Counts do not equate visual weight: size, opacity, shape and source overlay rules differ.','The 0.01 grid is descriptive of slider changes at that step, not of actual widget precision or slider snapping.','No C++/app, alpha, temporal, display, performance or allocation evidence.']}
(OUT/'metrics.json').write_text(json.dumps(metrics,ensure_ascii=False,indent=2)+'\n',encoding='utf-8',newline='\n')
checks={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(OUT.iterdir()) if p.is_file() and p.name!='checksums.json'}
(OUT/'checksums.json').write_text(json.dumps(checks,ensure_ascii=False,indent=2)+'\n',encoding='utf-8',newline='\n')
print(json.dumps({'conditions':len(rows),'markers':markers,'outputs':len(checks)},ensure_ascii=False))
