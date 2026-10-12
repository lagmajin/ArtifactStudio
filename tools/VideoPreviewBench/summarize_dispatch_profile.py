"""Summarize bounded callback/render diagnostics without inventing frame joins."""
import argparse,json,re,statistics
from pathlib import Path

def main():
 parser=argparse.ArgumentParser(description=__doc__)
 parser.add_argument('log',type=Path); parser.add_argument('--output',required=True,type=Path)
 args=parser.parse_args()
 if args.output.exists(): parser.error('Output already exists')
 text=args.log.read_text(encoding='utf-8',errors='replace')
 dispatch=[dict(queuedMs=float(a),frameAtCallback=int(b),frameSerialBefore=int(c)) for a,b,c in re.findall(r'VideoPreviewGuiDispatchProfile\] queuedMs=([0-9.]+) frameAtCallback=(-?\d+) frameSerialBefore=(\d+)',text)]
 renders=[dict(frameSerial=int(a),frameAtRenderStart=int(b),totalMs=float(c)) for a,b,c in re.findall(r'VideoPreviewRenderDurationProfile\] frameSerial=(\d+) frameAtRenderStart=(-?\d+) totalMs=([0-9.]+)',text)]
 if not dispatch or not renders: parser.error('Missing frame-labelled diagnostics')
 def stats(rows,key):
  values=[r[key] for r in rows]
  return dict(samples=len(values),medianMs=statistics.median(values),maximumMs=max(values))
 slow=[r for r in dispatch if r['queuedMs']>20.0]
 result=dict(schema='artifact-dispatch-profile-summary-v1',log=str(args.log.resolve()),dispatch=dispatch,renders=renders,dispatchTiming=stats(dispatch,'queuedMs'),renderTiming=stats(renders,'totalMs'),frameJoinVerified=False,slowDispatchCount=len(slow),slowDispatches=slow,caveats=['Callback position is not the requested frame at posting','Idle callbacks and render skips prevent automatic serial+1 joins','Bounded profiled sample; not normal performance acceptance'])
 args.output.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8',newline='\n')
 print(json.dumps({k:result[k] for k in ['dispatchTiming','renderTiming','frameJoinVerified']}))
if __name__=='__main__': main()
