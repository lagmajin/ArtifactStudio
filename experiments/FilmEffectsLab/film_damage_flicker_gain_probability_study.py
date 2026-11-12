from __future__ import annotations
import hashlib, json, math
from pathlib import Path
from PIL import Image, ImageDraw

ROOT=Path(__file__).resolve().parent
PLAN=ROOT/'plans/film-damage-flicker-gain-probability-r1.json'
SOURCE=ROOT.parent.parent/'Artifact/src/Effects/Rasterizer/FilmDamageEffect.cpp'
OUT=ROOT/'captures/film-damage-flicker-gain-probability-r1'
OUT.mkdir(parents=True,exist_ok=True)
TONES=(0,.01,.02,.05,.18,.5,.9,.98,1.0)
CONTROLS=(0,.08,.25,.5,1.0)
text=SOURCE.read_text(encoding='utf-8')
markers={
 'normal_gain_formula':'const float flickerGain = 1.0f + normal(rng) * flicker;' in text,
 'rgb_multiplied_by_gain':'float value = sourceRow[x][c] * flickerGain + grainNoise;' in text,
 'damage_overlay_follows_gain':'value = value * (1.0f - damageAlpha) + damageRow[x][c] * damageAlpha;' in text,
 'burn_added_after_gain':'if (c == 2) value += burn * 1.35f;' in text,
 'alpha_restored':'outputRow[x][3] = sourceRow[x][3];' in text,
 'flicker_control_clamped':'flicker_ = std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 0.08f;' in text,
}
if not all(markers.values()): raise RuntimeError(markers)

def phi(z:float)->float:
 return .5*math.erfc(-z/math.sqrt(2.0))
def q(z:float)->float:
 return .5*math.erfc(z/math.sqrt(2.0))
def digest(p:Path)->str: return hashlib.sha256(p.read_bytes()).hexdigest()

rows=[]
for f in CONTROLS:
 negative=q(1.0/f) if f>0 else 0.0
 row={'flicker':f,'gain_mean':1.0,'gain_variance':f*f,'negative_gain_probability':negative,
      'expected_negative_samples_in_24':negative*24,'expected_negative_samples_in_120':negative*120,'tones':[]}
 for tone in TONES:
  below=negative if tone>0 else 0.0
  threshold=(1.0/tone-1.0)/f if f>0 and tone>0 else math.inf
  above=q(threshold) if math.isfinite(threshold) else 0.0
  # A neutral RGB pixel has identical per-channel events; therefore P(any channel crosses) equals the single-channel probability.
  row['tones'].append({'source_tone':tone,'p_channel_output_lt_0':below,
                       'p_channel_output_gt_1':above,'p_neutral_rgb_any_channel_lt_0':below,
                       'p_neutral_rgb_any_channel_gt_1':above,
                       'z_threshold_gt_1':None if not math.isfinite(threshold) else threshold})
 rows.append(row)

# Render a clearly labeled above-one event probability matrix. Log color is visual-only; JSON retains exact probabilities.
w=1100; left=190; top=96; cellw=98; cellh=76
im=Image.new('RGB',(left+len(TONES)*cellw+20,top+len(CONTROLS)*cellh+80),(248,248,248)); d=ImageDraw.Draw(im)
d.text((left,22),'Film Damage Flicker: P(source tone × gain > 1)',fill=(0,0,0))
for j,t in enumerate(TONES): d.text((left+j*cellw+8,68),f'tone {t:g}',fill=(0,0,0))
for i,row in enumerate(rows):
 y=top+i*cellh; f=row['flicker']; pneg=row['negative_gain_probability']
 d.text((12,y+7),f'flicker {f:g}',fill=(0,0,0)); d.text((12,y+29),f'P(gain<0) {pneg:.3g}',fill=(0,0,0))
 for j,entry in enumerate(row['tones']):
  p=entry['p_channel_output_gt_1']; intensity=0 if p<=0 else max(0,min(1,(math.log10(p)+36)/36))
  red=int(255*intensity); blue=int(255*(1-intensity)); color=(red,80,blue)
  x=left+j*cellw; d.rectangle((x,y,x+cellw-2,y+cellh-2),fill=color)
  label=f'{p:.2%}' if p>=.0001 else (f'{p:.1e}' if p>0 else '0')
  d.text((x+7,y+14),label,fill=(255,255,255) if intensity<.35 else (0,0,0))
d.text((left,top+len(CONTROLS)*cellh+15),'Log-colored cells for display only; values are analytic Gaussian probabilities, not measured frame counts.',fill=(0,0,0))
im.save(OUT/'above_one_probability_matrix.png')

# Negative gain probability curve over supported flicker values (log axis on graphic).
chart=Image.new('RGB',(920,430),(255,255,255)); d=ImageDraw.Draw(chart)
d.text((20,16),'Film Damage Flicker: theoretical P(gain < 0)',fill=(0,0,0))
x0,y0,x1,y1=88,60,875,350
d.rectangle((x0,y0,x1,y1),outline=(40,40,40))
lo,hi=-40,0
for tick in (-40,-30,-20,-10,0):
 yy=y1-(tick-lo)/(hi-lo)*(y1-y0); d.line((x0,yy,x1,yy),fill=(220,220,220)); d.text((24,yy-7),f'10^{tick}',fill=(0,0,0))
for row in rows:
 f=row['flicker']; p=row['negative_gain_probability']
 if f==0 or p==0: xx=x0; yy=y1
 else:
  xx=x0+(f/1.0)*(x1-x0); yy=y1-(math.log10(p)-lo)/(hi-lo)*(y1-y0)
 d.ellipse((xx-5,yy-5,xx+5,yy+5),fill=(196,45,35)); d.text((xx+8,yy-12),f'{f:g}: {p:.3g}',fill=(0,0,0))
d.text((x0,y1+24),'flicker control (0–1); vertical axis is logarithmic probability.',fill=(0,0,0))
chart.save(OUT/'negative_gain_probability_curve.png')

plan_hash=digest(PLAN); src_hash=digest(SOURCE); script_hash=digest(Path(__file__))
metrics={'study':'Film Damage Flicker analytic gain probability r1','environment':{'python':'stdlib math.erfc','date':'2026-11-12'},
 'input_sha256':{'plan':plan_hash,'source':src_hash,'runner':script_hash},'source_markers':markers,
 'distribution_model':'Z ~ N(0,1), G=1+fZ. P(G<0)=Q(1/f). For v>0, P(vG>1)=Q((1/v-1)/f). No sampling.',
 'conditions':{'flicker':list(CONTROLS),'tones':list(TONES),'strict_boundaries':True},'results':rows,
 'limitations':['Marginal Gaussian probabilities only; exact MSVC/STL normal_distribution sequence is not simulated.','No temporal correlation, sequence run-length, seek/reverse or perceptual acceptance evidence.','Neutral grayscale only; no colored-material or premultiplied alpha behavior.','No downstream clamp or display transform; >1 and <0 are raw equation events.','No app, renderer, performance, or allocation execution.']}
(OUT/'metrics.json').write_text(json.dumps(metrics,ensure_ascii=False,indent=2)+'\n',encoding='utf-8',newline='\n')
files=[p for p in OUT.iterdir() if p.is_file() and p.name!='checksums.json']
checks={p.name:digest(p) for p in sorted(files)}
(OUT/'checksums.json').write_text(json.dumps(checks,ensure_ascii=False,indent=2)+'\n',encoding='utf-8',newline='\n')
print(json.dumps({'flicker_controls':len(rows),'tone_conditions':sum(len(r['tones']) for r in rows),'markers':markers,'negative_probabilities':[(r['flicker'],r['negative_gain_probability']) for r in rows]}))
