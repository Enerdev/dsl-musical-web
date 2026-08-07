const express = require('express');
const cors = require('cors');
const { spawn } = require('child_process');

const app = express();
app.use(cors());
// El codigo .mus llega como texto plano en el body del POST
app.use(express.text({ type: '*/*', limit: '1mb' }));

app.get('/', (req, res) => {
  res.send('DSL Musical - Compilador API. POST tu codigo .mus a /compilar');
});

app.post('/compilar', (req, res) => {
  const codigo = req.body || '';

  if (!codigo.trim()) {
    return res.status(400).json({ error: 'No se recibio codigo fuente.' });
  }

  const proceso = spawn('./compilador');

  let salida = '';
  let errorSalida = '';
  let respondido = false;

  proceso.stdout.on('data', (data) => { salida += data.toString(); });
  proceso.stderr.on('data', (data) => { errorSalida += data.toString(); });

  // Timeout de seguridad: si el binario se cuelga (ej. loop infinito
  // por un bug no detectado), lo matamos a los 5s en vez de dejar el
  // proceso colgado consumiendo memoria en Railway.
  const timeout = setTimeout(() => {
    proceso.kill('SIGKILL');
  }, 5000);

  proceso.on('close', (code) => {
    clearTimeout(timeout);
    if (respondido) return;
    respondido = true;
    res.json({ output: salida, stderr: errorSalida, exitCode: code });
  });

  proceso.on('error', (err) => {
    clearTimeout(timeout);
    if (respondido) return;
    respondido = true;
    res.status(500).json({ error: 'No se pudo ejecutar el compilador: ' + err.message });
  });

  proceso.stdin.write(codigo);
  proceso.stdin.end();
});

// Generar una partitura simple (SVG) desde el codigo .mus usando un script Python
app.post('/sheet', (req, res) => {
  const codigo = req.body || '';
  if (!codigo.trim()) return res.status(400).json({ error: 'No se recibio codigo fuente.' });

  const format = (req.query.format || 'svg').toLowerCase();
  const proceso = spawn('python3', ['generate_sheet.py', format], { cwd: __dirname });

  let chunks = [];
  let errorChunks = [];
  let responded = false;

  proceso.stdout.on('data', (data) => { chunks.push(Buffer.isBuffer(data) ? data : Buffer.from(data)); });
  proceso.stderr.on('data', (data) => { errorChunks.push(Buffer.isBuffer(data) ? data : Buffer.from(data)); });

  const timeout = setTimeout(() => { proceso.kill('SIGKILL'); }, 5000);

  proceso.on('close', (code) => {
    clearTimeout(timeout);
    if (responded) return;
    responded = true;
    const salida = Buffer.concat(chunks).toString('utf8');
    const errorSalida = Buffer.concat(errorChunks).toString('utf8');
    if (code !== 0) return res.status(500).json({ error: 'Error generando partitura', stderr: errorSalida });
    // salida contiene el resultado; devolver con el Content-Type apropiado
    if (format === 'musicxml') {
      res.setHeader('Content-Type', 'application/xml');
      res.send(salida);
    } else if (format === 'abc') {
      res.setHeader('Content-Type', 'text/plain');
      res.send(salida);
    } else if (format === 'pdf') {
      res.setHeader('Content-Type', 'application/pdf');
      res.setHeader('Content-Disposition', 'attachment; filename="partitura.pdf"');
      res.end(Buffer.concat(chunks));
    } else {
      res.setHeader('Content-Type', 'image/svg+xml');
      res.send(salida);
    }
  });

  proceso.on('error', (err) => {
    clearTimeout(timeout);
    if (responded) return;
    responded = true;
    res.status(500).json({ error: 'No se pudo ejecutar el generador: ' + err.message });
  });

  proceso.stdin.write(codigo);
  proceso.stdin.end();
});

const PORT = process.env.PORT || 3000;
app.listen(PORT, () => console.log(`Servidor escuchando en puerto ${PORT}`));
