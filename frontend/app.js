// Moved and refactored JS from inline <script> in index.html
// Endpoint público del backend en Render — actualizar si cambia
const BACKEND_URL = 'https://dsl-musical-web.onrender.com/compilar';
const SHEET_URL = 'https://dsl-musical-web.onrender.com/sheet';

const editor = document.getElementById('editor');
const highlightLayer = document.getElementById('highlightLayer');
const estado = document.getElementById('estado');
const btn = document.getElementById('btnCompilar');
const btnForce = document.getElementById('btnForceCompile');
const btnAst = document.getElementById('btnAst');
const positionIndicator = document.getElementById('positionIndicator');
const btnPlayAudio = document.getElementById('btnPlayAudio');
const btnStopAudio = document.getElementById('btnStopAudio');
const instrumentSelect = document.getElementById('instrumentSelect');
const tempoRange = document.getElementById('tempoRange');
const tempoValue = document.getElementById('tempoValue');
const soundModeSelect = document.getElementById('soundMode');
const humanizeChk = document.getElementById('humanizeChk');
const chorusChk = document.getElementById('chorusChk');
const chorusAmount = document.getElementById('chorusAmount');
const swingChk = document.getElementById('swingChk');

if (btnForce) btnForce.style.display = 'inline-flex';
if (btnAst) btnAst.style.display = 'none';
if (btnPlayAudio) btnPlayAudio.disabled = true;
if (instrumentSelect) instrumentSelect.value = 'PIANO';

const errorsList = document.getElementById('errorsList');
const errorSummary = document.getElementById('errorSummary');
const errorBadges = document.getElementById('errorBadges');
const sheet = document.getElementById('sheet');
const astView = document.getElementById('astView');

let arbolActual = null;
let mostrandoAst = false;
let lastParsedMusic = [];
let audioContext = null;
let reverbNode = null;
let noiseBuffer = null;
let activeAudioSources = [];
let activeAudioNodes = [];
// Samples support
const SAMPLE_FILES = {
  'PIANO': 'audio/piano_A4.mp3',
  'VIOLIN': 'audio/violin_A4.mp3'
};
const SAMPLE_BASE_FREQ = 440.0; // A4
const samples = {}; // instrument -> AudioBuffer

async function loadInstrumentSample(instrument) {
  if (!SAMPLE_FILES[instrument]) return false;
  if (samples[instrument]) return true;
  // Try loading external file first
  try {
    const url = SAMPLE_FILES[instrument];
    const res = await fetch(url);
    if (res.ok) {
      const array = await res.arrayBuffer();
      if (!audioContext) audioContext = new (window.AudioContext || window.webkitAudioContext)();
      const buf = await audioContext.decodeAudioData(array);
      samples[instrument] = buf;
      console.log('[samples] loaded', instrument, url);
      return true;
    }
  } catch (e) {
    console.warn('[samples] failed to fetch', instrument, e);
  }

  // Fallback: synthesize a short sample using OfflineAudioContext (A4 base)
  try {
    const sampleDur = 2.4;
    const sr = 44100;
    const off = new OfflineAudioContext(1, Math.floor(sr * sampleDur), sr);

    if (instrument === 'PIANO') {
      const o = off.createOscillator(); o.type = 'triangle'; o.frequency.value = SAMPLE_BASE_FREQ;
      const o2 = off.createOscillator(); o2.type = 'sine'; o2.frequency.value = SAMPLE_BASE_FREQ * 2;
      const g = off.createGain();
      g.gain.setValueAtTime(0.0001, 0);
      g.gain.linearRampToValueAtTime(1.0, 0.004);
      g.gain.exponentialRampToValueAtTime(0.0001, sampleDur);

      // percussive noise
      const nbuf = off.createBuffer(1, Math.floor(sr * 0.12), sr);
      const nd = nbuf.getChannelData(0);
      for (let i = 0; i < nd.length; i++) nd[i] = (Math.random() * 2 - 1) * Math.pow(1 - i / nd.length, 2);
      const ns = off.createBufferSource(); ns.buffer = nbuf;
      const ng = off.createGain(); ng.gain.setValueAtTime(0.18, 0); ng.gain.exponentialRampToValueAtTime(0.001, 0.08);

      const filt = off.createBiquadFilter(); filt.type = 'lowpass'; filt.frequency.value = 3800;

      o.connect(g); o2.connect(g); ns.connect(ng); ng.connect(g);
      g.connect(filt); filt.connect(off.destination);

      o.start(0); o2.start(0); ns.start(0);
      o.stop(sampleDur); o2.stop(sampleDur); ns.stop(sampleDur);
    } else if (instrument === 'VIOLIN') {
      const o = off.createOscillator(); o.type = 'sawtooth'; o.frequency.value = SAMPLE_BASE_FREQ;
      const g = off.createGain(); g.gain.setValueAtTime(0.0001, 0); g.gain.linearRampToValueAtTime(0.9, 0.06); g.gain.exponentialRampToValueAtTime(0.0001, sampleDur);
      const filt = off.createBiquadFilter(); filt.type = 'lowpass'; filt.frequency.value = 2400; filt.Q.value = 0.8;

      // vibrato LFO
      const lfo = off.createOscillator(); lfo.frequency.value = 5.5; const lfoGain = off.createGain(); lfoGain.gain.value = 8; lfo.connect(lfoGain); lfoGain.connect(o.detune);

      o.connect(filt); filt.connect(g); g.connect(off.destination);
      lfo.start(0); o.start(0); o.stop(sampleDur); lfo.stop(sampleDur);
    }

    const rendered = await off.startRendering();
    samples[instrument] = rendered;
    console.log('[samples] synthesized', instrument);
    return true;
  } catch (e) {
    console.warn('[samples] synth failed', instrument, e);
    return false;
  }
}

if (tempoRange && tempoValue) {
  tempoRange.addEventListener('input', () => { tempoValue.textContent = tempoRange.value; });
}
if (chorusAmount) chorusAmount.addEventListener('input', () => { /* UI-only slider */ });

function stopAudioPlayback() {
  if (activeAudioSources.length) {
    for (const src of activeAudioSources) {
      try { src.stop(); } catch (e) { }
      try { src.disconnect(); } catch (e) { }
    }
    activeAudioSources = [];
  }
  if (activeAudioNodes.length) {
    for (const node of activeAudioNodes) {
      try { node.disconnect(); } catch (e) { }
    }
    activeAudioNodes = [];
  }
}

if (btnStopAudio) {
  btnStopAudio.addEventListener('click', () => {
    stopAudioPlayback();
  });
}

// ---------- Resaltado de lineas con error ----------
function extraerLineasError(output) {
  const lineas = new Set();
  const reBracket = /\[ERROR (?:SINT|SEMANTICO) L(\d+)\]/g;
  let m;
  while ((m = reBracket.exec(output)) !== null) lineas.add(parseInt(m[1], 10));
  const reLex = /^\s*(\d+)\s+\S+\s+\S+.*ERROR LEXICO/gm;
  while ((m = reLex.exec(output)) !== null) lineas.add(parseInt(m[1], 10));
  return [...lineas].sort((a, b) => a - b);
}

function actualizarResaltado(lineas) {
  highlightLayer.innerHTML = '';
  if (!lineas.length) return;
  const cs = getComputedStyle(editor);
  const lineHeight = parseFloat(cs.lineHeight) || 22.4;
  const paddingTop = parseFloat(cs.paddingTop) || 20;
  for (const n of lineas) {
    const div = document.createElement('div');
    div.className = 'error-line';
    div.style.top = (paddingTop + (n - 1) * lineHeight) + 'px';
    div.style.height = lineHeight + 'px';
    highlightLayer.appendChild(div);
  }
  sincronizarScroll();
  const cs2 = getComputedStyle(editor);
  const lh = parseFloat(cs2.lineHeight) || 22.4;
  const pt = parseFloat(cs2.paddingTop) || 20;
  editor.scrollTop = Math.max(0, pt + (lineas[0] - 1) * lh - editor.clientHeight / 2);
}

function sincronizarScroll() {
  highlightLayer.style.transform = 'translateY(' + (-editor.scrollTop) + 'px)';
}
editor.addEventListener('scroll', sincronizarScroll);

function updateCursorPosition() {
  if (!positionIndicator || !editor) return;
  const cursorIndex = editor.selectionStart;
  const beforeCursor = editor.value.slice(0, cursorIndex);
  const line = beforeCursor.split('\n').length;
  const lastNewline = beforeCursor.lastIndexOf('\n');
  const column = cursorIndex - lastNewline;
  positionIndicator.textContent = `L${line}:C${column}`;
}

editor.addEventListener('input', updateCursorPosition);
editor.addEventListener('click', updateCursorPosition);
editor.addEventListener('keyup', updateCursorPosition);
editor.addEventListener('select', updateCursorPosition);
updateCursorPosition();

// ---------- Arbol AST ----------
function parsearAST(output) {
  const marcador = "AST generado:";
  const idx = output.indexOf(marcador);
  if (idx === -1) return null;
  let resto = output.slice(idx + marcador.length);
  const finIdx = resto.indexOf("==============================================================================");
  if (finIdx !== -1) resto = resto.slice(0, finIdx);
  const lineasRaw = resto.split("\n").filter(l => l.trim().length > 0);
  const raiz = { texto: "Programa", hijos: [], nivel: -1 };
  const pila = [raiz];
  for (const linea of lineasRaw) {
    const espacios = (linea.match(/^ */) || [''])[0].length;
    const nivel = Math.floor(espacios / 2);
    const texto = linea.trim();
    if (texto === "Programa") continue;
    const nodo = { texto, hijos: [], nivel };
    while (pila.length > 1 && pila[pila.length - 1].nivel >= nivel) pila.pop();
    pila[pila.length - 1].hijos.push(nodo);
    pila.push(nodo);
  }
  return raiz;
}

function claseTipo(texto) {
  const w = texto.split(' ')[0];
  const map = { Nota: 'ast-nota', Bloque: 'ast-bloque', Declaracion: 'ast-decl', Asignacion: 'ast-asig', Mostrar: 'ast-io', Ingresar: 'ast-io' };
  return map[w] || '';
}

function crearNodoDOM(nodo) {
  if (nodo.hijos.length === 0) {
    const div = document.createElement('div');
    div.className = 'ast-leaf ' + claseTipo(nodo.texto);
    div.textContent = nodo.texto;
    return div;
  }
  const det = document.createElement('details');
  det.open = true;
  const sum = document.createElement('summary');
  sum.className = claseTipo(nodo.texto);
  sum.textContent = nodo.texto;
  det.appendChild(sum);
  const wrap = document.createElement('div');
  wrap.className = 'ast-children';
  for (const h of nodo.hijos) wrap.appendChild(crearNodoDOM(h));
  det.appendChild(wrap);
  return det;
}

function renderAst(arbol) {
  astView.innerHTML = '';
  if (!arbol || arbol.hijos.length === 0) {
    astView.innerHTML = '<em>(AST vacio)</em>';
    return;
  }
  astView.appendChild(crearNodoDOM(arbol));
}

if (btnAst) btnAst.addEventListener('click', () => {
  mostrandoAst = !mostrandoAst;
  errorsList.style.display = mostrandoAst ? 'none' : 'block';
  astView.style.display = mostrandoAst ? 'block' : 'none';
  btnAst.textContent = mostrandoAst ? 'Ver errores' : 'Ver arbol AST';
});

if (btnForce) btnForce.addEventListener('click', () => compilar(true));
if (btn) btn.addEventListener('click', () => compilar(false));

// ---------- Compilar ----------
async function compilar(force = false) {
  const codigo = editor.value;
  if (!codigo.trim()) {
    errorsList.innerHTML = "<em>No hay codigo para compilar.</em>";
    updateAudioPlaybackState([]);
    return;
  }

  updateAudioPlaybackState(parseMusicCode(codigo));

  btn.disabled = true;
  estado.textContent = "Compilando...";
  estado.className = "status";
  errorsList.innerHTML = "<em>Compilando...</em>";
  actualizarResaltado([]);
  btnAst.style.display = 'none';
  mostrandoAst = false;
  astView.style.display = 'none';
  btnAst.textContent = 'Ver arbol AST';

  const avisoLento = setTimeout(() => {
    errorsList.innerHTML = "<em>Compilando... (El servidor puede tardar 30-50s si estaba dormido.)</em>";
  }, 6000);

  try {
    const res = await fetch(BACKEND_URL, {
      method: "POST",
      headers: { "Content-Type": "text/plain" },
      body: codigo
    });
    clearTimeout(avisoLento);

    console.groupCollapsed('[DSL Musical] compilar()');
    console.log('force:', force);
    console.log('BACKEND_URL:', BACKEND_URL);
    console.log('HTTP status:', res.status);
    console.log('response headers:', Array.from(res.headers.entries()));

    if (!res.ok) throw new Error("El servidor respondio con estado " + res.status);

    const data = await res.json();
    const salida = data.output || "(sin salida)";
    console.log('backend output:', salida);

    // heurísticas del cliente
    const lexIssues = detectLexicalIssues(codigo);
    const semIssues = detectSemanticIssues(codigo);
    console.log('lexIssues:', lexIssues);
    console.log('semIssues:', semIssues);

    // errores reportados por el backend
    const backendErrors = extraerErroresDetalle(salida);
    let showClientIssues = lexIssues.length || semIssues.length;

    if (backendErrors.length) {
      actualizarResaltado(extraerLineasError(salida));
      showErrorsList(backendErrors);
      estado.textContent = 'Errores';
      estado.className = 'status err';
    } else if (showClientIssues && !force) {
      const merged = [];
      for (const li of lexIssues) merged.push({ type: 'Lexico', line: li.line, message: li.message });
      for (const si of semIssues) merged.push({ type: 'Semantico', line: si.line, message: si.message });
      showErrorsList(merged);
      actualizarResaltado(merged.map(x => x.line));
      estado.textContent = 'Errores';
      estado.className = 'status err';
    } else {
      if (showClientIssues && force) {
        const merged = [];
        for (const li of lexIssues) merged.push({ type: 'Lexico', line: li.line, message: li.message });
        for (const si of semIssues) merged.push({ type: 'Semantico', line: si.line, message: si.message });
        showErrorsList(merged);
        actualizarResaltado(merged.map(x => x.line));
        estado.textContent = 'Forzado con avisos';
        estado.className = 'status err';
      } else {
        actualizarResaltado([]);
        showErrorsList([]);
        estado.textContent = 'Listo';
        estado.className = 'status ok';
      }
    }

    // AST
    arbolActual = parsearAST(salida);
    if (arbolActual && arbolActual.hijos.length > 0) {
      renderAst(arbolActual);
      btnAst.style.display = 'inline-block';
    }

    // Intentar pedir al backend una partitura SVG generada por Python, salvo errores léxicos sin forzar
    if (lexIssues.length && !force) {
      await renderMusicFromCode(codigo);
    } else {
      try {
        const resSheet = await fetch(SHEET_URL, {
          method: 'POST', headers: { 'Content-Type': 'text/plain' }, body: codigo
        });
        if (resSheet.ok) {
          const svg = await resSheet.text();
          sheet.innerHTML = svg;
        } else {
          await renderMusicFromCode(codigo);
        }
      } catch (e) {
        await renderMusicFromCode(codigo);
      }
    }
  } catch (err) {
    clearTimeout(avisoLento);
    console.error('[DSL Musical] compilar() error:', err);
    errorsList.innerHTML = "<pre>No se pudo conectar con el backend.\n\n" + err.message +
      "\n\nVerifica que BACKEND_URL en este archivo apunte a tu servicio de Render.</pre>";
    estado.textContent = "Error";
    estado.className = "status err";
  } finally {
    btn.disabled = false;
    btnForce.disabled = false;
    console.groupEnd();
  }
}

// Descargar PDF desde el backend
const btnDownloadPDF = document.getElementById('btnDownloadPDF');
if (btnDownloadPDF) btnDownloadPDF.addEventListener('click', async () => {
  const codigo = editor.value;
  if (!codigo.trim()) return alert('No hay codigo para generar PDF.');
  btnDownloadPDF.disabled = true;
  btnDownloadPDF.textContent = 'Generando...';
  try {
    const res = await fetch(SHEET_URL + '?format=pdf', { method: 'POST', headers: { 'Content-Type': 'text/plain' }, body: codigo });
    if (!res.ok) throw new Error('Servidor respondio ' + res.status);
    const blob = await res.blob();
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = (document.querySelector('#sheetTitle')?.textContent || 'score') + '.pdf';
    document.body.appendChild(a);
    a.click();
    a.remove();
    URL.revokeObjectURL(url);
  } catch (err) {
    alert('Error generando PDF: ' + err.message);
  } finally {
    btnDownloadPDF.disabled = false;
    btnDownloadPDF.textContent = 'Descargar PDF';
  }
});

// Descargar MusicXML desde el backend
const btnDownload = document.getElementById('btnDownloadMusicXML');
if (btnDownload) btnDownload.addEventListener('click', async () => {
  const codigo = editor.value;
  if (!codigo.trim()) return alert('No hay codigo para generar MusicXML.');
  btnDownload.disabled = true;
  btnDownload.textContent = 'Generando...';
  try {
    const res = await fetch(SHEET_URL + '?format=musicxml', { method: 'POST', headers: { 'Content-Type': 'text/plain' }, body: codigo });
    if (!res.ok) throw new Error('Servidor respondio ' + res.status);
    const text = await res.text();
    const blob = new Blob([text], { type: 'application/xml' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = (document.querySelector('#sheetTitle')?.textContent || 'score') + '.musicxml';
    document.body.appendChild(a);
    a.click();
    a.remove();
    URL.revokeObjectURL(url);
  } catch (err) {
    alert('Error generando MusicXML: ' + err.message);
  } finally {
    btnDownload.disabled = false;
    btnDownload.textContent = 'Descargar MusicXML';
  }
});

// Extrae errores con tipo, linea y mensaje del output del backend
function extraerErroresDetalle(output) {
  const errores = [];
  let m;
  const reBracket = /\[ERROR\s+(SINT|SEMANTICO)\s+L(\d+)\]\s*(.*)/g;
  while ((m = reBracket.exec(output)) !== null) {
    errores.push({ type: m[1] === 'SINT' ? 'Sintactico' : 'Semantico', line: parseInt(m[2], 10), message: m[3].trim() });
  }
  const reLex = /^\s*(\d+)\s+.*ERROR LEXICO.*$/gm;
  while ((m = reLex.exec(output)) !== null) {
    errores.push({ type: 'Lexico', line: parseInt(m[1], 10), message: (m[0] || '').trim() });
  }
  return errores;
}

// Heurística para detectar errores léxicos en el código fuente
// - cadenas sin cerrar (comillas dobles)
// - caracteres no imprimibles o tokens sospechosos (puede ampliarse)
function detectLexicalIssues(code) {
  const issues = [];
  const lines = code.split('\n');
  for (let i = 0; i < lines.length; i++) {
    const line = lines[i];
    // detectar comillas dobles sin cerrar en la misma o en líneas posteriores
    const quotes = (line.match(/\"/g) || []).length;
    // cuenta total de comillas hasta esta línea para detectar apertura sin cierre
    const totalQuotesUpToLine = lines.slice(0, i + 1).join('\n').match(/\"/g);
    const total = totalQuotesUpToLine ? totalQuotesUpToLine.length : 0;
    if (total % 2 === 1) {
      // si hay una comilla abierta que no se cierra dentro de las primeras N líneas,
      // marcar la línea donde aparece la apertura (mejor heurística: buscar la primera '"')
      const idx = (line.indexOf('\"') !== -1) ? line.indexOf('\"') : 0;
      issues.push({ type: 'Lexico', line: i + 1, message: 'Cadena entrecomillada sin cerrar (falta ")' });
    }

    // detectar caracteres no imprimibles raros (ej. carácter de control)
    if (/[\x00-\x08\x0B\x0C\x0E-\x1F]/.test(line)) {
      issues.push({ type: 'Lexico', line: i + 1, message: 'Caracter no imprimible o inválido en la linea' });
    }
  }
  // eliminar duplicados por linea
  const uniq = [];
  const seen = new Set();
  for (const it of issues) {
    const key = `${it.type}|${it.line}|${it.message}`;
    if (!seen.has(key)) { seen.add(key); uniq.push(it); }
  }
  return uniq;
}

// Heurística semántica ligera en cliente
// - valida la directiva COMPAS: formato NUM/NUM y denominador potencia de 2 (2,4,8,16)
function detectSemanticIssues(code) {
  const issues = [];
  const lines = code.split('\n');
  for (let i = 0; i < lines.length; i++) {
    const line = lines[i].trim();
    // buscar COMPAS x/y (case-insensitive)
    const m = line.match(/^COMPAS\s+(\d+)\s*\/\s*(\d+)$/i);
    if (m) {
      const num = parseInt(m[1], 10);
      const den = parseInt(m[2], 10);
      // denominador válido: potencia de 2 (2,4,8,16,...). Limitamos a comunes: 2,4,8,16
      const valid = [2,4,8,16].includes(den);
      if (!valid) {
        issues.push({ type: 'Semantico', line: i + 1, message: `Compás ${num}/${den} inválido: el denominador debe ser 2, 4, 8 o 16` });
      }
    }
  }
  return issues;
}

function badgeForType(t) {
  const map = { 'Lexico': 'background:var(--err); color:#fff; padding:4px 8px; border-radius:12px; font-family:Courier New,monospace; font-size:12px;', 'Sintactico': 'background:#f39c12; color:#fff; padding:4px 8px; border-radius:12px; font-family:Courier New,monospace; font-size:12px;', 'Semantico': 'background:#8e44ad; color:#fff; padding:4px 8px; border-radius:12px; font-family:Courier New,monospace; font-size:12px;', 'Otro': 'background:#666; color:#fff; padding:4px 8px; border-radius:12px; font-family:Courier New,monospace; font-size:12px;' };
  return map[t] || map['Otro'];
}

function showErrorsList(errores) {
  errorBadges.innerHTML = '';
  if (!errores || errores.length === 0) {
    errorSummary.textContent = 'Sin errores';
    errorsList.innerHTML = '<em>Sin errores. La partitura se ha generado a la derecha.</em>';
    return;
  }
  // Resumen
  const counts = errores.reduce((acc, e) => { acc[e.type] = (acc[e.type] || 0) + 1; return acc; }, {});
  const parts = Object.entries(counts).map(([k, v]) => `${k}: ${v}`);
  errorSummary.textContent = parts.join(' · ');

  // Badges
  for (const t of Object.keys(counts)) {
    const span = document.createElement('span');
    span.setAttribute('style', badgeForType(t));
    span.textContent = `${t} (${counts[t]})`;
    errorBadges.appendChild(span);
  }

  // Lista detallada
  const ul = document.createElement('div');
  ul.style.display = 'flex';
  ul.style.flexDirection = 'column';
  ul.style.gap = '8px';
  for (const e of errores) {
    const row = document.createElement('div');
    row.style.padding = '8px';
    row.style.borderRadius = '6px';
    row.style.background = 'rgba(255,255,255,0.03)';
    row.style.display = 'flex';
    row.style.alignItems = 'center';
    const b = document.createElement('span');
    b.setAttribute('style', badgeForType(e.type));
    b.textContent = e.type;
    b.style.marginRight = '12px';
    const txt = document.createElement('div');
    txt.innerHTML = `<strong>${e.line ? 'L' + e.line : ''}</strong> ${e.message}`;
    row.appendChild(b);
    row.appendChild(txt);
    row.style.cursor = e.line ? 'pointer' : 'default';
    if (e.line) row.addEventListener('click', () => {
      const cs = getComputedStyle(editor);
      const lh = parseFloat(cs.lineHeight) || 22.4;
      const pt = parseFloat(cs.paddingTop) || 20;
      editor.scrollTop = Math.max(0, pt + (e.line - 1) * lh - editor.clientHeight / 2);
      editor.focus();
    });
    ul.appendChild(row);
  }
  errorsList.innerHTML = '';
  errorsList.appendChild(ul);
}

// Helper: carga un script y retorna una Promise
function loadScript(url) {
  return new Promise((resolve, reject) => {
    const s = document.createElement('script');
    s.src = url;
    s.async = true;
    s.onload = () => resolve();
    s.onerror = (e) => reject(new Error('No se pudo cargar ' + url));
    document.head.appendChild(s);
  });
}

// Render musical notes en cliente (mejorado): genera SVG directamente sin VexFlow
function parseMusicCode(codigo) {
  const note_re = /([A-GA-ZÑñ]+)([#b♯♭])?\s*:\s*([A-Za-z]+)/gi;
  const dur_val = { 'REDONDA': 4, 'BLANCA': 2, 'NEGRA': 1, 'CORCHEA': 0.5 };
  const measures = codigo.split('|').map(p => p.trim()).filter(p => p.length > 0);
  const parsed = [];
  for (const bar of measures) {
    const arr = [];
    let m;
    while ((m = note_re.exec(bar)) !== null) {
      const raw = m[1].toUpperCase();
      const acc = m[2] || '';
      const dur = (m[3] || '').toUpperCase();
      const beats = dur_val[dur];
      if (!beats) continue;
      arr.push({ name: raw, accidental: acc, dur, beats });
    }
    parsed.push(arr);
  }
  return parsed;
}

function updateAudioPlaybackState(parsed) {
  lastParsedMusic = parsed || [];
  if (btnPlayAudio) btnPlayAudio.disabled = lastParsedMusic.length === 0;
}

function renderMusicFromCode(codigo) {
  sheet.innerHTML = '';
  try {
    const parsed = parseMusicCode(codigo);
    updateAudioPlaybackState(parsed);

    if (parsed.length === 0) {
      sheet.innerHTML = '<em>No se encontraron notas reconocibles para generar partitura.</em>';
      return;
    }

    const measures_count = Math.max(1, parsed.length);
    const width = Math.max(600, measures_count * 240);
    const height = 220;
    const margin = 24;
    const stave_top = 70;
    const line_gap = 10;
    const staff_y = [stave_top, stave_top + line_gap, stave_top + 2*line_gap, stave_top + 3*line_gap, stave_top + 4*line_gap];

    let svg = '';
    svg += `<svg xmlns="http://www.w3.org/2000/svg" width="${width}" height="${height}" viewBox="0 0 ${width} ${height}">`;
    svg += '<style>.note{fill:#000}.note-hollow{fill:#fff;stroke:#000}.note-stem{stroke:#000;stroke-width:2}.flag{stroke:#000;stroke-width:2;fill:none}.title{font-family:serif;font-size:16px;fill:#222}.meta{font-family:serif;font-size:12px;fill:#333}</style>';
    svg += `<rect width="100%" height="100%" fill="#fffdf5"/>`;
    const titleText = (document.querySelector('#sheetTitle')?.textContent || 'Partitura');
    svg += `<text x="${margin}" y="30" class="title">${titleText}</text>`;

    // draw staff lines
    for (const y of staff_y) svg += `<line x1="${margin}" y1="${y}" x2="${width-margin}" y2="${y}" stroke="#666" stroke-width="1"/>`;

    const measure_w = (width - 2*margin) / measures_count;

    // helper: map simple name to offset from middle line (middle line = 0)
    const base_map = { 'DO': -3, 'RE': -2, 'MI': -1, 'FA': 0, 'SOL': 1, 'LA': 2, 'SI': 3 };

    for (let mi=0; mi<parsed.length; mi++) {
      const meas = parsed[mi];
      const mx = margin + mi * measure_w;
      // measure vertical line
      svg += `<line x1="${mx}" y1="${staff_y[0]-12}" x2="${mx}" y2="${staff_y[4]+12}" stroke="#444" stroke-width="1"/>`;

      // calculate total beats to distribute spacing
      const total = meas.reduce((s,n) => s + (dur_val[n.dur] || 1), 0) || 1;
      const unit = (measure_w - 40) / Math.max(total,1);
      let cx = mx + 20;
      for (const note of meas) {
        const beats = dur_val[note.dur];
        const cx_note = cx + (unit * beats)/2;
        // position: middle line is staff_y[2]
        const y0 = staff_y[2];
        const pos = base_map[note.name] !== undefined ? base_map[note.name] : 0;
        const cy = y0 - pos * (line_gap/1.6);

        // head: hollow for blanca/redonda, filled for negra/corchea
        const isFilled = (note.dur === 'NEGRA' || note.dur === 'CORCHEA');
        if (isFilled) svg += `<ellipse class="note" cx="${cx_note}" cy="${cy}" rx="8" ry="6"/>`;
        else svg += `<ellipse class="note-hollow" cx="${cx_note}" cy="${cy}" rx="8" ry="6"/>`;

        // stem for filled or half notes
        if (note.dur === 'NEGRA' || note.dur === 'CORCHEA' || note.dur === 'BLANCA') {
          svg += `<line class="note-stem" x1="${cx_note+8}" y1="${cy}" x2="${cx_note+8}" y2="${cy-36}"/>`;
        }
        // flag for corchea
        if (note.dur === 'CORCHEA') {
          const fx = cx_note+8; const fy = cy-36;
          svg += `<path class="flag" d="M ${fx} ${fy} q 8 6 14 2"/>`;
        }

        // ledger lines if note outside staff
        const topY = staff_y[0]; const bottomY = staff_y[4];
        if (cy < topY - 4) {
          for (let ly=cy; ly < topY; ly += (line_gap)) {
            svg += `<line x1="${cx_note-12}" y1="${ly}" x2="${cx_note+12}" y2="${ly}" stroke="#000" stroke-width="1"/>`;
          }
        } else if (cy > bottomY + 4) {
          for (let ly=cy; ly > bottomY; ly -= (line_gap)) {
            svg += `<line x1="${cx_note-12}" y1="${ly}" x2="${cx_note+12}" y2="${ly}" stroke="#000" stroke-width="1"/>`;
          }
        }

        // label (small)
        svg += `<text x="${cx_note-12}" y="${cy+30}" font-family="Courier New, monospace" font-size="11" fill="#333">${note.name}:${note.dur}</text>`;

        cx += unit * beats;
      }

      // final bar line at measure end
      const xend = mx + measure_w;
      svg += `<line x1="${xend}" y1="${staff_y[0]-12}" x2="${xend}" y2="${staff_y[4]+12}" stroke="#444" stroke-width="1"/>`;
    }

    svg += `</svg>`;
    sheet.innerHTML = svg;
  } catch (e) {
    sheet.innerHTML = '<pre>Error al generar partitura local: ' + (e && e.message ? e.message : e) + '</pre>';
  }
}

function noteFrequency(name, accidental) {
  const rootFreq = {
    'DO': 261.63,
    'RE': 293.66,
    'MI': 329.63,
    'FA': 349.23,
    'SOL': 392.00,
    'LA': 440.00,
    'SI': 493.88
  };
  const base = rootFreq[name];
  if (!base) return null;
  const semitoneRatio = Math.pow(2, 1 / 12);
  if (accidental === '#' || accidental === '♯') return base * semitoneRatio;
  if (accidental === 'b' || accidental === '♭') return base / semitoneRatio;
  return base;
}

function applyADSREnvelope(gainNode, startTime, duration, attack = 0.02, decay = 0.08, sustainLevel = 0.75, release = 0.1) {
  const endTime = startTime + duration;
  if (duration <= 0) return;

  let a = attack;
  let d = decay;
  let r = release;
  const minEnvelopeTime = a + d + r;
  if (minEnvelopeTime > duration) {
    const scale = duration / (minEnvelopeTime + 1e-9);
    a *= scale;
    d *= scale;
    r *= scale;
  }

  const attackEnd = startTime + a;
  const decayEnd = attackEnd + d;
  const releaseStart = Math.max(decayEnd, endTime - r);

  gainNode.gain.setValueAtTime(0.0001, startTime);
  gainNode.gain.linearRampToValueAtTime(1.0, attackEnd);
  gainNode.gain.exponentialRampToValueAtTime(Math.max(0.0001, sustainLevel), decayEnd);
  gainNode.gain.setValueAtTime(Math.max(0.0001, sustainLevel), releaseStart);
  gainNode.gain.exponentialRampToValueAtTime(0.0001, endTime);
}

async function playParsedMusic(parsed) {
  if (!window.AudioContext && !window.webkitAudioContext) return alert('Audio no soportado en este navegador.');
  if (!parsed || parsed.length === 0) return;

  stopAudioPlayback();
  if (!audioContext) audioContext = new (window.AudioContext || window.webkitAudioContext)();
  if (audioContext.state === 'suspended') await audioContext.resume();

  // lazy resources
  if (!noiseBuffer) {
    const buf = audioContext.createBuffer(1, audioContext.sampleRate * 0.2, audioContext.sampleRate);
    const data = buf.getChannelData(0);
    for (let i = 0; i < data.length; i++) data[i] = (Math.random() * 2 - 1) * Math.pow(1 - i / data.length, 2);
    noiseBuffer = buf;
  }

  function createReverbBuffer(sec = 2, decay = 2) {
    const rate = audioContext.sampleRate;
    const len = rate * sec;
    const ir = audioContext.createBuffer(2, len, rate);
    for (let ch = 0; ch < 2; ch++) {
      const data = ir.getChannelData(ch);
      for (let i = 0; i < len; i++) data[i] = (Math.random() * 2 - 1) * Math.pow(1 - i / len, decay);
    }
    return ir;
  }
  if (!reverbNode) {
    reverbNode = audioContext.createConvolver();
    reverbNode.buffer = createReverbBuffer(2.4, 2.2);
  }

  const instrument = instrumentSelect?.value || 'PIANO';
  const now = audioContext.currentTime + 0.06;
  const tempo = (tempoRange && tempoRange.value) ? parseInt(tempoRange.value, 10) : 120;
  const secondsPerBeat = 60 / tempo;
  let startTime = now;

  const masterDry = audioContext.createGain();
  const masterWet = audioContext.createGain();
  masterDry.gain.setValueAtTime(0.9, now);
  masterWet.gain.setValueAtTime(0.25, now);
  activeAudioNodes.push(masterDry, masterWet);

  // High-quality master: compressor -> soft clipper -> destination
  const masterComp = audioContext.createDynamicsCompressor();
  masterComp.threshold.setValueAtTime(-6, now);
  masterComp.knee.setValueAtTime(6, now);
  masterComp.ratio.setValueAtTime(6, now);
  masterComp.attack.setValueAtTime(0.003, now);
  masterComp.release.setValueAtTime(0.25, now);
  activeAudioNodes.push(masterComp);

  // soft clipper via waveshaper
  const clip = audioContext.createWaveShaper();
  activeAudioNodes.push(clip);
  function makeSoftClipper(amount = 2) {
    const samples = 44100;
    const curve = new Float32Array(samples);
    const k = typeof amount === 'number' ? amount : 2;
    for (let i = 0; i < samples; ++i) {
      const x = (i * 2) / samples - 1;
      curve[i] = Math.tanh(k * x);
    }
    return curve;
  }
  clip.curve = makeSoftClipper(3);
  clip.oversample = '4x';

  // routing: masterDry + masterWet -> masterComp -> clip -> destination
  masterDry.connect(masterComp);
  masterWet.connect(masterComp);
  masterComp.connect(clip);
  clip.connect(audioContext.destination);

  // reverb connects into masterWet
  reverbNode.connect(masterWet);

  // optional chorus (global)
  let chorusLFO = null;
  let chorusDelay = null;
  if (chorusChk && chorusChk.checked) {
    const amount = (chorusAmount && chorusAmount.value) ? parseInt(chorusAmount.value, 10) : 30;
    chorusDelay = audioContext.createDelay();
    chorusDelay.delayTime.value = 0.02; // base
    chorusLFO = audioContext.createOscillator();
    chorusLFO.frequency.value = 0.8;
    const chorusLFOGain = audioContext.createGain();
    chorusLFOGain.gain.value = Math.max(0.001, amount / 1000);
    chorusLFO.connect(chorusLFOGain);
    chorusLFOGain.connect(chorusDelay.delayTime);
    // route: masterDry -> chorusDelay -> destination (mixed)
    masterDry.connect(chorusDelay);
    chorusDelay.connect(audioContext.destination);
    chorusLFO.start(now);
    activeAudioSources.push(chorusLFO);
    activeAudioNodes.push(chorusDelay, chorusLFOGain);
    // stop later when playback ends
  }

  let globalNoteIndex = 0;
  for (const measure of parsed) {
    for (const note of measure) {
      const freq = noteFrequency(note.name, note.accidental);
      if (!freq) { startTime += note.beats * secondsPerBeat; continue; }

      const isViolin = instrument === 'VIOLIN';
      const duration = note.beats * secondsPerBeat * 0.95;
      // humanize timing and velocity
      if (humanizeChk && humanizeChk.checked) {
        const humanMs = 18; // +- ms jitter
        const jitter = (Math.random() * 2 - 1) * humanMs / 1000;
        startTime += jitter;
      }
      // swing: delay every second note slightly
      if (swingChk && swingChk.checked) {
        if (globalNoteIndex % 2 === 1) startTime += secondsPerBeat * 0.06; // 6% swing
      }
      const voiceGain = audioContext.createGain();
      // apply humanized velocity
      if (humanizeChk && humanizeChk.checked) {
        const vel = 1 - Math.random() * 0.14; // reduce up to 14%
        voiceGain.gain.setValueAtTime(vel, startTime);
      }
      applyADSREnvelope(voiceGain, startTime, duration, isViolin ? 0.06 : 0.01, isViolin ? 0.12 : 0.08, isViolin ? 0.8 : 0.7, isViolin ? 0.12 : 0.08);

      const pan = audioContext.createStereoPanner();
      pan.pan.value = (Math.random() * 0.6 - 0.3);

      // If user selected samples, try sample playback first
      const wantSamples = soundModeSelect && soundModeSelect.value === 'SAMPLES';
      if (wantSamples) {
        const ok = await loadInstrumentSample(instrument);
        if (ok && samples[instrument]) {
          const bufSrc = audioContext.createBufferSource();
          bufSrc.buffer = samples[instrument];
          bufSrc.playbackRate.setValueAtTime((freq / SAMPLE_BASE_FREQ), startTime);

          // connect through voiceGain -> pan -> dry + reverb
          bufSrc.connect(voiceGain);
          voiceGain.connect(pan);
          pan.connect(masterDry);
          voiceGain.connect(reverbNode);

          bufSrc.start(startTime);
          bufSrc.stop(startTime + duration + 0.05);
          activeAudioSources.push(bufSrc);
          startTime += note.beats * secondsPerBeat;
          globalNoteIndex++;
          continue; // next note
        }
      }

      if (!isViolin) {
        const o1 = audioContext.createOscillator();
        o1.type = 'triangle';
        o1.frequency.setValueAtTime(freq, startTime);
        o1.detune.value = (Math.random() * 12 - 6);

        const o2 = audioContext.createOscillator();
        o2.type = 'sine';
        o2.frequency.setValueAtTime(freq * 2, startTime);
        o2.detune.value = (Math.random() * 6 - 3);

        const g2 = audioContext.createGain();
        g2.gain.setValueAtTime(0.18, startTime);

        const noiseSrc = audioContext.createBufferSource();
        noiseSrc.buffer = noiseBuffer;
        const noiseGain = audioContext.createGain();
        noiseGain.gain.setValueAtTime(0.15, startTime);
        noiseGain.gain.exponentialRampToValueAtTime(0.001, startTime + Math.min(0.06, duration));

        o1.connect(voiceGain);
        o2.connect(g2);
        g2.connect(voiceGain);
        noiseSrc.connect(noiseGain);
        noiseGain.connect(voiceGain);

        voiceGain.connect(pan);
        pan.connect(masterDry);
        voiceGain.connect(reverbNode);

        o1.start(startTime);
        o1.stop(startTime + duration + 0.05);
        o2.start(startTime);
        o2.stop(startTime + duration + 0.05);
        noiseSrc.start(startTime);
        activeAudioSources.push(o1, o2, noiseSrc);
        activeAudioNodes.push(g2, noiseGain, pan, voiceGain);
      } else {
        const o = audioContext.createOscillator();
        o.type = 'sawtooth';
        o.frequency.setValueAtTime(freq, startTime);

        const lfo = audioContext.createOscillator();
        lfo.frequency.setValueAtTime(5.5, startTime);
        const lfoGain = audioContext.createGain();
        lfoGain.gain.setValueAtTime(8, startTime);
        lfo.connect(lfoGain);
        lfoGain.connect(o.detune);

        const filt = audioContext.createBiquadFilter();
        filt.type = 'lowpass';
        filt.frequency.setValueAtTime(1800, startTime);
        filt.Q.setValueAtTime(0.8, startTime);

        o.connect(filt);
        filt.connect(voiceGain);
        voiceGain.connect(pan);
        pan.connect(masterDry);
        voiceGain.connect(reverbNode);

        lfo.start(startTime);
        o.start(startTime);
        o.stop(startTime + duration + 0.05);
        lfo.stop(startTime + duration + 0.05);
        activeAudioSources.push(o, lfo);
        activeAudioNodes.push(filt, pan, voiceGain);
      }

      voiceGain.connect(pan);
      pan.connect(masterDry);

      startTime += note.beats * secondsPerBeat;
      globalNoteIndex++;
    }
  }
  // schedule chorus stop shortly after last note
  if (chorusLFO) {
    const stopAt = startTime + 1.2;
    chorusLFO.stop(stopAt);
  }
}

 if (btnPlayAudio) {
   btnPlayAudio.addEventListener('click', () => {
     if (lastParsedMusic.length === 0) return;
     playParsedMusic(lastParsedMusic);
   });
 }

 if (btnStopAudio) {
   btnStopAudio.addEventListener('click', () => {
     stopAudioPlayback();
   });
 }
