import math, json, sys
def dec(c): return c/12.92 if c<=0.04045 else ((c+0.055)/1.055)**2.4
def enc(c): return c*12.92 if c<=0.0031308 else 1.055*c**(1/2.4)-0.055
def hexrgb(h): h=int(h.lstrip('#'),16); return ((h>>16)&255)/255,((h>>8)&255)/255,(h&255)/255
def tohex(rgb): return '#%02X%02X%02X'%tuple(max(0,min(255,round(c*255))) for c in rgb)
def oklab(rgb):
    r,g,b=[dec(c) for c in rgb]
    l=(0.4122214708*r+0.5363325363*g+0.0514459929*b)**(1/3)
    m=(0.2119034982*r+0.6806995451*g+0.1073969566*b)**(1/3)
    s=(0.0883024619*r+0.2817188376*g+0.6299787005*b)**(1/3)
    return (0.2104542553*l+0.7936177850*m-0.0040720468*s,1.9779984951*l-2.4285922050*m+0.4505937099*s,0.0259040371*l+0.7827717662*m-0.8086757660*s)
def fromlab(L,a,b):
    l=(L+0.3963377774*a+0.2158037573*b)**3; m=(L-0.1055613458*a-0.0638541728*b)**3; s=(L-0.0894841775*a-1.2914855480*b)**3
    r=4.0767416621*l-3.3077115913*m+0.2309699292*s; g=-1.2684380046*l+2.6097574011*m-0.3413193965*s; bb=-0.0041960863*l-0.7034186147*m+1.7076147010*s
    return tuple(enc(max(0,min(1,c))) for c in (r,g,bb))
def L(rgb): return oklab(rgb)[0]
def withL(rgb,Lt):
    _,a,b=oklab(rgb); return fromlab(Lt,a,b)
def lum(rgb):
    r,g,b=[dec(c) for c in rgb]; return 0.2126*r+0.7152*g+0.0722*b
def contrast(x,y):
    a,b=lum(x)+0.05,lum(y)+0.05; return max(a,b)/min(a,b)
def over(fg,alpha,bg): return tuple(f*alpha+b*(1-alpha) for f,b in zip(fg,bg))
def parse(ref):
    if '@' in ref:
        h,a=ref.split('@'); return hexrgb(h),float(a.rstrip('%'))/100
    return hexrgb(ref),1.0
COLS=['light','dark','dark_elevated','hc_light','hc_dark','hc_dark_elevated']
DARK={'light':False,'dark':True,'dark_elevated':True,'hc_light':False,'hc_dark':True,'hc_dark_elevated':True}
def mac(light,dark): return {'light':light,'dark':dark,'dark_elevated':dark,'hc_light':light,'hc_dark':dark,'hc_dark_elevated':dark,'src':'macOS'}
def ios(*v): return dict(zip(COLS,v),src='iOS')
def hig(light,dark,hcl,hcd): return {'light':light,'dark':dark,'dark_elevated':dark,'hc_light':hcl,'hc_dark':hcd,'hc_dark_elevated':hcd,'src':'HIG'}
AREA={False:0.10,True:0.05}
HAIR={False:0.30,True:0.13}
ROWS=[
 ('window_background',mac('#FFFFFF','#1E1E1E'),('fixedL',0.81,None)),
 ('control_background',mac('#FFFFFF','#1E1E1E'),('ladderup','window_background')),
 ('system_background',ios('#FFFFFF','#000000','#1C1C1E','#FFFFFF','#000000','#242426'),('exact',)),
 ('secondary_system_background',ios('#F2F2F7','#1C1C1E','#2C2C2E','#EBEBF0','#242426','#363638'),('area','system_background')),
 ('tertiary_system_background',ios('#FFFFFF','#2C2C2E','#3A3A3C','#FFFFFF','#363638','#444446'),('exact',)),
 ('system_grouped_background',ios('#F2F2F7','#000000','#1C1C1E','#EBEBF0','#000000','#242426'),('fixedL',0.81,None)),
 ('secondary_system_grouped_background',ios('#FFFFFF','#1C1C1E','#2C2C2E','#FFFFFF','#242426','#363638'),('area','system_grouped_background')),
 ('tertiary_system_grouped_background',ios('#F2F2F7','#2C2C2E','#3A3A3C','#EBEBF0','#363638','#444446'),('area','secondary_system_grouped_background')),
 ('under_page_background',mac('#F6F6F6','#282828'),('area','window_background')),
 ('text_background',mac('#FFFFFF','#1E1E1E'),('ladderup','window_background')),
 ('alternating_content_background',mac('#FFFFFF','#1E1E1E'),('ladderup','window_background')),
 ('alternating_content_background_alternate',mac('#F4F5F5','#FFFFFF@5%'),('over','alternating_content_background','area','alternating_content_background')),
 ('scrubber_textured_background',mac('#F6F6F6','#282828'),('area','window_background')),
 ('label',mac('#000000@85%','#FFFFFF@85%'),('over','control_background','text','window_background',4.5)),
 ('secondary_label',mac('#000000@50%','#FFFFFF@55%'),('over','control_background','text','window_background',4.5)),
 ('tertiary_label',mac('#000000@26%','#FFFFFF@25%'),('over','control_background','text','control_background',3.0)),
 ('quaternary_label',mac('#000000@10%','#FFFFFF@10%'),('over','control_background','text','control_background',2.0)),
 ('quinary_label',mac('#000000@5%','#FFFFFF@5%'),('over','control_background','text','control_background',1.5)),
 ('placeholder_text',mac('#000000@50%','#FFFFFF@55%'),('over','control_background','text','control_background',3.0)),
 ('text',mac('#000000','#FFFFFF'),('exact',)),
 ('header_text',mac('#000000@85%','#FFFFFF'),('over','window_background','text','window_background',4.5)),
 ('window_frame_text',mac('#000000@85%','#FFFFFF@85%'),('over','window_background','text','window_background',4.5)),
 ('link',mac('#0068DA','#419CFF'),('text','control_background',4.5)),
 ('system_fill',mac('#000000@10%','#FFFFFF@10%'),('over','control_background','fill','control_background',0.10)),
 ('secondary_system_fill',mac('#000000@8%','#FFFFFF@8%'),('over','control_background','fill','control_background',0.08)),
 ('tertiary_system_fill',mac('#000000@5%','#FFFFFF@5%'),('over','control_background','fill','control_background',0.05)),
 ('quaternary_system_fill',mac('#000000@3%','#FFFFFF@3%'),('over','control_background','fill','control_background',0.03)),
 ('quinary_system_fill',mac('#000000@1%','#FFFFFF@1%'),('over','control_background','fill','control_background',0.01)),
 ('separator',mac('#000000@10%','#FFFFFF@10%'),('over','control_background','hair','control_background')),
 ('opaque_separator',ios('#E7E7E8','#2A2A2C','#2A2A2C','#E7E7E8','#2A2A2C','#2A2A2C'),('hair','control_background')),
 ('grid',mac('#E6E6E6','#1A1A1A'),('hair','control_background')),
 ('control',mac('#FFFFFF','#FFFFFF@25%'),('over','control_background','fill','control_background',0.08)),
 ('control_text',mac('#000000@85%','#FFFFFF@85%'),('over','control','text','control',4.5)),
 ('disabled_control_text',mac('#000000@25%','#FFFFFF@25%'),('over','control','text','control',2.0)),
 ('selected_control',mac('#B3D7FF','#3F638B'),('exact',)),
 ('selected_control_text',mac('#000000@85%','#FFFFFF@85%'),('over','selected_control','text','selected_control',4.5)),
 ('alternate_selected_control_text',mac('#FFFFFF','#FFFFFF'),('exact',)),
 ('selected_text',mac('#000000','#FFFFFF'),('exact',)),
 ('selected_text_background',mac('#B3D7FF','#3F638B'),('exact',)),
 ('selected_menu_item_text',mac('#FFFFFF','#FFFFFF'),('exact',)),
 ('selected_content_background',mac('#0064E1','#0059D1'),('exact',)),
 ('unemphasized_selected_content_background',mac('#DCDCDC','#464646'),('area','secondary_system_background')),
 ('unemphasized_selected_text_background',mac('#DCDCDC','#464646'),('area','control_background')),
 ('unemphasized_selected_text',mac('#000000','#FFFFFF'),('exact',)),
 ('keyboard_focus_indicator',mac('#0067F4@50%','#1AA9FF@50%'),('over','control_background')),
 ('find_highlight',mac('#FFFF00','#FFFF00'),('exact',)),
 ('highlight',mac('#FFFFFF','#B4B4B4'),('exact',)),
 ('shadow',mac('#000000','#000000'),('exact',)),
 ('dark_text',ios('#000000','#000000','#000000','#000000','#000000','#000000'),('exact',)),
 ('light_text',ios('#FFFFFF@60%','#FFFFFF@60%','#FFFFFF@60%','#FFFFFF@60%','#FFFFFF@60%','#FFFFFF@60%'),('overfixed','#000000')),
]
HUES=[('red','#FF383C','#FF4245','#E9152D','#FF6165'),('orange','#FF8D28','#FF9230','#C55300','#FFA056'),('yellow','#FFCC00','#FFD600','#A16A00','#FEDF43'),('green','#34C759','#30D158','#008932','#4AD968'),('mint','#00C8B3','#00DAC3','#008575','#54DFCB'),('teal','#00C3D0','#00D2E0','#008198','#3BDDEC'),('cyan','#00C0E8','#3CD3FE','#007EAE','#6DD9FF'),('blue','#0088FF','#0091FF','#1E6EF4','#5CB8FF'),('indigo','#6155F5','#6D7CFF','#564ADE','#A7AAFF'),('purple','#CB30E0','#DB34F2','#B02FC2','#EA8DFF'),('pink','#FF2D55','#FF375F','#E7124D','#FF8AC4'),('brown','#AC7F5E','#B78A66','#956D51','#DBA679')]
for name,l,d,hl,hd in HUES: ROWS.append((name,hig(l,d,hl,hd),('hue',)))
ROWS.append(('gray',{'light':'#8E8E93','dark':'#98989D','dark_elevated':'#98989D','hc_light':'#6C6C70','hc_dark':'#AEAEB2','hc_dark_elevated':'#AEAEB2','src':'macOS (iOS hc)'},('exact',)))
for name,l,d,hl,hd in [('gray2','#AEAEB2','#636366','#8E8E93','#7C7C80'),('gray3','#C7C7CC','#48484A','#AEAEB2','#545456'),('gray4','#D1D1D6','#3A3A3C','#BCBCC0','#444446'),('gray5','#E5E5EA','#2C2C2E','#D8D8DC','#363638'),('gray6','#F2F2F7','#1C1C1E','#EBEBF0','#242426')]:
    ROWS.append((name,hig(l,d,hl,hd),('exact',)))
READABLE={'label':'system_grouped_background','secondary_label':'system_grouped_background'}
LADDER=['#1C1C1E','#2C2C2E','#3A3A3C','#48484A','#636366']
final={c:{} for c in COLS}; reason={}
def away(ref,ground,step,dark,lighter_in_dark=False):
    lr,lg=L(ref),L(ground)
    if lighter_in_dark and dark and lr<lg+step:
        return withL(ref,lg+step),lg+step-lr
    if abs(lr-lg)>=step-1e-4: return ref,None
    direction = 1 if (lr>lg or (abs(lr-lg)<1e-4 and dark)) else -1
    if abs(lr-lg)<1e-4 and not dark: direction=-1
    t=lg+direction*step
    if t>1: t=lg-step
    if t<0: t=lg+step
    return withL(ref,t), t-lr
def textfix(ref,ground,ratio,dark):
    right_side = (L(ref)>L(ground)) if dark else (L(ref)<L(ground))
    ratio=ratio+0.03 if ratio>1.6 else ratio
    if right_side and contrast(ref,ground)>=ratio-1e-3: return ref,None
    if not right_side:
        ref=withL(ref,L(ground))
    lr=L(ref); step=0.002; t=lr
    lr0=lr
    while contrast(withL(ref,t),ground)<ratio and 0<t<1:
        t += step if dark else -step
    return withL(ref,t), t-lr
for name,refs,rule in ROWS:
    reason[name]={}
    for col in COLS:
        dark=DARK[col]; rgb,alpha=parse(refs[col]); why='exact'; kind=rule[0]
        if kind=='overfixed':
            rgb=over(rgb,alpha,hexrgb(rule[1])); why='composited over %s'%rule[1]
        elif kind=='over':
            rgb=over(rgb,alpha,final[col][rule[1]]); why='composited over %s'%rule[1]
        elif alpha<1:
            raise SystemExit('translucent without over: '+name)
        elif kind=='ladderup' and dark:
            g=final[col][rule[1]]
            if L(rgb)-L(g)<0.02:
                for h in LADDER:
                    if L(hexrgb(h))-L(g)>=0.02:
                        why='dark-elevated ladder %s (%+.3f L)'%(h,L(hexrgb(h))-L(rgb)); rgb=hexrgb(h); break
        if name in READABLE:
            fixed,delta=textfix(rgb,final[col][READABLE[name]],4.5,dark)
            if delta is not None and abs(delta)>5e-4:
                rgb=fixed; why+='; WCAG AA %+.3f L'%delta
        final[col][name]=rgb; reason[name][col]=why
out={'rows':[],'cols':COLS}
for name,refs,rule in ROWS:
    out['rows'].append({'role':name,'src':refs['src'],'ref':{c:refs[c] for c in COLS},'final':{c:tohex(final[c][name]) for c in COLS},'L':{c:round(L(final[c][name]),3) for c in COLS},'why':reason[name]})
json.dump(out,open(sys.argv[1],'w'),indent=1)
for r in out['rows']:
    print('%-42s %-14s %-9s %-40s | %-14s %-9s %s'%(r['role'],r['ref']['light'],r['final']['light'],r['why']['light'],r['ref']['dark_elevated'],r['final']['dark_elevated'],r['why']['dark_elevated']))
