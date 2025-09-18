#include <WiFi.h>
#include "ThingSpeak.h"
#include <Preferences.h>

// ========================== Wi-Fi ==========================
const char* ssid     = "YOUR-SSID"; // with parentheses "abc"
const char* password = "YOUR-PASSWORD"; // with parentheses "xyz"

// ======================= ThingSpeak ========================
unsigned long channelID = YOUR THINGSPEAK CHANNEL ID; // without parentheses abc
String writeAPIKey      = "YOUR WRITE API KEY"; // with parentheses "xyz"
WiFiClient client;

// ====================== Pin Mapping ========================
// XIAO ESP32-C3: D0=GPIO2, D1=GPIO3, D2=GPIO4 (all ADC capable)
const int PIN_A = D0;   // A -> 100% presence
const int PIN_B = D1;   // B -> 25% presence
const int PIN_C = D2;   // C -> 75% presence

// =========== Sampling / Flicker / Posting Timers ===========
const uint16_t SAMPLE_MICRO_WINDOW_MS = 250;    // finer time resolution
const uint32_t THINGSPEAK_PERIOD_MS   = 15000;  // post every 15 s

// -------- Flicker parameters (runtime & persisted) ---------
// Calibrated defaults (your values)
const uint16_t DEF_FLICKER_WIN_MS       = 4000;    // 4s window
const uint16_t DEF_FLICKER_MIN_DWELL_MS = 600;     // each state dwell ≥600ms
const uint8_t  DEF_FLICKER_MIN_EDGES    = 1;       // at least one 100↔75 toggle
const uint8_t  DEF_FLICKER_MAX_OTHER_PC = 60;      // kept for export/debug
const uint32_t DEF_FLICKER_LATCH_MS     = 10000;   // hold 4% for 10s

// Runtime (loaded from NVS or defaults)
uint16_t flickWinMs       = DEF_FLICKER_WIN_MS;
uint16_t flickMinDwellMs  = DEF_FLICKER_MIN_DWELL_MS;
uint8_t  flickMinEdges    = DEF_FLICKER_MIN_EDGES;
uint8_t  flickMaxOtherPc  = DEF_FLICKER_MAX_OTHER_PC; // not used in tolerant eval
uint32_t flickLatchMs     = DEF_FLICKER_LATCH_MS;

// ============= Default thresholds (from your CSV) ===========
const int DEF_thrA = 1866;  // mV
const int DEF_thrB = 1674;  // mV
const int DEF_thrC = 1695;  // mV

int thrA = DEF_thrA, thrB = DEF_thrB, thrC = DEF_thrC;

// ================== Calibration / Storage ===================
Preferences prefs;
struct ABC { int aMv; int bMv; int cMv; };
ABC cal100{ -1,-1,-1 }, cal75{ -1,-1,-1 }, cal50{ -1,-1,-1 }, cal25{ -1,-1,-1 }, cal5{ -1,-1,-1 };

bool runMode = true;  // start in RUN mode

// =========================== Utils ==========================
void printlnRule() { Serial.println(F("------------------------------------------------------")); }

void connectToWiFi() {
  Serial.print(F("Connecting to Wi-Fi"));
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(400);
    Serial.print(".");
  }
  Serial.println(F("\n✅ Connected to Wi-Fi"));
}

void sendToThingSpeak(int level, const String& statusMsg) {
  ThingSpeak.setField(1, level);
  // Optional: create Field 2 in your channel first if you want status text
  // ThingSpeak.setField(2, statusMsg);
  int code = ThingSpeak.writeFields(channelID, writeAPIKey.c_str());
  Serial.print(F("ThingSpeak -> Level: ")); Serial.print(level);
  Serial.print(F("% | Status: ")); Serial.print(statusMsg);
  Serial.print(F(" | HTTP ")); Serial.println(code);
}

int avgMilliVoltsOver(uint32_t ms, int pin) {
  uint32_t start = millis();
  int32_t sum = 0;
  uint32_t cnt = 0;
  while (millis() - start < ms) {
    sum += analogReadMilliVolts(pin);
    cnt++;
    delay(50); // ~20 Hz
  }
  return (cnt ? sum / (int)cnt : 0);
}

ABC loadABC(const char* key) {
  ABC v;
  String p = String(key);
  v.aMv = prefs.getInt((p + "_a").c_str(), -1);
  v.bMv = prefs.getInt((p + "_b").c_str(), -1);
  v.cMv = prefs.getInt((p + "_c").c_str(), -1);
  return v;
}

void saveABC(const char* key, const ABC& v) {
  String p = String(key);
  prefs.putInt((p + "_a").c_str(), v.aMv);
  prefs.putInt((p + "_b").c_str(), v.bMv);
  prefs.putInt((p + "_c").c_str(), v.cMv);
}

void saveThresholds() {
  prefs.putInt("thrA", thrA);
  prefs.putInt("thrB", thrB);
  prefs.putInt("thrC", thrC);
}
void loadThresholds() {
  if (prefs.isKey("thrA") && prefs.isKey("thrB") && prefs.isKey("thrC")) {
    thrA = prefs.getInt("thrA", DEF_thrA);
    thrB = prefs.getInt("thrB", DEF_thrB);
    thrC = prefs.getInt("thrC", DEF_thrC);
  } else {
    thrA = DEF_thrA; thrB = DEF_thrB; thrC = DEF_thrC;
  }
}

// ---- Persist/Load flicker params ----
void saveFlickerParams() {
  prefs.putUShort("fkWinMs", flickWinMs);
  prefs.putUShort("fkMinDw", flickMinDwellMs);
  prefs.putUChar ("fkMinEd", flickMinEdges);
  prefs.putUChar ("fkMaxOp", flickMaxOtherPc);
  prefs.putUInt  ("fkLatch", flickLatchMs);
}
void loadFlickerParams() {
  flickWinMs      = prefs.getUShort("fkWinMs", DEF_FLICKER_WIN_MS);
  flickMinDwellMs = prefs.getUShort("fkMinDw", DEF_FLICKER_MIN_DWELL_MS);
  flickMinEdges   = prefs.getUChar ("fkMinEd", DEF_FLICKER_MIN_EDGES);
  flickMaxOtherPc = prefs.getUChar ("fkMaxOp", DEF_FLICKER_MAX_OTHER_PC);
  flickLatchMs    = prefs.getUInt  ("fkLatch", DEF_FLICKER_LATCH_MS);
}

String missingStatesList() {
  String m = "";
  if (cal100.aMv < 0) { if (m.length()) m += ", "; m += "100"; }
  if (cal75.aMv  < 0) { if (m.length()) m += ", "; m += "75";  }
  if (cal50.aMv  < 0) { if (m.length()) m += ", "; m += "50";  }
  if (cal25.aMv  < 0) { if (m.length()) m += ", "; m += "25";  }
  if (cal5.aMv   < 0) { if (m.length()) m += ", "; m += "5";   }
  return (m.length() ? m : "none");
}

bool hasSufficientDataForCompute(String &whyNot) {
  bool ok = true;
  // A: ON {100,50}, OFF {75,25,5}
  if (!((cal100.aMv>=0) || (cal50.aMv>=0))) { ok=false; whyNot += "A missing ON (need cal 100 or 50)\n"; }
  if (!((cal75.aMv>=0)  || (cal25.aMv>=0) || (cal5.aMv>=0))) { ok=false; whyNot += "A missing OFF (need cal 75/25/5)\n"; }
  // B: ON {50,25,5}, OFF {100,75}
  if (!((cal50.bMv>=0) || (cal25.bMv>=0) || (cal5.bMv>=0))) { ok=false; whyNot += "B missing ON (need cal 50/25/5)\n"; }
  if (!((cal100.bMv>=0) || (cal75.bMv>=0))) { ok=false; whyNot += "B missing OFF (need cal 100 or 75)\n"; }
  // C: ON {75,5}, OFF {100,50,25}
  if (!((cal75.cMv>=0) || (cal5.cMv>=0))) { ok=false; whyNot += "C missing ON (need cal 75 or 5)\n"; }
  if (!((cal100.cMv>=0) || (cal50.cMv>=0) || (cal25.cMv>=0))) { ok=false; whyNot += "C missing OFF (need cal 100/50/25)\n"; }
  return ok;
}

void computeThresholdsFromCal() {
  // A:
  float A_on=0, A_off=0; int nAon=0, nAoff=0;
  if (cal100.aMv>=0){A_on += cal100.aMv; nAon++;}
  if (cal50.aMv>=0) {A_on += cal50.aMv;  nAon++;}
  if (cal75.aMv>=0) {A_off+= cal75.aMv;  nAoff++;}
  if (cal25.aMv>=0) {A_off+= cal25.aMv;  nAoff++;}
  if (cal5.aMv>=0)  {A_off+= cal5.aMv;   nAoff++;}
  // B:
  float B_on=0, B_off=0; int nBon=0, nBoff=0;
  if (cal50.bMv>=0) {B_on += cal50.bMv; nBon++;}
  if (cal25.bMv>=0) {B_on += cal25.bMv; nBon++;}
  if (cal5.bMv>=0)  {B_on += cal5.bMv;  nBon++;}
  if (cal100.bMv>=0){B_off+= cal100.bMv; nBoff++;}
  if (cal75.bMv>=0) {B_off+= cal75.bMv;  nBoff++;}
  // C:
  float C_on=0, C_off=0; int nCon=0, nCoff=0;
  if (cal75.cMv>=0) {C_on += cal75.cMv; nCon++;}
  if (cal5.cMv>=0)  {C_on += cal5.cMv;  nCon++;}
  if (cal100.cMv>=0){C_off+= cal100.cMv; nCoff++;}
  if (cal50.cMv>=0) {C_off+= cal50.cMv;  nCoff++;}
  if (cal25.cMv>=0) {C_off+= cal25.cMv;  nCoff++;}

  float Aon = (nAon>0)? A_on/nAon : 2500;
  float Aoff= (nAoff>0)?A_off/nAoff: 1000;
  float Bon = (nBon>0)? B_on/nBon : 2500;
  float Boff= (nBoff>0)?B_off/nBoff: 1000;
  float Con = (nCon>0)? C_on/nCon : 2500;
  float Coff= (nCoff>0)?C_off/nCoff: 1000;

  thrA = constrain((int)((Aon + Aoff)/2.0f - 50.0f), 300, 3200);
  thrB = constrain((int)((Bon + Boff)/2.0f - 50.0f), 300, 3200);
  thrC = constrain((int)((Con + Coff)/2.0f - 50.0f), 300, 3200);

  saveThresholds();

  printlnRule();
  Serial.println(F("✅ Computed thresholds (mV):"));
  Serial.print(F("  thrA=")); Serial.println(thrA);
  Serial.print(F("  thrB=")); Serial.println(thrB);
  Serial.print(F("  thrC=")); Serial.println(thrC);
  printlnRule();
}

void loadAllCal() {
  cal100 = loadABC("cal100");
  cal75  = loadABC("cal75");
  cal50  = loadABC("cal50");
  cal25  = loadABC("cal25");
  cal5   = loadABC("cal5");
  loadThresholds();
  loadFlickerParams();
}

void showAll() {
  printlnRule();
  Serial.println(F("Saved calibration (mV):"));
  Serial.print(F("  100%: A=")); Serial.print(cal100.aMv);
  Serial.print(F(" B=")); Serial.print(cal100.bMv);
  Serial.print(F(" C=")); Serial.println(cal100.cMv);
  Serial.print(F("   75%: A=")); Serial.print(cal75.aMv);
  Serial.print(F(" B=")); Serial.print(cal75.bMv);
  Serial.print(F(" C=")); Serial.println(cal75.cMv);
  Serial.print(F("   50%: A=")); Serial.print(cal50.aMv);
  Serial.print(F(" B=")); Serial.print(cal50.bMv);
  Serial.print(F(" C=")); Serial.println(cal50.cMv);
  Serial.print(F("   25%: A=")); Serial.print(cal25.aMv);
  Serial.print(F(" B=")); Serial.print(cal25.bMv);
  Serial.print(F(" C=")); Serial.println(cal25.cMv);
  Serial.print(F("    5%: A=")); Serial.print(cal5.aMv);
  Serial.print(F(" B=")); Serial.print(cal5.bMv);
  Serial.print(F(" C=")); Serial.println(cal5.cMv);

  Serial.println(F("Thresholds in use (mV):"));
  Serial.print(F("  thrA=")); Serial.println(thrA);
  Serial.print(F("  thrB=")); Serial.println(thrB);
  Serial.print(F("  thrC=")); Serial.println(thrC);

  Serial.println(F("Flicker parameters:"));
  Serial.print(F("  windowMs=")); Serial.println(flickWinMs);
  Serial.print(F("  minDwellMs=")); Serial.println(flickMinDwellMs);
  Serial.print(F("  minEdges=")); Serial.println(flickMinEdges);
  Serial.print(F("  maxOther%=")); Serial.println(flickMaxOtherPc);
  Serial.print(F("  latchMs=")); Serial.println(flickLatchMs);
  Serial.print(F("Missing states: ")); Serial.println(missingStatesList());
  printlnRule();
}

void resetAll() {
  prefs.clear();
  cal100 = {-1,-1,-1}; cal75 = {-1,-1,-1}; cal50 = {-1,-1,-1}; cal25 = {-1,-1,-1}; cal5 = {-1,-1,-1};
  thrA = DEF_thrA; thrB = DEF_thrB; thrC = DEF_thrC;
  flickWinMs       = DEF_FLICKER_WIN_MS;
  flickMinDwellMs  = DEF_FLICKER_MIN_DWELL_MS;
  flickMinEdges    = DEF_FLICKER_MIN_EDGES;
  flickMaxOtherPc  = DEF_FLICKER_MAX_OTHER_PC;
  flickLatchMs     = DEF_FLICKER_LATCH_MS;
  printlnRule();
  Serial.println(F("All calibration/thresholds cleared. Reverting to defaults (incl. flicker)."));
  printlnRule();
}

// ---------------- Export helpers ----------------
void exportCSV() {
  printlnRule();
  Serial.println(F("BEGIN_CSV"));
  Serial.println(F("state,A_mV,B_mV,C_mV"));
  Serial.print(F("100,")); Serial.print(cal100.aMv); Serial.print(','); Serial.print(cal100.bMv); Serial.print(','); Serial.println(cal100.cMv);
  Serial.print(F("75,"));  Serial.print(cal75.aMv);  Serial.print(','); Serial.print(cal75.bMv);  Serial.print(','); Serial.println(cal75.cMv);
  Serial.print(F("50,"));  Serial.print(cal50.aMv);  Serial.print(','); Serial.print(cal50.bMv);  Serial.print(','); Serial.println(cal50.cMv);
  Serial.print(F("25,"));  Serial.print(cal25.aMv);  Serial.print(','); Serial.print(cal25.bMv);  Serial.print(','); Serial.println(cal25.cMv);
  Serial.print(F("5,"));   Serial.print(cal5.aMv);   Serial.print(','); Serial.print(cal5.bMv);   Serial.print(','); Serial.println(cal5.cMv);
  Serial.println(F(""));
  Serial.println(F("thresholds,thrA_mV,thrB_mV,thrC_mV"));
  Serial.print(F("values,")); Serial.print(thrA); Serial.print(','); Serial.print(thrB); Serial.print(','); Serial.println(thrC);
  Serial.println(F(""));
  Serial.println(F("flicker,windowMs,minDwellMs,minEdges,maxOtherPercent,latchMs"));
  Serial.print(F("values,")); Serial.print(flickWinMs); Serial.print(','); Serial.print(flickMinDwellMs); Serial.print(',');
  Serial.print(flickMinEdges); Serial.print(','); Serial.print(flickMaxOtherPc); Serial.print(','); Serial.println(flickLatchMs);
  Serial.println(F("END_CSV"));
  printlnRule();
  Serial.println(F("Copy everything between BEGIN_CSV and END_CSV into a .csv file."));
  printlnRule();
}

void exportJSON() {
  printlnRule();
  Serial.println(F("BEGIN_JSON"));
  Serial.print(F("{\"calibration\":{"));
  Serial.print(F("\"100\":{")); Serial.print(F("\"A\":")); Serial.print(cal100.aMv); Serial.print(F(",\"B\":")); Serial.print(cal100.bMv); Serial.print(F(",\"C\":")); Serial.print(cal100.cMv); Serial.print(F("},"));
  Serial.print(F("\"75\":{"));  Serial.print(F("\"A\":")); Serial.print(cal75.aMv);  Serial.print(F(",\"B\":")); Serial.print(cal75.bMv);  Serial.print(F(",\"C\":")); Serial.print(cal75.cMv);  Serial.print(F("},"));
  Serial.print(F("\"50\":{"));  Serial.print(F("\"A\":")); Serial.print(cal50.aMv);  Serial.print(F(",\"B\":")); Serial.print(cal50.bMv);  Serial.print(F(",\"C\":")); Serial.print(cal50.cMv);  Serial.print(F("},"));
  Serial.print(F("\"25\":{"));  Serial.print(F("\"A\":")); Serial.print(cal25.aMv);  Serial.print(F(",\"B\":")); Serial.print(cal25.bMv);  Serial.print(F(",\"C\":")); Serial.print(cal25.cMv);  Serial.print(F("},"));
  Serial.print(F("\"5\":{"));   Serial.print(F("\"A\":")); Serial.print(cal5.aMv);   Serial.print(F(",\"B\":")); Serial.print(cal5.bMv);   Serial.print(F(",\"C\":")); Serial.print(cal5.cMv);   Serial.print(F("}},"));  
  Serial.print(F("\"thresholds\":{")); Serial.print(F("\"thrA\":")); Serial.print(thrA); Serial.print(F(",\"thrB\":")); Serial.print(thrB); Serial.print(F(",\"thrC\":")); Serial.print(thrC); Serial.print(F("},"));
  Serial.print(F("\"flicker\":{"));
  Serial.print(F("\"windowMs\":"));       Serial.print(flickWinMs);       Serial.print(F(","));
  Serial.print(F("\"minDwellMs\":"));     Serial.print(flickMinDwellMs);  Serial.print(F(","));
  Serial.print(F("\"minEdges\":"));       Serial.print(flickMinEdges);    Serial.print(F(","));
  Serial.print(F("\"maxOtherPercent\":")) ;Serial.print(flickMaxOtherPc); Serial.print(F(","));
  Serial.print(F("\"latchMs\":"));        Serial.print(flickLatchMs);     Serial.print(F("}}"));
  Serial.println();
  Serial.println(F("END_JSON"));
  printlnRule();
}

// ========================= Classification ==========================

inline int instantLevelFromBits(uint8_t bits) {
  switch (bits) {
    case 0b100: return 100; // A
    case 0b001: return 75;  // C
    case 0b110: return 50;  // A+B
    case 0b010: return 25;  // B
    case 0b011: return 5;   // B+C
    default:     return 0;  // noise/other
  }
}

int mapBitsToLevel(uint8_t bitsABC) {
  switch (bitsABC) {
    case 0b100: return 100;
    case 0b001: return 75;
    case 0b110: return 50;
    case 0b010: return 25;
    case 0b011: return 5;
    case 0b000: return 5;   // none -> empty
    case 0b101: return 75;  // A+C -> prefer 75
    case 0b111: return 50;  // A+B+C -> middle
    default:     return 5;
  }
}

// ========================= Help & Cal ===============================

void printHelp() {
  printlnRule();
  Serial.println(F("Commands:"));
  Serial.println(F("  cal 100  -> capture 30s for 100% (A)"));
  Serial.println(F("  cal 75   -> capture 30s for 75%  (C)"));
  Serial.println(F("  cal 50   -> capture 30s for 50%  (A+B)"));
  Serial.println(F("  cal 25   -> capture 30s for 25%  (B)"));
  Serial.println(F("  cal 5    -> capture 30s for 5%   (B+C)"));
  Serial.println(F("  cal 4    -> flicker auto-cal: 4s sample of 100%↔75%, saves tuned params"));
  Serial.println(F("  compute  -> compute thresholds (auto-saves)"));
  Serial.println(F("  run      -> force RUN mode now"));
  Serial.println(F("  show     -> show saved calibration & thresholds"));
  Serial.println(F("  export   -> dump CSV"));
  Serial.println(F("  export json -> dump JSON"));
  Serial.println(F("  reset    -> clear all calibration/thresholds (revert to defaults)"));
  Serial.println(F("  help     -> this list"));
  printlnRule();
}

void captureStateBlocking(const char* name, ABC &slot) {
  printlnRule();
  Serial.print(F("CALIBRATING STATE ")); Serial.print(name);
  Serial.println(F(" — hold the inputs steady."));
  Serial.println(F("Capturing for ~30 seconds..."));
  printlnRule();

  int a = avgMilliVoltsOver(30000, PIN_A);
  int b = avgMilliVoltsOver(30000, PIN_B);
  int c = avgMilliVoltsOver(30000, PIN_C);

  slot = {a,b,c};
  String base = String("cal") + name;
  saveABC(base.c_str(), slot);

  Serial.print(F("Captured ")); Serial.print(name); Serial.print(F(" (mV): A="));
  Serial.print(a); Serial.print(F(" B=")); Serial.print(b); Serial.print(F(" C=")); Serial.println(c);
  Serial.print(F("Still missing: ")); Serial.println(missingStatesList());
  printlnRule();
}

// ---- 4-second flicker calibration (locks parameters) ----
void calibrateFlicker4s() {
  printlnRule();
  Serial.println(F("CALIBRATING FLICKER (4s) — alternate 100% (A) and 75% (C) for 4 seconds."));
  Serial.println(F("Sampling..."));
  printlnRule();

  const uint32_t duration = 4000;
  const uint16_t stepMs   = 50;
  uint32_t start = millis();

  uint32_t dwell100=0, dwell75=0, dwellOther=0;
  uint16_t edges=0;
  int prevCore=0;

  while (millis() - start < duration) {
    int mvA = analogReadMilliVolts(PIN_A);
    int mvB = analogReadMilliVolts(PIN_B);
    int mvC = analogReadMilliVolts(PIN_C);
    bool iA = (mvA > thrA), iB = (mvB > thrB), iC = (mvC > thrC);
    uint8_t bits = (iA?0b100:0) | (iB?0b010:0) | (iC?0b001:0);
    int inst = instantLevelFromBits(bits);

    int core = 0;
    if (inst==100) { dwell100 += stepMs; core = 100; }
    else if (inst==75) { dwell75 += stepMs; core = 75; }
    else { dwellOther += stepMs; core = 0; }

    if (core && prevCore && core!=prevCore) edges++;
    if (core) prevCore = core;

    delay(stepMs);
  }

  // Tune: min dwell = 60% of smaller of (dwell100, dwell75), clamp [400..2000] ms
  uint32_t minBoth = min(dwell100, dwell75);
  uint16_t tunedMinDwell = (uint16_t)constrain((int)(minBoth * 0.6f), 400, 2000);
  // Suggest max-other% (kept for export)
  uint8_t tunedMaxOtherPc = (uint8_t)min(60, (int)((dwellOther * 125UL) / (duration)));
  uint8_t tunedMinEdges = 1; // require at least one edge

  // Save
  flickWinMs       = duration;
  flickMinDwellMs  = tunedMinDwell;
  flickMaxOtherPc  = tunedMaxOtherPc;
  flickMinEdges    = tunedMinEdges;
  saveFlickerParams();

  printlnRule();
  Serial.println(F("✅ Flicker parameters updated & saved:"));
  Serial.print(F("  windowMs="));      Serial.println(flickWinMs);
  Serial.print(F("  minDwellMs="));    Serial.println(flickMinDwellMs);
  Serial.print(F("  minEdges="));      Serial.println(flickMinEdges);
  Serial.print(F("  maxOther%="));     Serial.println(flickMaxOtherPc);
  Serial.print(F("  latchMs="));       Serial.println(flickLatchMs);
  printlnRule();
}

// ============================ Setup =============================
void setup() {
  Serial.begin(115200);
  delay(600);

  analogReadResolution(12);
  analogSetPinAttenuation(PIN_A, ADC_11db);
  analogSetPinAttenuation(PIN_B, ADC_11db);
  analogSetPinAttenuation(PIN_C, ADC_11db);

  prefs.begin("wtank-cal", false);
  loadAllCal();

  connectToWiFi();
  ThingSpeak.begin(client);

  printlnRule();
  Serial.println(F("Water Tank — RUN mode (type 'help' for commands)."));
  Serial.print(F("Using thresholds (mV): thrA=")); Serial.print(thrA);
  Serial.print(F(" thrB=")); Serial.print(thrB);
  Serial.print(F(" thrC=")); Serial.println(thrC);
  Serial.println(F("Flicker params (loaded):"));
  Serial.print(F("  windowMs=")); Serial.println(flickWinMs);
  Serial.print(F("  minDwellMs=")); Serial.println(flickMinDwellMs);
  Serial.print(F("  minEdges=")); Serial.println(flickMinEdges);
  Serial.print(F("  maxOther%=")); Serial.println(flickMaxOtherPc);
  Serial.print(F("  latchMs=")); Serial.println(flickLatchMs);
  Serial.print(F("Missing states: ")); Serial.println(missingStatesList());
  printHelp();
}

// ============================== Loop ============================
// 250 ms sensing, immediate 4% trigger on edge + half-min dwell,
// tolerant 4 s window fallback, 15 s ThingSpeak posting
void loop() {
  // ---- Serial commands ----
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim(); cmd.toLowerCase();

    if      (cmd == "help")         printHelp();
    else if (cmd == "show")         showAll();
    else if (cmd == "reset")        resetAll();
    else if (cmd == "export")       exportCSV();
    else if (cmd == "export json")  exportJSON();
    else if (cmd == "compute") {
      String why;
      if (hasSufficientDataForCompute(why)) computeThresholdsFromCal();
      else {
        printlnRule();
        Serial.println(F("❌ Not enough calibration to compute thresholds yet."));
        Serial.print(why);
        Serial.print(F("Still missing: "));
        Serial.println(missingStatesList());
        printlnRule();
      }
    }
    else if (cmd == "run")          { runMode = true; Serial.println(F("▶️ RUN mode.")); }
    else if (cmd == "cal 4")        { runMode = false; calibrateFlicker4s(); Serial.println(F("Resuming RUN mode.")); runMode = true; }
    else if (cmd.startsWith("cal")) {
      runMode = false;
      if      (cmd == "cal 100") captureStateBlocking("100", cal100);
      else if (cmd == "cal 75")  captureStateBlocking("75",  cal75);
      else if (cmd == "cal 50")  captureStateBlocking("50",  cal50);
      else if (cmd == "cal 25")  captureStateBlocking("25",  cal25);
      else if (cmd == "cal 5")   captureStateBlocking("5",   cal5);
      else                       Serial.println(F("Unknown cal command. Use: cal 100|75|50|25|5|4"));

      String why;
      if (hasSufficientDataForCompute(why)) computeThresholdsFromCal();
      else {
        Serial.println(F("Not enough data to compute thresholds yet."));
        Serial.print(F("Reason(s):\n")); Serial.print(why);
        Serial.print(F("Still missing: ")); Serial.println(missingStatesList());
      }
      Serial.println(F("Resuming RUN mode."));
      runMode = true;
    }
    else if (cmd.length() > 0) {
      Serial.println(F("Unknown command. Type 'help'."));
    }
  }

  if (!runMode) { delay(10); return; }

  // ---- Fast micro-window (~250 ms) ----
  uint32_t microStart = millis();
  long aSum=0, bSum=0, cSum=0;
  uint16_t microSamples = 0;

  while (millis() - microStart < SAMPLE_MICRO_WINDOW_MS) {
    aSum += analogReadMilliVolts(PIN_A);
    bSum += analogReadMilliVolts(PIN_B);
    cSum += analogReadMilliVolts(PIN_C);
    microSamples++;
    delay(10); // ~100 Hz inside the 250 ms window
  }

  int aMvAvg = (microSamples ? aSum / (int)microSamples : 0);
  int bMvAvg = (microSamples ? bSum / (int)microSamples : 0);
  int cMvAvg = (microSamples ? cSum / (int)microSamples : 0);

  bool iA = (aMvAvg > thrA);
  bool iB = (bMvAvg > thrB);
  bool iC = (cMvAvg > thrC);

  uint8_t bits = (iA?0b100:0) | (iB?0b010:0) | (iC?0b001:0);
  int inst = instantLevelFromBits(bits);

  // ----- Rolling time-based flicker with tolerant + immediate detection -----
  static uint32_t flickerWindowStart = millis();
  static uint32_t dwellMs100 = 0, dwellMs75 = 0, dwellMsOther = 0;
  static uint32_t dwellSinceLastEdge100 = 0, dwellSinceLastEdge75 = 0; // NEW
  static uint16_t altEdges = 0;
  static int prevCore = 0; // 0/100/75

  static int lastEvaluatedLevel = 5;     // default empty
  static uint32_t flickerLatchUntil = 0; // latch timer

  uint16_t thisWindowMs = SAMPLE_MICRO_WINDOW_MS;

  int core = 0;
  switch (inst) {
    case 100:
      dwellMs100 += thisWindowMs;
      dwellSinceLastEdge100 += thisWindowMs;
      core = 100;
      break;
    case 75:
      dwellMs75  += thisWindowMs;
      dwellSinceLastEdge75  += thisWindowMs;
      core = 75;
      break;
    default:
      dwellMsOther += thisWindowMs;
      core = 0;
      break;
  }

  // Edge detect (100 <-> 75)
  bool isEdge = false;
  if (core && prevCore && core != prevCore) {
    altEdges++;
    isEdge = true;
  }
  if (core) prevCore = core;

  // Immediate trigger: on a real alternation if both halves have ≥½ of min dwell
  if (isEdge) {
    const uint16_t halfMin = max<uint16_t>(flickMinDwellMs / 2, 250); // at least ~250 ms
    bool enoughEachHalf =
      (dwellSinceLastEdge100 >= halfMin && dwellSinceLastEdge75 >= halfMin);

    if (enoughEachHalf && millis() >= flickerLatchUntil) {
      lastEvaluatedLevel = 4;
      flickerLatchUntil  = millis() + flickLatchMs;
      // reset edge accumulators after a trigger
      dwellSinceLastEdge100 = 0;
      dwellSinceLastEdge75  = 0;
      Serial.println(F("[FLICKER] Immediate trigger via edge + half-dwell"));
    } else {
      // on any edge, reset the opposite accumulator so we measure “since edge”
      if (core == 100) dwellSinceLastEdge75 = 0;
      if (core == 75)  dwellSinceLastEdge100 = 0;
    }
  }

  // Window evaluation (tolerant): require both present, ≥50% combined, ≥1 edge
  if (millis() - flickerWindowStart >= flickWinMs) {
    uint32_t win = flickWinMs;

    bool enough100   = (dwellMs100 >= flickMinDwellMs);
    bool enough75    = (dwellMs75  >= flickMinDwellMs);
    uint32_t duo     = dwellMs100 + dwellMs75;
    bool duoShareOK  = (duo >= (win * 50) / 100);     // ≥50% combined time in 100/75
    bool edgesOK     = (altEdges >= flickMinEdges);   // ≥1 toggle

    bool flickerDetected = enough100 && enough75 && duoShareOK && edgesOK;

    int normalLevel = mapBitsToLevel(bits); // last micro decision

    if (!(millis() < flickerLatchUntil)) { // only affect level if not already latched
      if (flickerDetected) {
        lastEvaluatedLevel = 4;
        flickerLatchUntil  = millis() + flickLatchMs; // <-- corrected

      } else {
        lastEvaluatedLevel = normalLevel;
      }
    }

    // Debug summary each evaluation
    Serial.print(F("[FLICKER ")); Serial.print(win); Serial.print(F("ms] "));
    Serial.print(F("d100="));  Serial.print(dwellMs100);
    Serial.print(F("ms d75=")); Serial.print(dwellMs75);
    Serial.print(F("ms duo=")); Serial.print(duo);
    Serial.print(F("ms edges=")); Serial.print(altEdges);
    Serial.print(F(" -> level=")); Serial.println(lastEvaluatedLevel);

    // Reset window + edge dwell accumulators
    dwellMs100 = dwellMs75 = dwellMsOther = 0;
    altEdges = 0;
    prevCore = 0;
    dwellSinceLastEdge100 = 0;
    dwellSinceLastEdge75  = 0;
    flickerWindowStart = millis();
  }

  // Debug each 250 ms decision
  Serial.print(F("A=")); Serial.print(iA ? "1" : "0");
  Serial.print(F(" (")); Serial.print(aMvAvg); Serial.print(F("mV), "));
  Serial.print(F("B=")); Serial.print(iB ? "1" : "0");
  Serial.print(F(" (")); Serial.print(bMvAvg); Serial.print(F("mV), "));
  Serial.print(F("C=")); Serial.print(iC ? "1" : "0");
  Serial.print(F(" (")); Serial.print(cMvAvg); Serial.print(F("mV)  -> pending level "));
  Serial.println(lastEvaluatedLevel);

  // ----- Post to ThingSpeak every 15 s -----
  static uint32_t lastPost = 0;
  if (millis() - lastPost >= THINGSPEAK_PERIOD_MS) {
    int level = (millis() < flickerLatchUntil) ? 4 : lastEvaluatedLevel;

    String status = (level == 100) ? "A" :
                    (level == 75)  ? "C" :
                    (level == 50)  ? "A+B" :
                    (level == 25)  ? "B" :
                    (level == 5)   ? "B+C" :
                    (level == 4)   ? "Flicker 100<->75" : "Unknown";

    sendToThingSpeak(level, "RUN " + status);
    lastPost = millis();
  }
}
