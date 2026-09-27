#!/usr/bin/env python3
"""Generate original mode icons, JSONL decoration and offline previews.
Requires Pillow and CairoSVG. No Nextion artwork or icon-font downloads.
"""
from pathlib import Path
import json, math
import cairosvg
from PIL import Image, ImageDraw, ImageFont
ROOT = Path(__file__).resolve().parents[1]
ICONS = ROOT / 'icons'
ICONS.mkdir(exist_ok=True)
# Original 64-unit line drawings. Paths describe mode concepts, not toolpaths.
def path(d): return f'<path d="{d}"/>'
def circle(x,y,r): return f'<circle cx="{x}" cy="{y}" r="{r}"/>'
def gear(x,y,r):
    points=[]
    for i in range(48):
        a=i*math.pi/24; rr=r if i%4 in (0,1) else r*.77
        points.append(f'{x+rr*math.cos(a):.2f},{y+rr*math.sin(a):.2f}')
    return f'<polygon points="{" ".join(points)}"/>'+circle(x,y,r*.32)
stock = path('M6 14 H54 V34 H6 Z M6 24 H58')
tool = path('M37 57 V43 L29 35 H43 V57')
shapes={
 'gear': gear(24,25,17)+gear(46,46,12),
 'turn': stock+tool+path('M46 45 H56 M51 40 L56 45 L51 50'),
 'face': stock+path('M58 8 V40 M44 56 V47 L54 37 V56 M45 40 V31 M40 36 L45 31 L50 36'),
 'cone': path('M6 11 H18 L55 27 V38 L18 49 H6 Z M6 30 H59')+path('M39 60 V53 L32 46 H47 V60'),
 'cut': stock+path('M32 15 V33 M28 58 V36 H36 V58 M44 52 V40 M39 45 L44 40 L49 45'),
 'thread': stock+''.join(path(f'M{x} 15 L{x-7} 33') for x in (18,28,38,48,58))+tool,
 'ellipse': '<ellipse cx="29" cy="26" rx="23" ry="15"/>'+path('M5 26 H55')+tool,
 'gcode': path('M22 10 L9 31 L22 52 M42 10 L55 31 L42 52 M36 16 L27 47'),
 'async': circle(31,31,23)+path('M31 15 V31 L44 39 M5 51 H20 M14 46 L20 51 L14 56'),
 'y': path('M32 57 V8 M25 15 L32 8 L39 15 M12 31 H52 M12 36 V26 M52 36 V26')+circle(32,31,5),
 'xgear': gear(24,28,19)+path('M49 57 V14 M43 20 L49 14 L55 20 M43 51 L49 57 L55 51'),
 'joy': '<ellipse cx="32" cy="51" rx="23" ry="8"/>'+path('M32 47 V24')+circle(32,15,9)+path('M10 25 V38 M5 32 L10 38 L15 32 M49 32 H61 M55 26 L61 32 L55 38'),
 'slot': path('M6 15 H57 V37 H6 Z M14 22 H47 V30 H14 Z M28 58 V44 H39 V58 M32 43 V31')+path('M13 48 H23 M18 43 L13 48 L18 53'),
}
rows=[json.loads(line) for line in (ROOT/'pages.jsonl').read_text().splitlines() if line.strip()]
rows=[r for r in rows if not r.get('comment','').startswith('modeIcon:')]
buttons=[r for r in rows if r['page']==2 and r.get('obj')=='btn' and r['id']!=100]
for b in buttons:
    name=b['text'].lower();color=b['text_color']
    svg=f'<svg xmlns="http://www.w3.org/2000/svg" width="64" height="64" viewBox="0 0 64 64"><g fill="none" stroke="{color}" stroke-width="2.6" stroke-linecap="round" stroke-linejoin="round">{shapes[name]}</g></svg>\n'
    (ICONS/(name+'.svg')).write_text(svg)
    cairosvg.svg2png(bytestring=svg.encode(),write_to=str(ICONS/(name+'.png')))
    b['pad_left']=72
    rows.append({'page':2,'id':b['id']+100,'obj':'img','x':b['x']+6,'y':b['y']+8,'w':64,'h':64,
                 'src':'L:/icons/'+name+'.png','click':False,'comment':'modeIcon:'+name})
(ROOT/'pages.jsonl').write_text(''.join(json.dumps(r,separators=(',',':'),ensure_ascii=False)+'\n' for r in rows))
# Offline geometry proof only; not an LVGL render or a claim of touch validation.
values={}
for line in (ROOT/'demo.cmd').read_text().splitlines():
    if line.startswith('jsonl '):
        d=json.loads(line[6:]);values[(d['page'],d['id'])]=d['text']
images=[]
for page,filename in [(1,'main-preview.png'),(2,'modes-preview.png')]:
    im=Image.new('RGB',(800,480),'black');draw=ImageDraw.Draw(im)
    for r in rows:
        if r['page']!=page or not r['id']:continue
        x,y,w,h=[r[k] for k in ('x','y','w','h')]
        if r['obj']=='img':
            icon=Image.open(ROOT/r['src'].removeprefix('L:/')).convert('RGBA');im.paste(icon,(x,y),icon);continue
        draw.rectangle((x,y,x+w-1,y+h-1),fill=r.get('bg_color','#000000'))
        text=values.get((page,r['id']),r.get('text',''))
        size=int(r.get('text_font','mono16').replace('mono',''))
        font=ImageFont.truetype(str(ROOT/'mono.ttf'),size)
        length=draw.textlength(text,font=font);left=r.get('pad_left',0);right=r.get('pad_right',0)
        px=x+left if r.get('align')=='left' else x+w-right-length if r.get('align')=='right' else x+left+(w-left-right-length)/2
        # Center visible glyph box in field; approximate LVGL baseline behavior.
        box=draw.textbbox((0,0),text,font=font)
        draw.text((px,y+(h-(box[3]-box[1]))/2-box[1]),text,font=font,fill=r.get('text_color','#FFFFFF'))
    im.save(ROOT/filename);images.append(im)
combined=Image.new('RGB',(800,960),'black');combined.paste(images[0],(0,0));combined.paste(images[1],(0,480));combined.save(ROOT/'layout-preview.png')
# Invariants protect the wire mapping and click targets.
keys=[(r['page'],r['id']) for r in rows];assert len(keys)==len(set(keys))
assert len(buttons)==13
mapping=json.loads((ROOT/'mapping.json').read_text())
for c in mapping['components']:
    target=next(r for r in rows if (r['page'],r['id'])==(c['page'],c['id']))
    if c.get('action') and c.get('nextion_component_id') is not None:assert target.get('click') and target['tag']['nextion_id']==c['nextion_component_id']
for r in rows:
    if r.get('comment','').startswith('modeIcon:'):
        assert not r['click'] and 0<=r['x'] and r['x']+r['w']<=800 and r['y']+r['h']<=480
print('PASS: 13 icons; unique IDs, bounds and all mapped actions preserved')
