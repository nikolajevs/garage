#!/usr/bin/env python3
"""Generate KiCad 7 project for the garage alarm carrier board."""
import uuid, json, os

OUT = "/home/claude/garage/pcb"
PROJ = "garage_carrier"

def u(): return str(uuid.uuid4())

# ---------------- nets ----------------
NETS = ["", "GND", "+12V", "+5V", "+3V3", "SPI_MOSI", "SPI_MISO", "SPI_SCK",
        "RC522_SS", "RC522_RST", "RADAR_TXD", "RADAR_RXD", "RADAR_OUT",
        "SIREN_SW", "GPIO32", "GATE", "REED", "GPIO2_LED", "LED_A"]
NID = {n: i for i, n in enumerate(NETS)}

# ---------------- components ----------------
# Each: ref, value, list of (padname, pinlabel, net or None), footprint geometry
ESP_L = [("1","EN",None),("2","VP/36",None),("3","VN/39",None),("4","D34",None),
         ("5","D35",None),("6","D32","GPIO32"),("7","D33",None),("8","D25","RADAR_OUT"),
         ("9","D26",None),("10","D27","REED"),("11","D14",None),("12","D12",None),
         ("13","D13",None),("14","GND","GND"),("15","VIN(5V)","+5V")]
ESP_R = [("1","D23","SPI_MOSI"),("2","D22",None),("3","TX0",None),("4","RX0",None),
         ("5","D21",None),("6","D19","SPI_MISO"),("7","D18","SPI_SCK"),("8","D5","RC522_SS"),
         ("9","D17/TX2","RADAR_RXD"),("10","D16/RX2","RADAR_TXD"),("11","D4","RC522_RST"),
         ("12","D2","GPIO2_LED"),("13","D15",None),("14","GND","GND"),("15","3V3","+3V3")]
RC522 = [("1","SDA(SS)","RC522_SS"),("2","SCK","SPI_SCK"),("3","MOSI","SPI_MOSI"),
         ("4","MISO","SPI_MISO"),("5","IRQ",None),("6","GND","GND"),
         ("7","RST","RC522_RST"),("8","3.3V","+3V3")]
LD2410 = [("1","VCC(5V)","+5V"),("2","GND","GND"),("3","OUT","RADAR_OUT"),
          ("4","RX","RADAR_RXD"),("5","TX","RADAR_TXD")]
PWR  = [("1","+12V","+12V"),("2","GND","GND")]
SIR  = [("1","SIREN+ (12V)","+12V"),("2","SIREN-","SIREN_SW")]
REED = [("1","REED","REED"),("2","GND","GND")]
BUCK = [("1","IN+ (12V)","+12V"),("2","IN- (GND)","GND"),("3","OUT+ (5V)","+5V"),("4","OUT- (GND)","GND")]
Q1   = [("1","G","GATE"),("2","D","SIREN_SW"),("3","S","GND")]
R1   = [("1","1","GPIO32"),("2","2","GATE")]
R2   = [("1","1","GATE"),("2","2","GND")]
R3   = [("1","1","GPIO2_LED"),("2","2","LED_A")]
LED1 = [("1","A","LED_A"),("2","K","GND")]
D1   = [("1","K","+12V"),("2","A","SIREN_SW")]
C1   = [("1","+","+5V"),("2","-","GND")]

P = 2.54
def col(x, y0, n): return [(x, y0 + i*P) for i in range(n)]

# footprint: (ref, value, pins, [(x,y) abs pad centers], padsize, drill, silk box (x1,y1,x2,y2) or None)
FP = [
 ("J1A","ESP32_row_L",ESP_L, col(34,12,15),1.7,1.0,(32.5,10.5,35.5,49.1)),
 ("J1B","ESP32_row_R",ESP_R, col(59.4,12,15),1.7,1.0,(57.9,10.5,60.9,49.1)),
 ("J2","RC522_HDR",RC522, col(80,10,8),1.7,1.0,(78.5,8.5,81.5,29.3)),
 ("J3","LD2410C_HDR",LD2410, col(80,36,5),1.7,1.0,(78.5,34.5,81.5,47.7)),
 ("J4","PWR_12V",PWR, [(9,14),(9,19)],2.6,1.3,(5.5,11,12.5,22)),
 ("J5","SIREN",SIR, [(9,29),(9,34)],2.6,1.3,(5.5,26,12.5,37)),
 ("J6","REED_SW",REED, [(9,44),(9,49)],2.6,1.3,(5.5,41,12.5,52)),
 ("U1","DCDC_12V_5V",BUCK, [(16,58),(16,64),(30,58),(30,64)],1.9,1.1,(14,56,32,66)),
 ("Q1","IRLZ44N",Q1, [(40,62),(42.54,62),(45.08,62)],1.9,1.1,(38.5,59.5,46.5,64.5)),
 ("R1","100R",R1, [(48,54),(48,60)],1.7,0.9,None),
 ("R2","10k",R2, [(52,62),(52,68)],1.7,0.9,None),
 ("R3","330R",R3, [(64,58),(70,58)],1.7,0.9,None),
 ("LED1","LED_5mm",LED1, [(74,58),(76.54,58)],1.7,0.9,None),
 ("D1","1N4007",D1, [(16,40),(16,48)],1.9,1.1,None),
 ("C1","470uF",C1, [(36,58),(36,61.5)],1.7,0.9,None),
]
HOLES = [(4,4),(88,4),(4,68),(88,68)]
BW, BH = 92, 72

# ---------------- PCB ----------------
def pcb():
    s = ['(kicad_pcb (version 20221018) (generator pcbnew)',
         ' (general (thickness 1.6))',' (paper "A4")',
         ' (layers (0 "F.Cu" signal) (31 "B.Cu" signal) (34 "B.Paste" user) (35 "F.Paste" user)'
         ' (36 "B.SilkS" user "B.Silkscreen") (37 "F.SilkS" user "F.Silkscreen")'
         ' (38 "B.Mask" user) (39 "F.Mask" user) (40 "Dwgs.User" user "User.Drawings")'
         ' (41 "Cmts.User" user "User.Comments") (44 "Edge.Cuts" user)'
         ' (46 "B.CrtYd" user "B.Courtyard") (47 "F.CrtYd" user "F.Courtyard")'
         ' (48 "B.Fab" user) (49 "F.Fab" user))',
         ' (setup (pad_to_mask_clearance 0))']
    for i, n in enumerate(NETS):
        s.append(f' (net {i} "{n}")')
    for ref, val, pins, pads, psz, drl, silk in FP:
        x0, y0 = pads[0]
        f = [f' (footprint "carrier:{ref}" (layer "F.Cu") (tstamp {u()}) (at {x0} {y0}) (attr through_hole)']
        f.append(f'  (fp_text reference "{ref}" (at 0 -2.5) (layer "F.SilkS") (tstamp {u()})'
                 f' (effects (font (size 1 1) (thickness 0.15))))')
        f.append(f'  (fp_text value "{val}" (at 0 2.5) (layer "F.Fab") (tstamp {u()})'
                 f' (effects (font (size 1 1) (thickness 0.15))))')
        if silk:
            x1,y1,x2,y2 = [c for c in silk]
            rx1,ry1,rx2,ry2 = x1-x0, y1-y0, x2-x0, y2-y0
            for a,b,c,d in ((rx1,ry1,rx2,ry1),(rx2,ry1,rx2,ry2),(rx2,ry2,rx1,ry2),(rx1,ry2,rx1,ry1)):
                f.append(f'  (fp_line (start {a} {b}) (end {c} {d}) (stroke (width 0.15) (type solid)) (layer "F.SilkS") (tstamp {u()}))')
        for (pname, plabel, net), (px, py) in zip(pins, pads):
            nid = NID[net] if net else 0
            nname = net if net else ""
            shape = "rect" if pname == "1" else "circle"
            f.append(f'  (pad "{pname}" thru_hole {shape} (at {round(px-x0,3)} {round(py-y0,3)})'
                     f' (size {psz} {psz}) (drill {drl}) (layers "*.Cu" "*.Mask")'
                     f' (net {nid} "{nname}") (tstamp {u()}))')
        f.append(' )')
        s.append("\n".join(f))
    # mounting holes
    for i,(hx,hy) in enumerate(HOLES, 1):
        s.append(f' (footprint "carrier:H{i}" (layer "F.Cu") (tstamp {u()}) (at {hx} {hy}) (attr through_hole exclude_from_pos_files exclude_from_bom)\n'
                 f'  (fp_text reference "H{i}" (at 0 -4.5) (layer "F.SilkS") (tstamp {u()}) (effects (font (size 1 1) (thickness 0.15))))\n'
                 f'  (fp_text value "M3" (at 0 4.5) (layer "F.Fab") (tstamp {u()}) (effects (font (size 1 1) (thickness 0.15))))\n'
                 f'  (pad "" thru_hole circle (at 0 0) (size 6 6) (drill 3.2) (layers "*.Cu" "*.Mask") (tstamp {u()}))\n )')
    # board edge
    for a,b,c,d in ((0,0,BW,0),(BW,0,BW,BH),(BW,BH,0,BH),(0,BH,0,0)):
        s.append(f' (gr_line (start {a} {b}) (end {c} {d}) (stroke (width 0.1) (type solid)) (layer "Edge.Cuts") (tstamp {u()}))')
    # labels on silkscreen
    s.append(f' (gr_text "GARAGE ALARM v1" (at 46 3.5) (layer "F.SilkS") (tstamp {u()}) (effects (font (size 1.5 1.5) (thickness 0.25))))')
    # GND zone on B.Cu
    s.append(f' (zone (net {NID["GND"]}) (net_name "GND") (layer "B.Cu") (tstamp {u()}) (hatch edge 0.508)\n'
             f'  (connect_pads (clearance 0.5)) (min_thickness 0.254) (filled_areas_thickness no)\n'
             f'  (fill yes (thermal_gap 0.508) (thermal_bridge_width 0.508))\n'
             f'  (polygon (pts (xy 1 1) (xy {BW-1} 1) (xy {BW-1} {BH-1}) (xy 1 {BH-1}))))')
    s.append(')')
    return "\n".join(s)

# ---------------- Schematic ----------------
def sch():
    root = u()
    libsyms, insts, labels = [], [], []
    # positions of symbols on sheet (x, y in mm)
    SPOS = {"J1A":(50,40),"J1B":(50,100),"J2":(110,40),"J3":(110,80),
            "J4":(160,40),"J5":(160,60),"J6":(160,80),"U1":(160,105),
            "Q1":(210,40),"R1":(210,60),"R2":(210,75),"R3":(210,90),
            "LED1":(210,105),"D1":(210,120),"C1":(160,130)}
    for ref, val, pins, pads, psz, drl, silk in FP:
        n = len(pins)
        sym = f"carrier:{ref}_SYM"
        h = n*P
        pindefs = []
        for i,(pname,plabel,net) in enumerate(pins):
            ly = -(i*P)
            pindefs.append(f'   (pin passive line (at -15.24 {ly} 0) (length 2.54)'
                           f' (name "{plabel}" (effects (font (size 1.27 1.27))))'
                           f' (number "{pname}" (effects (font (size 1.27 1.27)))))')
        libsyms.append(
f'''  (symbol "{sym}" (pin_names (offset 1.016)) (in_bom yes) (on_board yes)
   (property "Reference" "{ref}" (at 0 2.54 0) (effects (font (size 1.27 1.27))))
   (property "Value" "{val}" (at 0 5.08 0) (effects (font (size 1.27 1.27))))
   (symbol "{ref}_SYM_0_1" (rectangle (start -12.7 2.54) (end 12.7 {round(-h+1.27,2)}) (stroke (width 0.254) (type default)) (fill (type background))))
   (symbol "{ref}_SYM_1_1"
{chr(10).join(pindefs)}
   ))''')
        X, Y = SPOS[ref]
        pinuuids = "\n".join(f'  (pin "{p[0]}" (uuid {u()}))' for p in pins)
        insts.append(
f''' (symbol (lib_id "{sym}") (at {X} {Y} 0) (unit 1) (in_bom yes) (on_board yes) (dnp no) (uuid {u()})
  (property "Reference" "{ref}" (at {X} {Y-6} 0) (effects (font (size 1.27 1.27))))
  (property "Value" "{val}" (at {X} {Y-3.5} 0) (effects (font (size 1.27 1.27))))
  (property "Footprint" "carrier:{ref}" (at {X} {Y} 0) (effects (font (size 1.27 1.27)) hide))
{pinuuids}
  (instances (project "{PROJ}" (path "/{root}" (reference "{ref}") (unit 1)))))''')
        for i,(pname,plabel,net) in enumerate(pins):
            if not net: continue
            ax, ay = X - 15.24, Y + i*P
            labels.append(f' (global_label "{net}" (shape input) (at {ax} {ay} 180)'
                          f' (effects (font (size 1.27 1.27)) (justify right)) (uuid {u()}))')
    return (f'(kicad_sch (version 20230121) (generator eeschema) (uuid {root}) (paper "A3")\n'
            ' (lib_symbols\n' + "\n".join(libsyms) + '\n )\n'
            + "\n".join(labels) + "\n" + "\n".join(insts) +
            f'\n (sheet_instances (path "/" (page "1")))\n)')

# ---------------- project ----------------
def pro():
    return json.dumps({"board":{"design_settings":{"defaults":{},"rules":{"min_clearance":0.2}}},
                       "meta":{"filename":f"{PROJ}.kicad_pro","version":1},
                       "net_settings":{"classes":[{"name":"Default","clearance":0.2,
                          "track_width":0.3,"via_diameter":0.8,"via_drill":0.4}]},
                       "schematic":{"legacy_lib_list":[]},"sheets":[["e63e39d7-6ac0-4ffd-8aa3-1841a4541b55",""]]}, indent=1)

# ---------------- SVG preview ----------------
def svg():
    k = 8  # scale
    e = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{BW*k}" height="{BH*k}" viewBox="0 0 {BW*k} {BH*k}">']
    e.append(f'<rect x="0" y="0" width="{BW*k}" height="{BH*k}" fill="#1a5c2a" stroke="#0d3317" stroke-width="4"/>')
    for hx,hy in HOLES:
        e.append(f'<circle cx="{hx*k}" cy="{hy*k}" r="{1.6*k}" fill="#c8b273" stroke="#8a7a4d"/>'
                 f'<circle cx="{hx*k}" cy="{hy*k}" r="{1.0*k}" fill="#1a5c2a"/>')
    for ref, val, pins, pads, psz, drl, silk in FP:
        if silk:
            x1,y1,x2,y2 = silk
            e.append(f'<rect x="{x1*k}" y="{y1*k}" width="{(x2-x1)*k}" height="{(y2-y1)*k}" fill="none" stroke="#eee" stroke-width="1.5"/>')
        for (pname,plabel,net),(px,py) in zip(pins,pads):
            e.append(f'<circle cx="{px*k}" cy="{py*k}" r="{psz/2*k}" fill="#c8b273" stroke="#8a7a4d"/>'
                     f'<circle cx="{px*k}" cy="{py*k}" r="{drl/2*k}" fill="#333"/>')
            if net:
                e.append(f'<text x="{(px+1.6)*k}" y="{py*k+4}" font-size="10" fill="#fff" font-family="monospace">{net}</text>')
        x0,y0 = pads[0]
        e.append(f'<text x="{x0*k}" y="{(y0-1.8)*k}" font-size="13" fill="#ffd" font-family="monospace" font-weight="bold">{ref}</text>')
    e.append(f'<text x="{34*k}" y="{5*k}" font-size="16" fill="#fff" font-family="monospace" font-weight="bold">GARAGE ALARM v1</text>')
    e.append('</svg>')
    return "\n".join(e)

os.makedirs(OUT, exist_ok=True)
open(f"{OUT}/{PROJ}.kicad_pcb","w").write(pcb())
open(f"{OUT}/{PROJ}.kicad_sch","w").write(sch())
open(f"{OUT}/{PROJ}.kicad_pro","w").write(pro())
open(f"{OUT}/preview.svg","w").write(svg())

# sanity: balanced parens
for fn in (f"{OUT}/{PROJ}.kicad_pcb", f"{OUT}/{PROJ}.kicad_sch"):
    t = open(fn).read()
    bal = t.count("(") - t.count(")")
    print(fn, "paren balance:", bal, "| size:", len(t))
print("done")
