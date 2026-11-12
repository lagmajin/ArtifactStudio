"""Track exact black/white RGB channel endpoints through the current photo suite."""
from __future__ import annotations
import hashlib, io, json, platform, sys
from pathlib import Path
import numpy as np
from PIL import Image, ImageCms, ImageDraw, ImageFont, ImageOps
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from full_suite_photo_grain_seed_variance_study import ASSET_NAMES, MANIFEST, load_source, resize_long_edge
sys.path.insert(0,str(ROOT))
from retro_lab import BAYER_4, PRESETS, apply_retro, channel_offset, ordered_quantize
CAPTURE=ROOT/'captures'/'full-suite-photo-stage-endpoints'
SIZES=(512,1024,2048)
SEED=31111

def endpoint_stats(rgb: np.ndarray)->dict[str,object]:
    x=np.asarray(rgb,dtype=np.float32)
    low=x<=0.0; high=x>=1.0; end=low|high
    per_channel=end.mean(axis=(0,1))
    px=np.any(end,axis=2)
    return {'channel endpoint ratio':float(end.mean()),'channel low ratio':float(low.mean()),'channel high ratio':float(high.mean()),'channel ratio by RGB':[float(v) for v in per_channel], 'pixels with any endpoint ratio':float(px.mean()),'pixel count':int(x.shape[0]*x.shape[1]),'finite values':bool(np.isfinite(x).all())}

def byte_stats(rgb:np.ndarray)->dict[str,object]:
    b=np.uint8(np.clip(rgb*255.0,0,255))
    low=b==0; high=b==255; e=low|high
    return {'RGB8 endpoint channel ratio':float(e.mean()),'RGB8 zero channel ratio':float(low.mean()),'RGB8 255 channel ratio':float(high.mean()),'RGB8 pixel any endpoint ratio':float(np.any(e,axis=2).mean())}

def stages(image:Image.Image,preset:dict[str,object])->dict[str,np.ndarray]:
    rgb_image=image.convert('RGB'); w,h=rgb_image.size
    x=np.asarray(rgb_image,dtype=np.float32)/255.0
    out={'input':x}
    if float(preset['scale'])<1:
        s=float(preset['scale']); size=(max(1,round(w*s)),max(1,round(h*s)))
        rgb_image=rgb_image.resize(size,Image.Resampling.BOX).resize((w,h),Image.Resampling.NEAREST)
        x=np.asarray(rgb_image,dtype=np.float32)/255.0
    out['pixelation']=x
    x=ordered_quantize(x,max(2,int(preset['colors'])),BAYER_4,min(max(float(preset['dither']),0),1))
    out['quantization_dither']=x
    x=channel_offset(x,float(preset['aberration']))
    out['channel_offset']=x
    if float(preset['scanlines'])>0:
        gain=np.where(np.arange(h)[:,None,None]%2==1,1-min(float(preset['scanlines']),.9),1.0)
        x=x*gain
    out['scanlines']=x
    out['grain_off_preclamp']=x
    clipped=np.clip(x,0,1)
    out['grain_off_clipped']=clipped
    out['grain_off_rgb8']=np.uint8(clipped*255.0).astype(np.float32)/255.0
    return out

def endpoint_map(rgb:np.ndarray)->Image.Image:
    a=np.asarray(rgb)
    low=np.any(a<=0.0,axis=2); high=np.any(a>=1.0,axis=2)
    out=np.zeros((*low.shape,3),dtype=np.uint8)
    out[low & ~high]=(235,65,65); out[high & ~low]=(65,145,245); out[low & high]=(235,210,65)
    return Image.fromarray(out,'RGB')

def main()->dict[str,object]:
    CAPTURE.mkdir(parents=True,exist_ok=True)
    manifest=json.loads(MANIFEST.read_text(encoding='utf-8'))
    records=[]; source_records={}; sheet_paths=[]; parity=0
    for name in ASSET_NAMES:
        original,meta=load_source(name,manifest); source_records[name]=meta
        for size in SIZES:
            image=resize_long_edge(original,size)
            for preset_name,preset in PRESETS.items():
                values=stages(image,preset)
                actual=np.asarray(apply_retro(image.convert('RGBA'),**{**preset,'grain':0.0},seed=SEED).convert('RGB'),dtype=np.uint8)
                modeled=np.uint8(values['grain_off_rgb8']*255.0)
                if not np.array_equal(actual,modeled): raise RuntimeError(f'grain-off parity failed {name}/{size}/{preset_name}')
                parity+=1
                stage_records=[]
                for stage,array in values.items():
                    entry={'stage':stage,'float':endpoint_stats(array),'rgb8 quantized':byte_stats(array)}
                    stage_records.append(entry)
                transition=[]
                for prev,cur in zip(stage_records,stage_records[1:]):
                    a=values[prev['stage']]; b=values[cur['stage']]
                    ae=(a<=0)|(a>=1); be=(b<=0)|(b>=1)
                    transition.append({'from':prev['stage'],'to':cur['stage'],'new endpoint channels':float(np.mean(~ae&be)),'released endpoint channels':float(np.mean(ae&~be)),'net endpoint channel ratio delta':float(be.mean()-ae.mean())})
                records.append({'source':name,'input long edge':size,'preset':preset_name,'seed':SEED,'dimensions':list(image.size),'stages':stage_records,'stage transitions':transition})
                if size==512:
                    tile=Image.new('RGB',(512,5*26+5*image.height),(27,28,31)); draw=ImageDraw.Draw(tile); font=ImageFont.load_default()
                    for row,stage in enumerate(('input','pixelation','quantization_dither','channel_offset','scanlines')):
                        y=row*(image.height+26); draw.text((4,y+5),stage,fill=(245,245,245),font=font)
                        tile.paste(endpoint_map(values[stage]),(0,y+26))
                    tile.save(CAPTURE/f'{Path(name).stem}-{preset_name}-endpoint-maps.png')
    # Compare endpoint ratios across resolutions and stage contribution medians.
    summary={}
    stage_names=list(stages(resize_long_edge(load_source(ASSET_NAMES[0],manifest)[0],512),PRESETS['handheld']).keys())
    for stage in stage_names:
        summary[stage]={}
        for size in SIZES:
            rows=[next(x for x in r['stages'] if x['stage']==stage) for r in records if r['input long edge']==size]
            summary[stage][str(size)]={'float endpoint channel ratio median':float(np.median([x['float']['channel endpoint ratio'] for x in rows])),'float endpoint channel ratio p95':float(np.percentile([x['float']['channel endpoint ratio'] for x in rows],95)),'RGB8 endpoint channel ratio median':float(np.median([x['rgb8 quantized']['RGB8 endpoint channel ratio'] for x in rows]))}
    transitions={}
    for key in ('input->pixelation','pixelation->quantization_dither','quantization_dither->channel_offset','channel_offset->scanlines','scanlines->grain_off_preclamp'):
        rows=[t for r in records for t in r['stage transitions'] if t['from']+'->'+t['to']==key]
        transitions[key]={'new endpoint channel ratio median':float(np.median([x['new endpoint channels'] for x in rows])),'released endpoint channel ratio median':float(np.median([x['released endpoint channels'] for x in rows])),'net endpoint ratio delta median':float(np.median([x['net endpoint channel ratio delta'] for x in rows]))}
    report={'study':'full-suite-photo-stage-endpoint-provenance','date':'2026-11-11','environment':{'python':platform.python_version(),'numpy':np.__version__,'pillow':Image.__version__},'question':'At which current suite stage are exact <=0/>=1 RGB channel endpoints first introduced or released, before grain?','conditions':{'sources':source_records,'sizes':list(SIZES),'presets':{k:dict(v) for k,v in PRESETS.items()},'seed':SEED,'grain':'disabled; current stage order otherwise preserved','input':'provenance/SHA-verified photo corpus; EXIF and ICC normalization inherited from experiment runner','endpoint definition':'float channel <=0 or >=1; RGB8 code 0 or 255 tracked separately','exact parity gates':parity},'stage medians by input size':summary,'transition medians':transitions,'records':records,'limitations':['descriptive fixed suite and single seed; no appearance target or acceptance threshold','RGB8 endpoint occupancy is not a clipping defect metric','opaque photos only; no alpha/HDR','Python/Pillow path only; no Artifact/Core/GPU/preview/export or performance validation']}
    path=CAPTURE/'photo-stage-endpoint-provenance.json';path.write_text(json.dumps(report,indent=2,ensure_ascii=False,allow_nan=False)+'\n',encoding='utf-8')
    print(json.dumps({'report':str(path),'records':len(records),'grain-off exact RGB8 parity gates':parity,'stage medians by input size':summary,'transition medians':transitions},indent=2))
    return report
if __name__=='__main__': main()
