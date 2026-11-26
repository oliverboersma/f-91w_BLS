import re,math,sys
def tok(s):
    return re.findall(r'\(|\)|"(?:[^"\\]|\\.)*"|[^\s()]+',s)
def parse(s):
    t=tok(s);i=0
    def p():
        nonlocal i
        i+=1;L=[]
        while t[i]!=')':
            if t[i]=='(':L.append(p())
            else:L.append(t[i].strip('"'));i+=1
        i+=1;return L
    while t[i]!='(':i+=1
    return p()
def kids(n,name):return [c for c in n if isinstance(c,list) and c and c[0]==name]
def kid(n,name):
    k=kids(n,name);return k[0] if k else None
def load(path):
    b=parse(open(path).read())
    out={'edge':[],'dwg':[],'dwgtext':[],'fps':[]}
    for g in b:
        if not isinstance(g,list):continue
        if g[0] in('gr_line','gr_arc','gr_poly'):
            ly=kid(g,'layer')[1]
            key={'Edge.Cuts':'edge','Dwgs.User':'dwg'}.get(ly)
            if not key:continue
            if g[0]=='gr_line':
                out[key].append(('L',[tuple(map(float,kid(g,'start')[1:3])),tuple(map(float,kid(g,'end')[1:3]))]))
            elif g[0]=='gr_poly':
                out[key].append(('P',[tuple(map(float,x[1:3])) for x in kid(g,'pts')[1:]]))
            else:
                out[key].append(('A',[tuple(map(float,kid(g,k)[1:3])) for k in('start','mid','end')]))
        if g[0]=='gr_text' and kid(g,'layer')[1]=='Dwgs.User':
            out['dwgtext'].append((g[1],tuple(map(float,kid(g,'at')[1:3]))))
        if g[0]=='footprint':
            at=kid(g,'at');x,y=float(at[1]),float(at[2]);r=float(at[3]) if len(at)>3 else 0
            ref='?'
            for pr in kids(g,'property'):
                if pr[1]=='Reference':ref=pr[2]
            for ft in kids(g,'fp_text'):
                if ft[1]=='reference':ref=ft[2]
            side=kid(g,'layer')[1]
            a=math.radians(r)
            def T(px,py):return (x+px*math.cos(a)+py*math.sin(a), y-px*math.sin(a)+py*math.cos(a))
            pts=[]
            for f in kids(g,'fp_line')+kids(g,'fp_rect')+kids(g,'fp_poly'):
                ly=kid(f,'layer')
                if not ly or 'CrtYd' not in ly[1]:continue
                if f[0]=='fp_poly':pts+= [T(*map(float,q[1:3])) for q in kid(f,'pts')[1:]]
                else:
                    s=list(map(float,kid(f,'start')[1:3]));e=list(map(float,kid(f,'end')[1:3]))
                    if f[0]=='fp_rect':pts+=[T(s[0],s[1]),T(e[0],s[1]),T(e[0],e[1]),T(s[0],e[1])]
                    else:pts+=[T(*s),T(*e)]
            out['fps'].append((ref,x,y,r,side,pts))
    return out
if __name__=='__main__':
    d=load(sys.argv[1])
    xs=[p[0] for _,ps in d['edge'] for p in ps];ys=[p[1] for _,ps in d['edge'] for p in ps]
    print('edge',len(d['edge']),'bbox',min(xs),max(xs),min(ys),max(ys))
    print('dwg',[(k,[tuple(round(v,2) for v in p) for p in ps]) for k,ps in d['dwg']][:20])
    print('txt',[(t[:30],a) for t,a in d['dwgtext']])
    print([(f[0],round(f[1],2),round(f[2],2),f[4],len(f[5])) for f in d['fps']])
