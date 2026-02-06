/*
 * ============================================================================
 * Cardputer ADV Keyboard Diagnostic Tool
 * ============================================================================
 * Companion to: "Definitive Keyboard Technical Reference v1.0"
 * 
 * Purpose: Dumps the complete KeysState struct for every keypress/combo
 *          to both screen and Serial (115200 baud) for analysis.
 *          Used to empirically verify all claims in the reference doc.
 *
 * Output format per keypress:
 *   P[n]: (x,y) ...        — Raw matrix positions of pressed keys
 *   F: TAB FN SHF ...      — Active boolean flags + modifier bitmask
 *   W[n]: 'a' 'b' ...      — word[] contents (resolved printable chars)
 *   H[n]: 0x04 0x05 ...    — hid_keys[] contents (HID usage IDs)
 *   M[n]: 0x80 0x81 ...    — modifier_keys[] (only if modifiers pressed)
 *   CAPS: ON               — (only if caps lock active)
 *
 * What to test:
 *   1. Every single key individually (verify word[], hid_keys[], booleans)
 *   2. Shift + every key (verify shifted chars in word[])
 *   3. Fn + every key (verify fn=true, what appears in word[]/hid_keys[])
 *   4. Fn + ; , . / (arrow key positions — what do we get?)
 *   5. Fn + backspace (DEL? what changes?)
 *   6. Fn + backtick (ESC? or just fn + `)
 *   7. Fn + 1-0 (F-keys? or just fn + number)
 *   8. Ctrl + keys, Alt + keys, Opt alone
 *   9. Caps Lock: press Fn+Shift to toggle, then type
 *  10. Multi-key simultaneous press (rollover test — 10+ confirmed)
 *  11. Modifier stacking (Fn+Shift+key, Ctrl+Alt+key, etc.)
 *
 * Controls:
 *   Fn+Shift = Toggle caps lock (for testing)
 *   All output goes to Serial AND screen
 * ============================================================================
 */

#include <M5Cardputer.h>

M5Canvas canvas(&M5Cardputer.Display);

// Scrolling log buffer
#define LOG_LINES 16
String logLines[LOG_LINES];
int logHead = 0;
int logCount = 0;

void logPrint(const String& line) {
    Serial.println(line);
    logLines[logHead] = line;
    logHead = (logHead + 1) % LOG_LINES;
    if (logCount < LOG_LINES) logCount++;
}

void drawLog() {
    canvas.fillScreen(TFT_BLACK);
    canvas.setTextColor(TFT_GREEN);
    canvas.setTextSize(1);
    canvas.setTextDatum(TL_DATUM);

    // Header
    canvas.setTextColor(TFT_YELLOW);
    canvas.drawString("KB DIAG | Fn+Shift=CapsToggle", 0, 0);
    canvas.setTextColor(TFT_GREEN);

    int startIdx = (logCount < LOG_LINES) ? 0 : logHead;
    int y = 10;
    for (int i = 0; i < logCount && y < 130; i++) {
        int idx = (startIdx + i) % LOG_LINES;
        canvas.drawString(logLines[idx], 0, y);
        y += 8;
    }

    canvas.pushSprite(0, 0);
}

String hexByte(uint8_t v) {
    char buf[5];
    snprintf(buf, sizeof(buf), "0x%02X", v);
    return String(buf);
}

String printableChar(char c) {
    if (c >= 32 && c <= 126) {
        char buf[4];
        snprintf(buf, sizeof(buf), "'%c'", c);
        return String(buf);
    } else {
        return hexByte((uint8_t)c);
    }
}

void dumpKeysState(Keyboard_Class::KeysState& s) {
    // Line 1: Boolean flags
    String flags = "F:";
    if (s.tab)   flags += "TAB ";
    if (s.fn)    flags += "FN ";
    if (s.shift) flags += "SHF ";
    if (s.ctrl)  flags += "CTL ";
    if (s.opt)   flags += "OPT ";
    if (s.alt)   flags += "ALT ";
    if (s.del)   flags += "DEL ";
    if (s.enter) flags += "ENT ";
    if (s.space) flags += "SPC ";
    if (flags == "F:") flags += "(none)";
    flags += " mod=" + hexByte(s.modifiers);
    logPrint(flags);

    // Line 2: word[] contents
    String wordStr = "W[" + String(s.word.size()) + "]:";
    for (auto c : s.word) {
        wordStr += printableChar(c) + " ";
    }
    if (s.word.empty()) wordStr += "(empty)";
    logPrint(wordStr);

    // Line 3: hid_keys[] contents
    String hidStr = "H[" + String(s.hid_keys.size()) + "]:";
    for (auto k : s.hid_keys) {
        hidStr += hexByte(k) + " ";
    }
    if (s.hid_keys.empty()) hidStr += "(empty)";
    logPrint(hidStr);

    // Line 4: modifier_keys[] contents
    if (!s.modifier_keys.empty()) {
        String modStr = "M[" + String(s.modifier_keys.size()) + "]:";
        for (auto k : s.modifier_keys) {
            modStr += hexByte(k) + " ";
        }
        logPrint(modStr);
    }

    // Line 5: Caps lock state
    if (M5Cardputer.Keyboard.capslocked()) {
        logPrint("CAPS: ON");
    }

    logPrint("---");
}

void setup() {
    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true);
    Serial.begin(115200);

    M5Cardputer.Display.setRotation(1);
    canvas.createSprite(240, 135);

    logPrint("Cardputer ADV KB Diag");
    logPrint("Press any key/combo...");
    logPrint("Fn+Shift = toggle caps");
    logPrint("Serial @ 115200");
    logPrint("---");
    drawLog();
}

bool prevFn = false;
bool prevShift = false;

void loop() {
    M5Cardputer.update();

    if (M5Cardputer.Keyboard.isChange()) {
        if (M5Cardputer.Keyboard.isPressed()) {
            Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();

            // Fn+Shift = toggle caps lock
            if (status.fn && status.shift && !(prevFn && prevShift)) {
                bool caps = !M5Cardputer.Keyboard.capslocked();
                M5Cardputer.Keyboard.setCapsLocked(caps);
                logPrint(caps ? ">>> CAPS LOCK ON" : ">>> CAPS LOCK OFF");
            }

            prevFn = status.fn;
            prevShift = status.shift;

            // Also dump raw key positions for rollover analysis
            const auto& keyList = M5Cardputer.Keyboard.keyList();
            String posStr = "P[" + String(keyList.size()) + "]:";
            for (const auto& p : keyList) {
                posStr += "(" + String(p.x) + "," + String(p.y) + ") ";
            }
            logPrint(posStr);

            dumpKeysState(status);
            drawLog();
        } else {
            // All keys released
            prevFn = false;
            prevShift = false;
        }
    }

    delay(5);
}