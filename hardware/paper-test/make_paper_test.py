"""1:1 A4 paper test for the PPG flex (rev A, rev B fold, rev C top). Run: python3 make_paper_test.py"""
import sys,os,math
sys.path.insert(0,os.path.dirname(os.path.abspath(__file__)))
from parse_kicad import load
from reportlab.pdfgen import canvas
from reportlab.lib.units import mm
from reportlab.lib.pagesizes import A4,letter
LETTER=os.environ.get('PAGE','letter')=='letter'
A4=letter if LETTER else A4
P='/mnt/project-files/'
BOARDS=[('Rev A (ordered, in production)',P+'pcb-layout/flex/kicad/f91w-ppg-flex.kicad_pcb'),
        ('Rev B (14 mm fold)',sys.argv[1] if len(sys.argv)>1 else P+'pcb-layout/paper-test/revB-f91w-ppg-flex.kicad_pcb'),
        ('Rev C (head moved 2.8 mm, U2 centred)',P+'pcb-layout/flex-revC-top/kicad/f91w-ppg-flex.kicad_pcb')]
OX,OY=151.216,97.6          # case centre in flex coords (rev C README)
PIEZO_C,PIEZO_D=(0,3.0),15.0 # piezo disc, 3 mm toward 6 o'clock (estimate)
BOWL_D=23.99
HOT={'C3','C4','L1','U1'}
c=canvas.Canvas(P+'pcb-layout/paper-test/f91w-flex-paper-test-1to1'+('-letter' if LETTER else '')+'.pdf',pagesize=A4)
c.setTitle('F-91W PPG flex paper test 1:1')
W,H=A4
def xy(cx,cy,fx,fy):  # mirrored: printed face = parts side (B)
    return (cx-(fx-OX)*mm, cy-(fy-OY)*mm)
def board(label,path,cx,cy,annotate):
    d=load(path)
    if annotate:
        c.setDash(1,1.5);c.setLineWidth(0.3)
        c.setStrokeColorRGB(.55,.55,.55);c.circle(cx,cy,BOWL_D/2*mm)
        c.setStrokeColorRGB(0,.35,.9)
        px,py=xy(cx,cy,OX+PIEZO_C[0],OY+PIEZO_C[1]);c.circle(px,py,PIEZO_D/2*mm)
        c.setDash()
        for ref,x,y,r,side,pts in d['fps']:
            if not pts:continue
            q=[xy(cx,cy,*p) for p in pts];xs=[a for a,b in q];ys=[b for a,b in q]
            hot=ref in HOT;f=side.startswith('F')
            c.setLineWidth(0.25);c.setStrokeColorRGB(*((.85,0,0) if hot else (.2,.2,.2)))
            c.setFillColorRGB(*((1,.8,.8) if hot else (.88,.88,.88)))
            if f:c.setDash(0.6,0.6)
            c.rect(min(xs),min(ys),max(xs)-min(xs),max(ys)-min(ys),stroke=1,fill=0 if f else 1)
            c.setDash();c.setFillColorRGB(*((.75,0,0) if hot else (0,0,0)));c.setFont('Helvetica',2.6)
            c.drawCentredString((min(xs)+max(xs))/2,(min(ys)+max(ys))/2-0.9,ref)
        for k,ps in d['dwg']:
            q=[xy(cx,cy,*p) for p in ps]
            if k=='L':
                c.setStrokeColorRGB(.85,0,0);c.setLineWidth(0.4);c.setDash(2,1)
                c.line(*q[0],*q[1]);c.setDash()
            elif k=='P':
                c.setStrokeColorRGB(.4,.6,.4);c.setLineWidth(0.25);c.setDash(0.5,0.8)
                pth=c.beginPath();pth.moveTo(*q[0])
                for p in q[1:]:pth.lineTo(*p)
                pth.close();c.drawPath(pth);c.setDash()
        c.setFont('Helvetica',3.6);c.setFillColorRGB(.85,0,0)
        for t,(x,y) in d['dwgtext']:
            if t.startswith(('fold','step')):
                X,Y=xy(cx,cy,156.6,y);c.drawRightString(X-0.5*mm,Y-1,t.replace(', approx.','').replace(' approx',''))
    c.setStrokeColorRGB(0,0,0);c.setLineWidth(0.35)
    for k,ps in d['edge']:
        q=[xy(cx,cy,*p) for p in ps]
        if k=='L':c.line(*q[0],*q[1])
        elif k=='P':
            pth=c.beginPath();pth.moveTo(*q[0])
            for p in q[1:]:pth.lineTo(*p)
            pth.close();c.drawPath(pth)
    c.setFillColorRGB(0,0,0);c.setFont('Helvetica-Bold',6.5)
    c.drawCentredString(cx,cy+15*mm,label)
# Row 1: annotated, Row 2: plain spares to cut
cols=[36*mm,105*mm,174*mm]
for (lab,p),x in zip(BOARDS,cols):
    board(lab,p,x,H-40*mm,True)
    board(lab+' - spare',p,x,H-95*mm,False)
c.setFont('Helvetica',6);c.setFillColorRGB(0,0,0)
c.drawString(14*mm,H-14*mm,'F-91W PPG flex paper test  -  1:1, printed face = parts side (B.Cu), seen from the caseback. Blank back = plain side (F).')
# legend
ly=H-135*mm;c.setFont('Helvetica',6.5)
def leg(y,draw,txt):
    draw(14*mm,y);c.setFillColorRGB(0,0,0);c.drawString(26*mm,y-1,txt)
def solid(x,y):c.setStrokeColorRGB(0,0,0);c.setLineWidth(.35);c.line(x,y+1,x+9*mm,y+1)
def fold(x,y):c.setStrokeColorRGB(.85,0,0);c.setLineWidth(.4);c.setDash(2,1);c.line(x,y+1,x+9*mm,y+1);c.setDash()
def circ(col):
    def f(x,y):c.setStrokeColorRGB(*col);c.setLineWidth(.3);c.setDash(1,1.5);c.line(x,y+1,x+9*mm,y+1);c.setDash()
    return f
def hot(x,y):c.setFillColorRGB(1,.8,.8);c.setStrokeColorRGB(.85,0,0);c.rect(x,y-.5,9*mm,3,fill=1)
def grey(x,y):c.setFillColorRGB(.88,.88,.88);c.setStrokeColorRGB(.2,.2,.2);c.rect(x,y-.5,9*mm,3,fill=1)
def stf(x,y):c.setStrokeColorRGB(.4,.6,.4);c.setDash(.5,.8);c.rect(x,y-.5,9*mm,3);c.setDash()
for i,(fn,t) in enumerate([(solid,'Cut line (Edge.Cuts).'),
  (fold,'Fold line (rev B/C only; positions are the KiCad estimates). Fold so the blank side is inside both folds.'),
  (grey,'Part courtyard on the parts side. J1 (dashed) is on the blank side.'),
  (hot,'C3, C4, L1, U1: sit over the piezo disc (~0.55 mm room in the 3D model). Check this area first.'),
  (circ((0,.35,.9)),'Piezo disc, 15 mm, 3 mm toward 6 o\'clock (photo estimate).'),
  (circ((.55,.55,.55)),'Caseback bowl ID 23.99 mm. The head should sit inside it.'),
  (stf,'Stiffener areas (green dotted).')]):
    leg(ly-i*5*mm,fn,t)
# instructions
iy=ly-40*mm;c.setFont('Helvetica-Bold',8);c.drawString(14*mm,iy,'How to use')
c.setFont('Helvetica',6.8)
for i,t in enumerate([
 '1. Print at 100 % / "Actual size" (turn off "Fit to page"). Check both scale marks below before cutting.',
 '2. Glue the page to thin card (about 0.1-0.2 mm, close to the 0.11 mm flex) or use 160 g paper. Cut on the black lines with a craft knife.',
 '3. Rev B/C: crease the red lines. Fold 1 goes back over J1, fold 2 wraps the 6 o\'clock edge of the mainboard, then step down past the battery.',
 '4. Put J1\'s end where the FFC connector is and lay the head in the caseback bowl. The head must sit inside the bowl with the MAXM86161 at the window.',
 '5. Stack a 15 mm paper disc (or the real piezo) and check that the red parts area has room. Note anything that buckles, overlaps or doesn\'t reach.',
 '6. Rev A has no fold section and is expected to fall short (it can\'t wrap the 6 o\'clock edge). Use it as the reference for what is in production.']):
    c.drawString(14*mm,iy-(i+1)*4.2*mm,t)
# scale checks
sy=28*mm
c.setStrokeColorRGB(0,0,0);c.setLineWidth(.4);c.line(14*mm,sy,64*mm,sy)
for i in range(11):
    t=2.5 if i%5==0 else 1.2;c.line((14+5*i)*mm,sy,(14+5*i)*mm,sy+t*mm)
c.setFont('Helvetica',6.5);c.drawString(14*mm,sy-4*mm,'50 mm scale bar: measure with a ruler, it must read exactly 50 mm.')
c.rect(110*mm,12*mm,85.6*mm,53.98*mm)
c.drawCentredString(152.8*mm,38*mm,'Credit card check: 85.60 x 53.98 mm')
c.drawCentredString(152.8*mm,34*mm,'A bank card should cover this box exactly.')
c.showPage();c.save()
