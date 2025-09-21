#include <FastLED.h>

// ------------- LED setup -------------
#define LED_PIN       2
#define NUM_LEDS      30
#define BRIGHTNESS    100
#define LED_TYPE      WS2812B
#define COLOR_ORDER   GRB
CRGB leds[NUM_LEDS];

// ------------- Buzzer (passive) -------------
#define BUZZER_PIN    18

// ------------- Modes -------------
enum Mode : uint8_t {
  MODE_OFF    = 0,
  MODE_PULSE  = 1,
  MODE_YELLOW = 2,  // Happy
  MODE_CALM   = 3,  // Blue
  MODE_ENERG  = 4,  // Orange
  MODE_ROM    = 5,  // Pink
  MODE_FOCUS  = 6,  // Cool white
  MODE_PARTY  = 7   // Rainbow cycle
};
Mode mode = MODE_OFF;

// ------------- Non-blocking pulse state -------------
uint8_t  pulseLevel = 0;     // 0..255
int8_t   pulseDir   = 5;     // step (+/-)
const uint16_t PULSE_STEP_MS = 20;
unsigned long lastPulseMs = 0;

// ------------- Party (rainbow) state -------------
uint8_t  partyHue = 0;
const uint16_t PARTY_STEP_MS = 20;
unsigned long lastPartyMs = 0;

// ------------- Notes (Für Elise intro) -------------
#define NOTE_C4   262
#define NOTE_CS4  277
#define NOTE_D4   294
#define NOTE_DS4  311
#define NOTE_E4   330
#define NOTE_F4   349
#define NOTE_FS4  370
#define NOTE_G4   392
#define NOTE_GS4  415
#define NOTE_A4   440
#define NOTE_AS4  466
#define NOTE_B4   494
#define NOTE_C5   523
#define NOTE_CS5  554
#define NOTE_D5   587
#define NOTE_DS5  622
#define NOTE_E5   659

const int melody[] = {
  // Phrase 1
  NOTE_E5, NOTE_DS5, NOTE_E5, NOTE_DS5, NOTE_E5, NOTE_B4, NOTE_D5, NOTE_C5, NOTE_A4,
  // Bridge 1
  NOTE_C4, NOTE_E4, NOTE_A4, NOTE_B4,
  // Bridge 2
  NOTE_E4, NOTE_GS4, NOTE_B4, NOTE_C5,
  // Phrase 1 repeat
  NOTE_E4, NOTE_E5, NOTE_DS5, NOTE_E5, NOTE_DS5, NOTE_E5, NOTE_B4, NOTE_D5, NOTE_C5, NOTE_A4
};
const uint16_t dur[] = {
  300,300,300,300,300,300,300,300,600,
  300,300,300,600,
  300,300,300,600,
  300,300,300,300,300,300,300,300,300,600
};
const uint8_t MELODY_LEN = sizeof(melody)/sizeof(melody[0]);
const uint16_t NOTE_GAP_MS   = 40;
const uint16_t LOOP_PAUSE_MS = 1200;

// Non-blocking song state
uint16_t      noteIdx = 0;
unsigned long noteChangeAt = 0;
bool          inLoopPause = false;

void startNote(uint16_t idx) {
  int freq = melody[idx];
  if (freq > 0) tone(BUZZER_PIN, freq);
  else          noTone(BUZZER_PIN);
  noteChangeAt = millis() + dur[idx] + NOTE_GAP_MS;
}

void stopSong() {
  noTone(BUZZER_PIN);
  noteIdx = 0;
  inLoopPause = false;
  noteChangeAt = 0;
}

// Helpers
void setSolid(const CRGB& c) {
  fill_solid(leds, NUM_LEDS, c);
  FastLED.show();
}

// ------------- Setup -------------
void setup() {
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);
  setSolid(CRGB::Black);

  Serial.begin(9600);
  pinMode(BUZZER_PIN, OUTPUT);
  noTone(BUZZER_PIN);
}

// ------------- Tasks -------------
void pulseStep() {
  unsigned long now = millis();
  if (now - lastPulseMs < PULSE_STEP_MS) return;
  lastPulseMs = now;

  int next = (int)pulseLevel + pulseDir;
  if (next >= 255) { next = 255; pulseDir = -abs(pulseDir); }
  if (next <= 0)   { next = 0;   pulseDir =  abs(pulseDir); }
  pulseLevel = (uint8_t)next;

  fill_solid(leds, NUM_LEDS, CRGB(pulseLevel, 0, 0)); // red breathe
  FastLED.show();
}

void partyStep() {
  unsigned long now = millis();
  if (now - lastPartyMs < PARTY_STEP_MS) return;
  lastPartyMs = now;

  // full-strip rainbow with slowly shifting hue
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CHSV(partyHue + i * 4, 255, 255);
  }
  FastLED.show();
  partyHue++;
}

void songStep() {
  unsigned long now = millis();

  if (noteChangeAt == 0) {         // start first note
    startNote(noteIdx);
    return;
  }
  if (now < noteChangeAt) return;  // wait until it's time to advance

  noTone(BUZZER_PIN);              // end current note

  noteIdx++;
  if (noteIdx >= MELODY_LEN) {
    if (!inLoopPause) {            // insert loop pause
      inLoopPause = true;
      noteChangeAt = now + LOOP_PAUSE_MS;
      return;
    } else {                       // loop back
      inLoopPause = false;
      noteIdx = 0;
    }
  }
  startNote(noteIdx);
}

// ------------- Main loop -------------
void loop() {
  // Read serial commands (ignore CR/LF)
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\r' || c == '\n') continue;

    if (c == '1') {
      mode = MODE_PULSE;
      // reset pulse + song state
      pulseLevel = 0;
      pulseDir   = 5;
      lastPulseMs = 0;
      partyHue = 0;
      stopSong();   // reset song state cleanly
      noteIdx = 0; noteChangeAt = 0; inLoopPause = false;
    }
    else if (c == '0') {
      mode = MODE_OFF;
      setSolid(CRGB::Black);
      stopSong();
    }
    else if (c == '2') {  // Happy → Yellow
      mode = MODE_YELLOW;
      setSolid(CRGB::Yellow);
      stopSong();
    }
    else if (c == '3') {  // Calm → soft blue
      mode = MODE_CALM;
      setSolid(CRGB(80, 140, 255));   // (R,G,B)
      stopSong();
    }
    else if (c == '4') {  // Energetic → orange
      mode = MODE_ENERG;
      setSolid(CRGB(255, 100, 0));
      stopSong();
    }
    else if (c == '5') {  // Romantic → pink
      mode = MODE_ROM;
      setSolid(CRGB(255, 64, 128));   // warm pink
      stopSong();
    }
    else if (c == '6') {  // Focus → cool white
      mode = MODE_FOCUS;
      setSolid(CRGB(200, 225, 255));  // slightly bluish white
      stopSong();
    }
    else if (c == '7') {  // Party → rainbow animation
      mode = MODE_PARTY;
      partyHue = 0;
      stopSong();
    }
  }

  // Run tasks depending on mode
  if (mode == MODE_PULSE) {
    pulseStep();
    songStep();
  } else if (mode == MODE_PARTY) {
    partyStep();
  }
  // Other modes are solid; nothing to update.
}
