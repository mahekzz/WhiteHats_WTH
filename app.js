// ---- Required modules ----
const http = require('http');
const fs = require('fs');
const { Server } = require('socket.io');
const { SerialPort } = require('serialport');
const { ReadlineParser } = require('@serialport/parser-readline');

// ---- Serve index.html ----
const index = fs.readFileSync('index.html');

// ---- Serial port setup ----
const port = new SerialPort({
  path: 'COM3',       // ← your ESP32 port
  baudRate: 9600,
  dataBits: 8,
  parity: 'none',
  stopBits: 1,
  // rtscts: false,    // optional; you can omit
});

port.on('open', () => console.log('Serial port OPEN:', port.path));
port.on('error', (e) => console.error('Serial error:', e.message));

const parser = port.pipe(new ReadlineParser({ delimiter: '\r\n' }));
parser.on('data', (line) => console.log('From ESP32:', line));

// ---- HTTP + Socket.io server ----
const app = http.createServer((req, res) => {
  res.writeHead(200, { 'Content-Type': 'text/html' });
  res.end(index);
});

const io = new Server(app);

// helper: write a one-char code + newline, with guard
function sendCode(socket, code, label) {
  if (!port.isOpen) {
    socket.emit('ledStatus', `Serial not open; couldn’t send ${label}`);
    return;
  }
  port.write(code + '\n', (err) => {
    if (err) {
      console.error('Write error:', err);
      socket.emit('ledStatus', `Error sending ${label}`);
    } else {
      console.log(`Sent to ESP32: ${JSON.stringify(code)}`);
      socket.emit('ledStatus', label);
    }
  });
}

io.on('connection', (socket) => {
  console.log('Client connected');

  socket.on('pulseLED', () => {
    console.log('Browser requested: PULSE');
    sendCode(socket, '1', 'Pulsing LED + music…');
  });

  socket.on('stopLED', () => {
    console.log('Browser requested: STOP');
    sendCode(socket, '0', 'Stopped');
  });

  // Happy (solid yellow)
  socket.on('Happy', () => {
    console.log('Browser requested: HAPPY');
    sendCode(socket, '2', 'Solid yellow (Happy)');
  });

  // (Optional) add the rest when you’re ready:
  socket.on('Calm',      () => sendCode(socket, '3', 'Calm mode'));
  socket.on('Energetic', () => sendCode(socket, '4', 'Energetic mode'));
  socket.on('Romantic',  () => sendCode(socket, '5', 'Romantic mode'));
  socket.on('Focus',     () => sendCode(socket, '6', 'Focus mode'));
  socket.on('Party',     () => sendCode(socket, '7', 'Party mode'));
});

app.listen(3000, () => {
  console.log('Server running at http://localhost:3000');
});
