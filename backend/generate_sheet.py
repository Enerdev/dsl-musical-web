#!/usr/bin/env python3
import sys
import re
from xml.sax.saxutils import escape

# Uso: generate_sheet.py [format]
# format: svg (default) | abc | musicxml

FORMAT = 'svg'
if len(sys.argv) > 1:
    FORMAT = sys.argv[1].lower()

codigo = sys.stdin.read() or ''
if not codigo.strip():
    print('', end='')
    sys.exit(0)

# Parse simple header info: COMPAS X/Y (time signature), TITULO, AUTOR
titulo = ''
autor = ''
compas = (4, 4)

for line in codigo.splitlines():
    l = line.strip()
    if l.upper().startswith('TITULO'):
        m = re.search(r'"(.*)"', l)
        if m: titulo = m.group(1)
    if l.upper().startswith('AUTOR'):
        m = re.search(r'"(.*)"', l)
        if m: autor = m.group(1)
    if l.upper().startswith('COMPAS'):
        m = re.search(r'(\d+)\/(\d+)', l)
        if m:
            compas = (int(m.group(1)), int(m.group(2)))

# Tokenizar notas por compases (|)
note_re = re.compile(r'([A-Za-zÑñ]+)\s*:\s*([A-Za-z]+)')

dur_map = {
    'REDONDA': 1.0,
    'BLANCA': 0.5,
    'NEGRA': 0.25,
    'CORCHEA': 0.125,
}

notes_measures = []
for bar in codigo.split('|'):
    meas = []
    for m in note_re.finditer(bar):
        name = m.group(1).strip().upper()
        dur = m.group(2).strip().upper()
        if dur not in dur_map:
            continue
        meas.append((name, dur, dur_map[dur]))
    if meas:
        notes_measures.append(meas)

def to_abc(titulo, autor, compas, measures):
    # Simple ABC header
    lines = []
    lines.append('X:1')
    lines.append('T:' + (titulo or 'Untitled'))
    if autor: lines.append('C:' + autor)
    lines.append('M:{}/{}'.format(compas[0], compas[1]))
    lines.append('L:1/4')
    lines.append('K:C')
    body = []
    for meas in measures:
        parts = []
        for n,d,vd in meas:
            letter = n[0].upper() if n else 'C'
            # map duration to ABC lengths (simplified)
            if d == 'REDONDA': parts.append(letter + '4')
            elif d == 'BLANCA': parts.append(letter + '2')
            elif d == 'NEGRA': parts.append(letter)
            elif d == 'CORCHEA': parts.append(letter + '/')
        body.append(' '.join(parts))
    lines.append(' | '.join(body))
    return '\n'.join(lines)

def to_musicxml(titulo, autor, compas, measures):
    # Very small MusicXML skeleton
    def note_xml(step, octave, duration_divisions, type_name):
        return ('<note><pitch><step>{}</step><octave>{}</octave></pitch>'
                '<duration>{}</duration><type>{}</type></note>').format(step, octave, duration_divisions, type_name)

    # map simple names to step+octave
    map_note = {'DO':('C',4),'RE':('D',4),'MI':('E',4),'FA':('F',4),'SOL':('G',4),'LA':('A',4),'SI':('B',4)}
    dur_type_map = {'REDONDA':('whole',4),'BLANCA':('half',2),'NEGRA':('quarter',1),'CORCHEA':('eighth',0.5)}

    header = ['<?xml version="1.0" encoding="UTF-8"?>', '<score-partwise version="3.1">', '<work><work-title>{}</work-title></work>'.format(escape(titulo or 'Untitled'))]
    header.append('<identification><creator type="composer">{}</creator></identification>'.format(escape(autor)))
    header.append('<part-list><score-part id="P1"><part-name>Music</part-name></score-part></part-list>')
    header.append('<part id="P1">')

    # simple divisions set to 4 (quarter = 1 division unit)
    divisions = 4
    for meas in measures:
        header.append('<measure>')
        # attributes on first measure
        header.append('<attributes><divisions>{}</divisions><time><beats>{}</beats><beat-type>{}</beat-type></time><clef><sign>G</sign><line>2</line></clef></attributes>'.format(divisions, compas[0], compas[1]))
        for n,d,vd in meas:
            step,octv = map_note.get(n,('C',4))
            type_name,divs = dur_type_map.get(d,('quarter',1))
            dur_val = int(divisions * (1.0 / dur_map[d])) if d in dur_map else divisions
            header.append(note_xml(step, octv, dur_val, type_name))
        header.append('</measure>')

    header.append('</part>')
    header.append('</score-partwise>')
    return '\n'.join(header)

def to_svg(titulo, autor, compas, measures):
    # Improved SVG: draw measures, spacing based on number of measures, labels
    measures_count = max(1, len(measures))
    width = max(600, measures_count * 220)
    height = 180
    margin = 20
    stave_top = 50
    line_gap = 8
    svg = []
    svg.append(f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">')
    svg.append('<style> .note { fill: black; } .note-stem { stroke: black; stroke-width: 2; } .title { font-family: serif; font-size: 16px; fill: #222; } .author { font-family: serif; font-size: 12px; fill: #333; }</style>')
    svg.append(f'<rect width="100%" height="100%" fill="#fffbe8"/>')
    if titulo:
        svg.append(f'<text x="{margin}" y="20" class="title">{escape(titulo)}</text>')
    if autor:
        svg.append(f'<text x="{margin}" y="36" class="author">{escape(autor)}</text>')

    # draw staff lines across the width
    staff_y = [stave_top + i * line_gap for i in range(5)]
    for y in staff_y:
        svg.append(f'<line x1="{margin}" y1="{y}" x2="{width-margin}" y2="{y}" stroke="#999" stroke-width="1"/>')

    # Draw measure separators and notes
    measure_w = (width - 2*margin) / measures_count
    note_x = margin + 10
    note_spacing = max(30, measure_w / 4 - 10)

    # map note names to staff position (0 = middle line)
    note_pos = {'DO':0,'RE':1,'MI':2,'FA':3,'SOL':4,'LA':5,'SI':6}

    for mi, meas in enumerate(measures):
        mx = margin + mi * measure_w
        # vertical bar line at measure start
        svg.append(f'<line x1="{mx}" y1="{staff_y[0]-6}" x2="{mx}" y2="{staff_y[-1]+6}" stroke="#666" stroke-width="1"/>')
        # draw notes inside measure
        cx = mx + 20
        for ni, (n,d,vd) in enumerate(meas):
            pos = note_pos.get(n,0)
            line0 = staff_y[2]  # middle line as reference
            cy = line0 - (pos * (line_gap/1.5))
            # draw head
            svg.append(f'<ellipse class="note" cx="{cx}" cy="{cy}" rx="8" ry="6" />')
            # stem for certain durations
            if d in ('NEGRA','CORCHEA'):
                svg.append(f'<line class="note-stem" x1="{cx+8}" y1="{cy}" x2="{cx+8}" y2="{cy-28}" />')
            # advance x
            cx += note_spacing
        # final bar line
        if mi == measures_count-1:
            xend = mx + measure_w
            svg.append(f'<line x1="{xend}" y1="{staff_y[0]-6}" x2="{xend}" y2="{staff_y[-1]+6}" stroke="#666" stroke-width="1"/>')

    svg.append('</svg>')
    return '\n'.join(svg)

if FORMAT == 'abc':
    print(to_abc(titulo, autor, compas, notes_measures))
elif FORMAT == 'musicxml':
    print(to_musicxml(titulo, autor, compas, notes_measures))
else:
    print(to_svg(titulo, autor, compas, notes_measures))
