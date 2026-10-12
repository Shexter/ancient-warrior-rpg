M64=(1<<64)-1; M32=(1<<32)-1
def sm(s):
    s=(s+0x9E3779B97F4A7C15)&M64; z=s
    z=((z^(z>>30))*0xBF58476D1CE4E5B9)&M64; z=((z^(z>>27))*0x94D049BB133111EB)&M64
    return s, z^(z>>31)
class Rng:
    def __init__(s,a,b): s.state=0; s.inc=((b<<1)|1)&M64; s.next(); s.state=(s.state+a)&M64; s.next()
    @staticmethod
    def stream(base,idx):
        st=base^idx; st,a=sm(st); st,b=sm(st); return Rng(a,b)
    def next(s):
        old=s.state; s.state=(old*6364136223846793005+s.inc)&M64
        x=(((old>>18)^old)>>27)&M32; r=old>>59; return ((x>>r)|(x<<((-r)&31)))&M32
    def below(s,b):
        x=s.next(); m=x*b; l=m&M32
        if l<b:
            t=((1<<32)-b)%b
            while l<t: x=s.next(); m=x*b; l=m&M32
        return m>>32
assert Rng.stream(42,0).next()==0xd11dd51f
SK={'frozen_throne':dict(mp=40,mult=1,tgt=[('stun',3),('defense_down',3)],nostun=True),
    'golden_bell':dict(mp=25,mult=1,heal=50,self=[('reflect',2)]),
    'meteor_fist':dict(mp=25,mult=2),'angel_wings':dict(mp=10,full=True),
    'supernova':dict(mp=50,mult=5,ignore=True,halve=True)}
class C:
    def __init__(s,n,hp,mp,atk,df,life): s.n=n;s.maxHp=s.hp=hp;s.maxMp=s.mp=mp;s.atk=atk;s.df=df;s.life=life;s.st=[]
    def has(s,i): return any(x[0]==i for x in s.st)
def battle(hero,en,rng,actions):
    ev=[]; turn=0; side=None
    def heal(c,a,tag):
        a=min(a,c.maxHp-c.hp)
        if a>0: c.hp+=a; ev.append(f"Healed {tag} {a} {c.hp}")
    def hit(att,tgt,at,tt,power,applydef,cause):
        d_=tgt.df//2 if tgt.has('defense_down') else tgt.df
        d=max(0,power-d_) if applydef else power
        if tgt.has('reflect') and d>0:
            r=d//2; tgt.hp=max(0,tgt.hp-(d-r)); ev.append(f"Damaged {tt} {d-r} {tgt.hp} {cause}")
            att.hp=max(0,att.hp-r); ev.append(f"Damaged {at} {r} {att.hp} Reflect"); return d-r
        tgt.hp=max(0,tgt.hp-d); ev.append(f"Damaged {tt} {d} {tgt.hp} {cause}"); return d
    def apply(c,tag,sid,t):
        c.st=[x for x in c.st if x[0]!=sid]+[[sid,t,side==tag]] if not c.has(sid) else [[sid,t,side==tag] if x[0]==sid else x for x in c.st]
        ev.append(f"StatusApplied {tag} {sid} {t}")
    def endturn(c,tag):
        keep=[]
        for x in c.st:
            if x[2]: x[2]=False; keep.append(x)
            else:
                x[1]-=1
                if x[1]==0: ev.append(f"StatusExpired {tag} {x[0]}")
                else: keep.append(x)
        c.st=keep
    ai=iter(actions)
    while True:
        a=next(ai,'attack')
        if a!='attack':
            s=SK[a]
            if hero.mp<s['mp'] or (s.get('nostun') and en.has('stun')): a='attack'
        turn+=1; side='Hero'; ev.append(f"TurnStarted Hero {turn}")
        if a=='attack':
            ev.append("Attacked Hero"); hit(hero,en,'Hero','Enemy',hero.atk,True,'Attack')
            if en.hp>0: heal(hero,hero.life,'Hero')
        else:
            s=SK[a]; hero.mp-=s['mp']; ev.append(f"SkillCast {a} {s['mp']}")
            if s.get('full'): heal(hero,hero.maxHp,'Hero')
            if s.get('halve'): en.hp//=2; ev.append(f"HpHalved Enemy {en.hp}")
            if s.get('mult'):
                dealt=hit(hero,en,'Hero','Enemy',s['mult']*hero.atk,not s.get('ignore'),'Skill')
                if s.get('heal'): heal(hero,dealt*s['heal']//100,'Hero')
            for sid,t in s.get('self',[]): apply(hero,'Hero',sid,t)
            for sid,t in s.get('tgt',[]): apply(en,'Enemy',sid,t)
        if en.hp==0: ev+= ["Defeated Enemy",f"BattleEnded Victory {turn}"]; return ev
        endturn(hero,'Hero')
        side='Enemy'
        while True:
            ev.append(f"TurnStarted Enemy {turn}")
            if en.has('stun'): ev.append("TurnSkipped Enemy stun"); endturn(en,'Enemy'); break
            roll=rng.below(10); ev.append("Attacked Enemy"); hit(en,hero,'Enemy','Hero',en.atk,True,'Attack'); extra=False
            if hero.hp==0: ev+=["Defeated Hero",f"BattleEnded Defeat {turn}"]; return ev
            if en.hp==0: ev+=["Defeated Enemy",f"BattleEnded Victory {turn}"]; return ev
            if roll<5: heal(en,en.life,'Enemy')
            else: extra=rng.below(10)<6
            endturn(en,'Enemy')
            if extra: ev.append("ExtraTurn Enemy"); continue
            break
        if turn>=1000: ev.append(f"BattleEnded Draw {turn}"); return ev
def medusa(): return C('Medusa',3000,0,250,20,10)
print("== A: starting Fighter (atk 100 def 25 life 0 hp 350 mp 50) vs Medusa, Rng::stream(1,0), always Attack")
for e in battle(C('Aria',350,50,100,25,0),medusa(),Rng.stream(1,0),[]): print(e)
print("== B: hero hp 2000 mp 200 atk 400 def 200 life 50 vs Medusa, Rng::stream(2026,0), actions: frozen_throne, golden_bell, then attack")
for e in battle(C('Aria',2000,200,400,200,50),medusa(),Rng.stream(2026,0),['frozen_throne','golden_bell']): print(e)
print("== C: same hero as B, Rng::stream(2026,0), actions: golden_bell, attack, attack (first 26 events)")
for e in battle(C('Aria',2000,200,400,200,50),medusa(),Rng.stream(2026,0),['golden_bell'])[:26]: print(e)
print("== D: hero hp 900 mp 100 atk 300 def 100 life 0 vs Medusa, Rng::stream(99,0), actions: supernova, meteor_fist (first 14 events)")
for e in battle(C('Aria',900,100,300,100,0),medusa(),Rng.stream(99,0),['supernova','meteor_fist'])[:14]: print(e)
