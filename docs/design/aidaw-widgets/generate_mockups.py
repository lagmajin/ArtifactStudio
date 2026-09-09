from PIL import Image, ImageDraw, ImageFont
import math
from pathlib import Path

OUT = Path(__file__).parent
W, H = 1600, 960
BG = (17, 19, 22)
PANEL = (27, 30, 34)
PANEL_2 = (33, 36, 41)
PANEL_3 = (40, 44, 49)
LINE = (61, 66, 73)
TEXT = (231, 233, 236)
MUTED = (149, 155, 163)
AMBER = (240, 174, 66)
AMBER_D = (139, 91, 24)
GREEN = (67, 192, 116)
CYAN = (62, 177, 202)
PURPLE = (157, 103, 222)
RED = (225, 82, 77)
BLUE = (78, 128, 221)

FONT_PATHS = [
    Path('C:/Windows/Fonts/segoeui.ttf'),
    Path('C:/Windows/Fonts/arial.ttf'),
]
BOLD_PATHS = [
    Path('C:/Windows/Fonts/seguisb.ttf'),
    Path('C:/Windows/Fonts/arialbd.ttf'),
]

def font(size, bold=False):
    paths = BOLD_PATHS if bold else FONT_PATHS
    for p in paths:
        if p.exists():
            return ImageFont.truetype(str(p), size)
    return ImageFont.load_default()

F11, F12, F13, F14, F16, F18, F22, F28 = [font(x) for x in (11,12,13,14,16,18,22,28)]
B11, B12, B13, B14, B16, B18, B22, B28 = [font(x, True) for x in (11,12,13,14,16,18,22,28)]

def gradient(size, top, bottom):
    im = Image.new('RGB', size, top)
    d = ImageDraw.Draw(im)
    for y in range(size[1]):
        t = y / max(1, size[1]-1)
        c = tuple(int(top[i]*(1-t)+bottom[i]*t) for i in range(3))
        d.line((0,y,size[0],y), fill=c)
    return im

def rr(d, box, radius=8, fill=PANEL, outline=LINE, width=1):
    d.rounded_rectangle(box, radius, fill=fill, outline=outline, width=width)

def txt(d, xy, value, fill=TEXT, f=F14, anchor=None):
    d.text(xy, value, fill=fill, font=f, anchor=anchor)

def button(d, box, label, active=False, accent=False, f=B12):
    fill = (63,48,26) if accent else ((52,55,61) if active else (34,37,42))
    outline = AMBER if (active or accent) else LINE
    rr(d, box, 5, fill, outline)
    txt(d, ((box[0]+box[2])//2, (box[1]+box[3])//2), label,
        AMBER if (active or accent) else TEXT, f, 'mm')

def topbar(d, title, subtitle):
    d.rectangle((0,0,W,58), fill=(22,24,27))
    d.line((0,57,W,57), fill=(66,70,76))
    rr(d,(16,14,44,42),6,(39,42,47),LINE)
    for x,h in [(23,12),(29,20),(35,15)]:
        d.line((x,36-h,x,36),fill=AMBER,width=3)
    txt(d,(58,20),'AIDAW',TEXT,B18)
    d.line((127,15,127,43),fill=LINE)
    txt(d,(143,20),title,TEXT,B18)
    txt(d,(W-22,29),subtitle,MUTED,F12,'rm')

def transport(d, tempo='120.00', key='C minor'):
    y=892
    d.rectangle((0,y,W,H),fill=(22,24,27))
    d.line((0,y,W,y),fill=LINE)
    for i,label in enumerate(['|<','>','||','[]','<>']):
        button(d,(18+i*58,y+14,66+i*58,y+52),label,active=(i==1),f=B14)
    txt(d,(345,y+33),'00:00:12:08',AMBER,B22,'lm')
    d.line((520,y+10,520,H-10),fill=LINE)
    txt(d,(548,y+24),tempo,TEXT,B14)
    txt(d,(548,y+43),'BPM',MUTED,F11)
    txt(d,(630,y+24),'4 / 4',TEXT,B14)
    txt(d,(630,y+43),'METER',MUTED,F11)
    txt(d,(704,y+24),key,TEXT,B14)
    txt(d,(704,y+43),'KEY',MUTED,F11)
    rr(d,(1260,y+14,1578,y+52),6,(28,31,35),LINE)
    txt(d,(1280,y+33),'AI credits  84%',MUTED,F12,'lm')
    d.rectangle((1390,y+28,1558,y+36),fill=(12,13,15))
    d.rectangle((1390,y+28,1531,y+36),fill=AMBER)

def draw_track_mockup():
    im=gradient((W,H),(23,25,29),(13,15,18)); d=ImageDraw.Draw(im)
    topbar(d,'ARRANGEMENT','Cloud AI connected  •  autosave on')
    # command toolbar
    d.rectangle((0,58,W,112),fill=(29,32,36)); d.line((0,111,W,111),fill=LINE)
    button(d,(18,70,105,100),'+ Track',accent=True)
    button(d,(116,70,194,100),'Record')
    button(d,(205,70,283,100),'Quantize')
    button(d,(294,70,372,100),'Humanize')
    rr(d,(1008,68,1578,102),7,(24,27,31),LINE)
    txt(d,(1024,85),'Ask AI: make the melody warmer and less busy',TEXT,F13,'lm')
    rr(d,(1502,72,1573,98),5,(67,48,22),AMBER)
    txt(d,(1537,85),'Suggest',AMBER,B12,'mm')
    # ruler
    left=338; top=146; bottom=846
    d.rectangle((0,112,left,H),fill=(25,28,32)); d.line((left,112,left,892),fill=(76,80,87))
    d.rectangle((left,112,W,top),fill=(23,26,30))
    beat_w=74
    for beat in range(0,18):
        x=left+beat*beat_w
        if x>W: break
        d.line((x,112,x,bottom),fill=(62,66,72) if beat%4==0 else (42,45,50),width=1)
        if beat%4==0: txt(d,(x+7,128),str(beat//4+1),MUTED,F12,'lm')
    # locator/work area
    d.rectangle((left+beat_w,left+4*beat_w,0,0) if False else (left+beat_w,116,left+13*beat_w,121),fill=AMBER)
    # column headers
    txt(d,(18,129),'TRACKS',MUTED,B12)
    txt(d,(210,129),'I/O',MUTED,B12)
    rows=[
      ('01','CHORDS','MIDI','AI seed • Am7  Fmaj7  C  G',PURPLE),
      ('02','MELODY','MIDI','AI suggestion • Variation B',AMBER),
      ('03','BASS','MIDI','Mono bass • Follow chords',CYAN),
      ('04','DRUMS','MIDI','Kit 07 • 72% humanize',RED),
      ('05','TEXTURE','AUDIO','vinyl-room-02.wav',GREEN),
    ]
    row_h=132
    for idx,(num,name,kind,desc,color) in enumerate(rows):
        y=top+idx*row_h
        selected=idx==1
        d.rectangle((0,y,W,y+row_h-1),fill=(35,36,40) if selected else ((29,32,36) if idx%2==0 else (26,29,33)))
        d.line((0,y+row_h-1,W,y+row_h-1),fill=(49,53,59))
        d.rectangle((0,y,5,y+row_h-1),fill=color)
        rr(d,(17,y+18,49,y+50),5,(44,47,53),color)
        txt(d,(33,y+34),num,color,B12,'mm')
        txt(d,(61,y+20),name,TEXT,B14)
        txt(d,(61,y+44),kind,MUTED,F11)
        button(d,(18,y+70,51,y+98),'M',active=(idx==4))
        button(d,(57,y+70,90,y+98),'S')
        button(d,(96,y+70,129,y+98),'R',active=(idx==2))
        button(d,(135,y+70,168,y+98),'A',active=(idx in (0,1,2)),f=B11)
        txt(d,(190,y+27),'IN',MUTED,F11); txt(d,(222,y+27),'All MIDI' if kind=='MIDI' else 'Stereo',TEXT,F11)
        txt(d,(190,y+49),'OUT',MUTED,F11); txt(d,(222,y+49),'Master',TEXT,F11)
        txt(d,(190,y+81),'-6.0 dB' if idx!=1 else '-3.5 dB',AMBER,F12)
        # clips
        cy=y+20; ch=90
        if idx<4:
            starts=[0.5,4.5,8.5,12.5]
            for ci,s in enumerate(starts):
                x=left+s*beat_w; cw=3.6*beat_w
                if x>W: continue
                rr(d,(x,cy,min(x+cw,W-10),cy+ch),5,tuple(max(0,c//4) for c in color),color if selected else tuple(int(c*.72) for c in color))
                txt(d,(x+10,cy+10),desc if ci==0 else f'{name.title()} {ci+1}',TEXT,F11)
                # midi notes
                for n in range(9):
                    nx=x+12+n*(cw-28)/9
                    ny=cy+35+((n*3+ci*2)%6)*7
                    nw=12+(n%3)*6
                    d.rounded_rectangle((nx,ny,min(nx+nw,x+cw-8),ny+4),2,fill=color)
        else:
            x=left+0.5*beat_w; cw=min(15.2*beat_w,W-x-12)
            rr(d,(x,cy,x+cw,cy+ch),5,(20,51,42),GREEN)
            txt(d,(x+10,cy+10),desc,TEXT,F11)
            mid=cy+59
            points=[]
            for px in range(int(x+8),int(x+cw-8),3):
                amp=8+18*abs(math.sin(px*.063)*math.sin(px*.017))
                points.extend([(px,mid-amp),(px,mid+amp)])
            for p in range(0,len(points),2): d.line((points[p],points[p+1]),fill=(86,206,137),width=1)
        if selected:
            d.rectangle((5,y,333,y+row_h-1),outline=AMBER,width=2)
            rr(d,(1120,y+82,1557,y+118),6,(51,40,24),AMBER)
            txt(d,(1136,y+100),'AI  3 variations ready',AMBER,B12,'lm')
            button(d,(1373,y+87,1458,y+113),'Preview',active=True)
            button(d,(1466,y+87,1549,y+113),'Accept',accent=True)
    # playhead
    px=left+6.35*beat_w
    d.line((px,112,px,bottom),fill=AMBER,width=2)
    d.polygon([(px-7,112),(px+7,112),(px,124)],fill=AMBER)
    # lower status
    d.rectangle((0,806,W,892),fill=(23,26,30)); d.line((0,806,W,806),fill=LINE)
    txt(d,(18,828),'SELECTED',MUTED,B11); txt(d,(18,852),'MELODY  •  Variation B',TEXT,B14)
    txt(d,(350,828),'AI INTENT',MUTED,B11); txt(d,(350,852),'Warm / sparse / singable  •  confidence 88%',TEXT,F13)
    button(d,(1268,826,1360,864),'Compare')
    button(d,(1370,826,1462,864),'Regenerate')
    button(d,(1472,826,1578,864),'Accept',accent=True)
    transport(d)
    im.save(OUT/'aidaw-track-widget-mockup-2026-09-09.png')


def meter(d,x,y,h,levels):
    for lane,level in enumerate(levels):
        lx=x+lane*17
        d.rounded_rectangle((lx,y,lx+11,y+h),3,fill=(12,14,16),outline=(50,54,59))
        segments=18
        on=int(segments*level)
        for s in range(segments):
            sy=y+h-5-s*(h-8)/segments
            col=GREEN if s<12 else (237,205,62) if s<16 else RED
            d.rectangle((lx+2,sy,lx+9,sy+3),fill=col if s<on else (28,38,33))

def knob(d,cx,cy,value,color=AMBER):
    d.ellipse((cx-23,cy-23,cx+23,cy+23),fill=(13,15,17),outline=(70,74,79),width=2)
    d.ellipse((cx-18,cy-18,cx+18,cy+18),fill=(38,41,45),outline=(92,95,101))
    ang=math.radians(225+270*value)
    d.line((cx,cy,cx+15*math.cos(ang),cy+15*math.sin(ang)),fill=color,width=3)

def draw_mixer_mockup():
    im=gradient((W,H),(23,25,28),(13,15,17)); d=ImageDraw.Draw(im)
    topbar(d,'MIXER','Arrangement 01  •  Follow selection')
    d.rectangle((0,58,W,112),fill=(29,32,36)); d.line((0,111,W,111),fill=LINE)
    txt(d,(20,85),'BUS',MUTED,F12,'lm'); button(d,(58,69,236,101),'Arrangement 01  v')
    button(d,(248,69,335,101),'Routing')
    button(d,(346,69,430,101),'Sends')
    rr(d,(1004,69,1578,101),6,(24,27,31),LINE)
    txt(d,(1020,85),'AI mix assistant: vocal space +2.0 dB',TEXT,F13,'lm')
    button(d,(1468,73,1572,97),'Review',accent=True)
    channels=[
      ('CHORDS','MIDI 01',PURPLE,-7.2,.54,.48),
      ('MELODY','MIDI 02',AMBER,-3.5,.62,.58),
      ('BASS','MIDI 03',CYAN,-5.8,.50,.66),
      ('DRUMS','MIDI 04',RED,-4.0,.56,.78),
      ('TEXTURE','AUDIO 05',GREEN,-11.5,.44,.42),
      ('REVERB','RETURN A',BLUE,-9.0,.50,.35),
      ('DELAY','RETURN B',(203,104,187),-12.0,.58,.28),
      ('MASTER','STEREO',TEXT,-1.2,.50,.87),
    ]
    margin=14; gap=5; strip_w=(W-margin*2-gap*7)//8; top=118; bottom=884
    for i,(name,kind,color,db,pan,lev) in enumerate(channels):
        x=margin+i*(strip_w+gap); selected=i==1; master=i==7
        fill=(34,37,41) if not selected else (42,39,34)
        d.rectangle((x,top,x+strip_w,bottom),fill=fill,outline=AMBER if selected else LINE,width=2 if selected else 1)
        d.rectangle((x,top,x+strip_w,125),fill=color)
        txt(d,(x+strip_w//2,146),name,TEXT,B14,'mm')
        txt(d,(x+strip_w//2,169),kind,MUTED,F11,'mm')
        # inserts
        txt(d,(x+12,194),'INSERTS',MUTED,F11)
        inserts=['Chord Voice','Soft Comp'] if i==0 else ['AI EQ','Tape Sat'] if i==1 else ['Mono Bass','Sidechain'] if i==2 else ['Transient','Bus Comp'] if i==3 else ['Low Cut','Space'] if i==4 else ['Plate 2.1s','Lo Cut'] if i==5 else ['1/8 Dotted','Duck'] if i==6 else ['Glue Comp','Limiter']
        for n,label in enumerate(inserts):
            yy=207+n*38; rr(d,(x+10,yy,x+strip_w-10,yy+30),4,(26,29,33),LINE)
            txt(d,(x+19,yy+15),label,TEXT,F11,'lm'); d.ellipse((x+strip_w-27,yy+10,x+strip_w-17,yy+20),fill=GREEN)
        # send
        txt(d,(x+12,293),'SEND',MUTED,F11); knob(d,x+strip_w//2,335,.38+i*.04,color)
        txt(d,(x+strip_w//2,369),'-12.0',AMBER,F11,'mm')
        # pan
        txt(d,(x+12,391),'PAN',MUTED,F11); knob(d,x+strip_w//2,430,pan,color)
        pantext='C' if abs(pan-.5)<.03 else ('18L' if pan<.5 else '12R')
        txt(d,(x+strip_w//2,464),pantext,AMBER,B12,'mm')
        # mute solo
        button(d,(x+17,482,x+72,512),'M',active=(i==4))
        button(d,(x+80,482,x+135,512),'S')
        button(d,(x+143,482,x+strip_w-17,512),'R',active=(i==1))
        # fader + meters
        fy=544; fh=245
        for tick,label in [(0,'+6'),(.12,'0'),(.32,'-12'),(.54,'-24'),(.76,'-36'),(1,'-60')]:
            yy=fy+int(fh*tick); txt(d,(x+10,yy),label,MUTED,F10 if False else F11,'lm'); d.line((x+37,yy,x+48,yy),fill=LINE)
        railx=x+72
        d.rounded_rectangle((railx,fy,railx+15,fy+fh),6,fill=(12,14,16),outline=(49,53,58))
        fader_y=fy+int((6-db)/66*fh)
        d.rounded_rectangle((railx-15,fader_y-14,railx+30,fader_y+14),4,fill=(133,136,140),outline=(211,213,216))
        d.line((railx-11,fader_y,railx+26,fader_y),fill=(55,57,60),width=2)
        meter(d,x+113,fy,fh,(lev,max(.05,lev-.04)))
        txt(d,(x+strip_w//2,816),f'{db:+.1f} dB',AMBER,B13,'mm')
        txt(d,(x+strip_w//2,848),'AI +1.5' if selected else ('PEAK -0.7' if master else ' '),AMBER if selected else MUTED,F11,'mm')
    transport(d,tempo='120.00',key='C minor')
    im.save(OUT/'aidaw-audio-mixer-widget-mockup-2026-09-09.png')

if __name__ == '__main__':
    draw_track_mockup()
    draw_mixer_mockup()
