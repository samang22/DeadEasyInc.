"""Original 48 kHz weapon foley and stylized gunshots; no external recordings.
Run with Python 3 (standard library only). Existing files at these generated paths are replaced.
"""
from pathlib import Path
import array, hashlib, json, math, random, wave
ROOT=Path(__file__).resolve().parents[2]
SR=48000
PROFILES={'Knife':(1500,.18),'Bat':(230,.27),'Axe':(820,.32),'Pistol':(150,.32),'Rifle':(105,.42),'Shotgun':(65,.6),'Sniper':(82,.85)}
manifest={'sample_rate':SR,'channels':1,'format':'PCM16','provenance':'Original procedural synthesis, without third-party samples','weapons':{}}
def clip(n):return [0.0]*int(n*SR)
def noise(x,start,duration,amp,cutoff,seed,high=False):
    rng=random.Random(seed); y=0.; a=1-math.exp(-2*math.pi*cutoff/SR)
    for i in range(int(duration*SR)):
        j=int(start*SR)+i
        if j>=len(x):break
        r=rng.uniform(-1,1);y+=a*(r-y);t=i/SR
        env=min(1,t/.001)*math.exp(-6*t/duration)
        x[j]+=amp*(r-y if high else y)*env

def tone(x,start,duration,freq,amp,drop=.0):
    phase=0.
    for i in range(int(duration*SR)):
        j=int(start*SR)+i
        if j>=len(x):break
        t=i/SR;phase+=2*math.pi*max(25,freq*(1-drop*t/duration))/SR
        x[j]+=amp*math.sin(phase)*min(1,t/.0007)*math.exp(-6*t/duration)

def write(weapon,event,folder,x,peak):
    avg=sum(x)/len(x);x=[v-avg for v in x]
    fade=min(240,len(x)//5)
    for i in range(fade):x[-1-i]*=i/fade
    first=next((i for i,v in enumerate(x) if abs(v)>.0005),0);x=x[first:]
    scale=peak/max(max(abs(v) for v in x),.001)
    data=array.array('h',(round(max(-.99,min(.99,v*scale))*32767) for v in x))
    name=f'S_{weapon}_{event}'
    rel=Path(folder)/(name+'.wav');dest=ROOT/'SourceAssets/Audio/Weapons'/rel;dest.parent.mkdir(parents=True,exist_ok=True)
    with wave.open(str(dest),'wb') as w:w.setnchannels(1);w.setsampwidth(2);w.setframerate(SR);w.writeframes(data.tobytes())
    manifest['weapons'].setdefault(weapon,{})[event]={'wav':str(dest.relative_to(ROOT)).replace('\\','/'),'asset':'/Game/DeadEasyInc/Audio/Weapons/'+str(rel.with_suffix('')).replace('\\','/'),'duration':round(len(data)/SR,4),'peak':round(max(abs(v) for v in data)/32767,3)}

def foley(weapon,event,folder,freq,seed,duration=.23,heavy=1):
    x=clip(duration)
    noise(x,0,duration,.65,1800,seed)
    for k in range(3):
        at=k*duration*.23
        noise(x,at,.035,.9,2600,seed+k+4,True)
        tone(x,at,.09,freq*(1+.31*k),.55*heavy,.12)
    write(weapon,event,folder,x,.4)

for index,(weapon,(freq,tail)) in enumerate(PROFILES.items()):
    seed=1009+index*719
    if index<3:
        x=clip(tail)
        # Rising air movement followed by a short decaying edge.
        rng=random.Random(seed);low=0.
        for i in range(len(x)):
            t=i/SR;u=t/tail;raw=rng.uniform(-1,1);low+=.12*(raw-low)
            env=math.sin(math.pi*u)**2
            x[i]=(raw-low)*env*(.4 if weapon=='Knife' else .16)+low*env*(.5 if weapon=='Bat' else .25)
        tone(x,.025,tail*.75,200 if weapon=='Bat' else freq,.08,.65)
        write(weapon,'Swing','Melee/Swing',x,.42)
        for event,kind in [('HitOrganic',0),('HitMetal',1),('HitStructure',2)]:
            x=clip(.45 if kind==1 else .29)
            noise(x,0,.16,1,850 if kind==0 else 4300,seed+30+kind)
            tone(x,0,.19,85 if kind==0 else 185,.7,.6)
            if kind==1:
                for ratio in [1,1.43,2.17,3.61]:tone(x,0,.42,freq*ratio,.32/ratio,.01)
            elif kind==0:noise(x,.012,.21,.6,2400,seed+77,True)
            else:tone(x,.005,.17,320 if weapon=='Bat' else 570,.35,.2)
            write(weapon,event,'Melee/Impact',x,.63)
    else:
        x=clip(tail)
        noise(x,0,.04,1.4,1200,seed,True)
        noise(x,.002,tail,.65,1300 if weapon!='Shotgun' else 650,seed+1)
        tone(x,0,.18 if weapon!='Sniper' else .32,freq,1,.7)
        tone(x,.001,.09,freq*3.4,.25,.5)
        for delay,amp in [(.032,.14),(.087,.055)]:noise(x,delay,tail-delay,amp,2100,seed+round(delay*1000))
        write(weapon,'Fire','Firearms/Fire',x,.76)
        foley(weapon,'ReloadStart','Firearms/Reload',freq*7,seed+100,.20)
        foley(weapon,'ReloadInsert','Firearms/Reload',freq*9,seed+200,.43 if weapon=='Shotgun' else .25)
        foley(weapon,'ReloadEnd','Firearms/Reload',freq*12,seed+300,.18 if weapon=='Pistol' else .27)
        x=clip(.12);noise(x,0,.022,.8,3800,seed+400,True);tone(x,0,.09,1200+freq*2,.8,.25)
        write(weapon,'DryFire','Firearms/DryFire',x,.34)
    foley(weapon,'Equip','Equip',freq*1.2,seed+500,.18+index*.012)
    foley(weapon,'Unequip','Equip',freq*.85,seed+600,.15+index*.01)
path=ROOT/'SourceAssets/Audio/Weapons/manifest.json';path.write_text(json.dumps(manifest,indent=2),encoding='utf-8')
rows=[v for sounds in manifest['weapons'].values() for v in sounds.values()]
assert len(rows)==46 and all(0<r['peak']<.99 and r['duration']>.05 for r in rows)
print(f'Generated {len(rows)} unique PCM16 mono WAVs at {SR} Hz; {sum(r["duration"] for r in rows):.2f} seconds total.')
