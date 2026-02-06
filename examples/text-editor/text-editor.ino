/*
 * ============================================================================
 * Cardputer ADV Text Editor — Keyboard Reference Validation
 * ============================================================================
 * Purpose: Validates every keyboard function documented in the Definitive
 *          Keyboard Technical Reference v1.0. If this editor works correctly,
 *          the reference is proven complete and accurate. NOTE: This is *not*
 *          a functional text editor. It is a keyboard validation tool only.
 *
 * Features tested:
 *   ✅ All printable characters (a-z, 0-9, symbols)
 *   ✅ Shift layer (uppercase, shifted symbols)
 *   ✅ Ctrl+key shortcuts (Ctrl forces uppercase — uses tolower)
 *   ✅ Fn+arrow keys (;=↑ ,=← .=↓ /=→)
 *   ✅ Fn+backtick = ESC (clears selection / exits)
 *   ✅ Fn+Backspace = forward delete
 *   ✅ Fn+1-0 = F1-F10 (display notification)
 *   ✅ Tab key (inserts 4 spaces)
 *   ✅ Enter key (newline)
 *   ✅ Backspace (delete behind cursor)
 *   ✅ Space (in word + space boolean)
 *   ✅ Caps Lock toggle (Fn+Shift)
 *   ✅ Opt key (toggles line numbers)
 *   ✅ Alt key (detected, shown in status)
 *
 * Controls:
 *   Typing      : All printable keys work as labeled
 *   Arrows      : Fn + ; , . / = Up Left Down Right
 *   Backspace   : Delete character behind cursor
 *   Fn+Bksp     : Delete character ahead of cursor (forward delete)
 *   Enter       : New line
 *   Tab         : Insert 4 spaces
 *   Fn+`        : ESC — clear notification / deselect
 *   Fn+Shift    : Toggle Caps Lock
 *   Fn+1-0      : F1-F10 (shows notification)
 *   Ctrl+S      : Save notification
 *   Ctrl+Z      : Undo last action
 *   Ctrl+A      : Select all (visual indicator)
 *   Opt         : Toggle line numbers
 *   Alt         : Shown in status bar when held
 *
 * ============================================================================
 */

#include <M5Cardputer.h>

// ============================================================================
// CONFIGURATION
// ============================================================================
#define MAX_LINES 100
#define MAX_LINE_LEN 256
#define VISIBLE_LINES 13
#define CHARS_PER_LINE 38
#define TAB_WIDTH 4
#define UNDO_STACK_SIZE 20
#define NOTIFICATION_MS 1500

// ============================================================================
// DATA STRUCTURES
// ============================================================================
struct EditorState {
    String lines[MAX_LINES];
    int lineCount;
    int cursorLine;
    int cursorCol;
    int scrollY;
    int scrollX;
    bool capsLock;
    bool selectAll;
    bool showLineNumbers;
    String notification;
    uint32_t notificationTime;
};

struct UndoEntry {
    String lines[MAX_LINES];
    int lineCount;
    int cursorLine;
    int cursorCol;
    bool valid;
};

// ============================================================================
// GLOBALS
// ============================================================================
M5Canvas canvas(&M5Cardputer.Display);
EditorState ed;
UndoEntry undoStack[UNDO_STACK_SIZE];
int undoHead = 0;
int undoCount = 0;
bool prevFn = false;
bool prevShift = false;
bool needsRedraw = true;

// ============================================================================
// UNDO SYSTEM
// ============================================================================
void pushUndo() {
    UndoEntry* u = &undoStack[undoHead];
    u->lineCount = ed.lineCount;
    u->cursorLine = ed.cursorLine;
    u->cursorCol = ed.cursorCol;
    for (int i = 0; i < ed.lineCount; i++) {
        u->lines[i] = ed.lines[i];
    }
    u->valid = true;
    undoHead = (undoHead + 1) % UNDO_STACK_SIZE;
    if (undoCount < UNDO_STACK_SIZE) undoCount++;
}

void popUndo() {
    if (undoCount == 0) {
        ed.notification = "Nothing to undo";
        ed.notificationTime = millis();
        return;
    }
    undoHead = (undoHead - 1 + UNDO_STACK_SIZE) % UNDO_STACK_SIZE;
    undoCount--;
    UndoEntry* u = &undoStack[undoHead];
    if (!u->valid) return;
    ed.lineCount = u->lineCount;
    ed.cursorLine = u->cursorLine;
    ed.cursorCol = u->cursorCol;
    for (int i = 0; i < ed.lineCount; i++) {
        ed.lines[i] = u->lines[i];
    }
    ed.notification = "Undo";
    ed.notificationTime = millis();
}

// ============================================================================
// CURSOR MOVEMENT
// ============================================================================
void cursorLeft() {
    if (ed.cursorCol > 0) {
        ed.cursorCol--;
    } else if (ed.cursorLine > 0) {
        ed.cursorLine--;
        ed.cursorCol = ed.lines[ed.cursorLine].length();
    }
}

void cursorRight() {
    if (ed.cursorCol < (int)ed.lines[ed.cursorLine].length()) {
        ed.cursorCol++;
    } else if (ed.cursorLine < ed.lineCount - 1) {
        ed.cursorLine++;
        ed.cursorCol = 0;
    }
}

void cursorUp() {
    if (ed.cursorLine > 0) {
        ed.cursorLine--;
        if (ed.cursorCol > (int)ed.lines[ed.cursorLine].length()) {
            ed.cursorCol = ed.lines[ed.cursorLine].length();
        }
    }
}

void cursorDown() {
    if (ed.cursorLine < ed.lineCount - 1) {
        ed.cursorLine++;
        if (ed.cursorCol > (int)ed.lines[ed.cursorLine].length()) {
            ed.cursorCol = ed.lines[ed.cursorLine].length();
        }
    }
}

void ensureCursorVisible() {
    if (ed.cursorLine < ed.scrollY) ed.scrollY = ed.cursorLine;
    if (ed.cursorLine >= ed.scrollY + VISIBLE_LINES) ed.scrollY = ed.cursorLine - VISIBLE_LINES + 1;

    int leftMargin = ed.showLineNumbers ? 4 : 0;
    int visibleChars = CHARS_PER_LINE - leftMargin;
    if (ed.cursorCol < ed.scrollX) ed.scrollX = ed.cursorCol;
    if (ed.cursorCol >= ed.scrollX + visibleChars) ed.scrollX = ed.cursorCol - visibleChars + 1;
}

// ============================================================================
// TEXT EDITING
// ============================================================================
void insertChar(char c) {
    pushUndo();
    String& line = ed.lines[ed.cursorLine];
    if (ed.cursorCol >= (int)line.length()) {
        line += c;
    } else {
        line = line.substring(0, ed.cursorCol) + String(c) + line.substring(ed.cursorCol);
    }
    ed.cursorCol++;
}

void handleBackspace() {
    if (ed.cursorCol > 0) {
        pushUndo();
        String& line = ed.lines[ed.cursorLine];
        line = line.substring(0, ed.cursorCol - 1) + line.substring(ed.cursorCol);
        ed.cursorCol--;
    } else if (ed.cursorLine > 0) {
        pushUndo();
        ed.cursorCol = ed.lines[ed.cursorLine - 1].length();
        ed.lines[ed.cursorLine - 1] += ed.lines[ed.cursorLine];
        for (int i = ed.cursorLine; i < ed.lineCount - 1; i++) {
            ed.lines[i] = ed.lines[i + 1];
        }
        ed.lines[ed.lineCount - 1] = "";
        ed.lineCount--;
        ed.cursorLine--;
    }
}

void handleForwardDelete() {
    String& line = ed.lines[ed.cursorLine];
    if (ed.cursorCol < (int)line.length()) {
        pushUndo();
        line = line.substring(0, ed.cursorCol) + line.substring(ed.cursorCol + 1);
    } else if (ed.cursorLine < ed.lineCount - 1) {
        pushUndo();
        ed.lines[ed.cursorLine] += ed.lines[ed.cursorLine + 1];
        for (int i = ed.cursorLine + 1; i < ed.lineCount - 1; i++) {
            ed.lines[i] = ed.lines[i + 1];
        }
        ed.lines[ed.lineCount - 1] = "";
        ed.lineCount--;
    }
}

void handleEnter() {
    if (ed.lineCount >= MAX_LINES) return;
    pushUndo();
    String& line = ed.lines[ed.cursorLine];
    String remainder = line.substring(ed.cursorCol);
    line = line.substring(0, ed.cursorCol);

    for (int i = ed.lineCount; i > ed.cursorLine + 1; i--) {
        ed.lines[i] = ed.lines[i - 1];
    }
    ed.cursorLine++;
    ed.lines[ed.cursorLine] = remainder;
    ed.lineCount++;
    ed.cursorCol = 0;
}

void handleTab() {
    for (int i = 0; i < TAB_WIDTH; i++) {
        insertChar(' ');
    }
}

void handleEscape() {
    if (ed.selectAll) {
        ed.selectAll = false;
        ed.notification = "Selection cleared";
    } else if (ed.notification.length() > 0) {
        ed.notification = "";
    } else {
        ed.notification = "ESC";
    }
    ed.notificationTime = millis();
}

void handleFKey(int num) {
    ed.notification = "F" + String(num);
    ed.notificationTime = millis();
}

void handleSave() {
    ed.notification = "Saved! (Ctrl+S)";
    ed.notificationTime = millis();
}

void handleSelectAll() {
    ed.selectAll = true;
    ed.notification = "Select All (Ctrl+A)";
    ed.notificationTime = millis();
}

void handleCopy() {
    ed.notification = "Copy (Ctrl+C)";
    ed.notificationTime = millis();
}

void handlePaste() {
    ed.notification = "Paste (Ctrl+V)";
    ed.notificationTime = millis();
}

// ============================================================================
// DISPLAY
// ============================================================================
void drawEditor() {
    canvas.fillScreen(TFT_BLACK);
    canvas.setTextSize(1);
    canvas.setTextDatum(TL_DATUM);

    int leftMargin = ed.showLineNumbers ? 24 : 0;
    int charW = 6;
    int lineH = 9;
    int textStartY = 1;

    // Draw text lines
    for (int i = 0; i < VISIBLE_LINES && (i + ed.scrollY) < ed.lineCount; i++) {
        int lineIdx = i + ed.scrollY;
        int y = textStartY + i * lineH;

        // Line numbers
        if (ed.showLineNumbers) {
            canvas.setTextColor(TFT_DARKGREY);
            char numBuf[5];
            snprintf(numBuf, sizeof(numBuf), "%3d", lineIdx + 1);
            canvas.drawString(numBuf, 0, y);
        }

        // Line text
        String& line = ed.lines[lineIdx];
        int startCol = ed.scrollX;
        int maxChars = (240 - leftMargin) / charW;

        if (startCol < (int)line.length()) {
            String visible = line.substring(startCol, startCol + maxChars);

            if (ed.selectAll) {
                canvas.setTextColor(TFT_BLACK);
                canvas.fillRect(leftMargin, y, visible.length() * charW, lineH, TFT_BLUE);
            } else {
                canvas.setTextColor(TFT_GREEN);
            }
            canvas.drawString(visible, leftMargin, y);
        }

        // Cursor
        if (lineIdx == ed.cursorLine) {
            int cursorX = leftMargin + (ed.cursorCol - ed.scrollX) * charW;
            if (cursorX >= leftMargin && cursorX < 240) {
                // Blinking cursor
                if ((millis() / 500) % 2 == 0) {
                    canvas.drawLine(cursorX, y, cursorX, y + lineH - 1, TFT_WHITE);
                }
            }
        }
    }

    // Status bar
    int statusY = 126;
    canvas.fillRect(0, statusY - 1, 240, 10, canvas.color565(30, 30, 60));
    canvas.setTextColor(TFT_WHITE);

    // Left: position
    String pos = "L" + String(ed.cursorLine + 1) + ":" + String(ed.cursorCol + 1);
    canvas.drawString(pos, 2, statusY);

    // Center: notification or mode indicators
    String center = "";
    if (ed.notification.length() > 0 && millis() - ed.notificationTime < NOTIFICATION_MS) {
        center = ed.notification;
        canvas.setTextColor(TFT_YELLOW);
    } else {
        ed.notification = "";
        if (ed.capsLock) center += "CAPS ";
        if (ed.showLineNumbers) center += "LN ";
        canvas.setTextColor(TFT_CYAN);
    }
    canvas.setTextDatum(TC_DATUM);
    canvas.drawString(center, 120, statusY);

    // Right: line count
    canvas.setTextDatum(TR_DATUM);
    canvas.setTextColor(TFT_WHITE);
    canvas.drawString(String(ed.lineCount) + "L", 238, statusY);

    canvas.pushSprite(0, 0);
}

// ============================================================================
// SETUP
// ============================================================================
void setup() {
    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true);
    Serial.begin(115200);

    M5Cardputer.Display.setRotation(1);
    canvas.createSprite(240, 135);

    // Init editor state
    ed.lineCount = 1;
    ed.lines[0] = "";
    ed.cursorLine = 0;
    ed.cursorCol = 0;
    ed.scrollY = 0;
    ed.scrollX = 0;
    ed.capsLock = false;
    ed.selectAll = false;
    ed.showLineNumbers = false;
    ed.notification = "Ready — type away!";
    ed.notificationTime = millis();

    drawEditor();
}

// ============================================================================
// MAIN LOOP
// ============================================================================
void loop() {
    M5Cardputer.update();

    if (M5Cardputer.Keyboard.isChange()) {
        if (M5Cardputer.Keyboard.isPressed()) {
            Keyboard_Class::KeysState s = M5Cardputer.Keyboard.keysState();

            // === Caps Lock toggle (Fn+Shift) ===
            if (s.fn && s.shift && !(prevFn && prevShift)) {
                ed.capsLock = !ed.capsLock;
                M5Cardputer.Keyboard.setCapsLocked(ed.capsLock);
                ed.notification = ed.capsLock ? "CAPS ON" : "CAPS OFF";
                ed.notificationTime = millis();
            }
            prevFn = s.fn;
            prevShift = s.shift;

            // === Fn layer: arrows, ESC, forward delete, F-keys ===
            if (s.fn) {
                for (auto c : s.word) {
                    switch (c) {
                        case ';': cursorUp();     break;
                        case ',': cursorLeft();   break;
                        case '.': cursorDown();   break;
                        case '/': cursorRight();  break;
                        case '`': handleEscape(); break;
                        default:
                            // F-keys: Fn + 1-9 = F1-F9, Fn + 0 = F10
                            if (c >= '1' && c <= '9') handleFKey(c - '0');
                            else if (c == '0') handleFKey(10);
                            break;
                    }
                }
                // Fn+Backspace = forward delete
                if (s.del) handleForwardDelete();

                ensureCursorVisible();
                needsRedraw = true;
                goto done;  // Skip normal processing when Fn is held
            }

            // === Ctrl shortcuts ===
            if (s.ctrl) {
                for (auto c : s.word) {
                    // ⚠️ Ctrl forces uppercase in word — normalize with tolower
                    switch (tolower(c)) {
                        case 's': handleSave();      break;
                        case 'z': popUndo();         break;
                        case 'a': handleSelectAll(); break;
                        case 'c': handleCopy();      break;
                        case 'v': handlePaste();     break;
                    }
                }
                needsRedraw = true;
                goto done;  // Skip normal text input when Ctrl is held
            }

            // === Alt: just show indicator ===
            if (s.alt) {
                ed.notification = "ALT held";
                ed.notificationTime = millis();
            }

            // === Opt: toggle line numbers ===
            if (s.opt) {
                ed.showLineNumbers = !ed.showLineNumbers;
                ed.notification = ed.showLineNumbers ? "Line numbers ON" : "Line numbers OFF";
                ed.notificationTime = millis();
            }

            // === Special keys ===
            if (s.del)   { handleBackspace(); }
            if (s.enter) { handleEnter(); }
            if (s.tab)   { handleTab(); }

            // === Printable text input ===
            // Characters in word are already resolved by the library
            // (lowercase, uppercase via shift/ctrl/caps — all handled)
            for (auto c : s.word) {
                if (ed.selectAll) {
                    // Replace all content on first keystroke after select-all
                    pushUndo();
                    ed.lineCount = 1;
                    ed.lines[0] = "";
                    ed.cursorLine = 0;
                    ed.cursorCol = 0;
                    ed.selectAll = false;
                }
                insertChar(c);
            }

            ensureCursorVisible();
            needsRedraw = true;

            done:;

        } else {
            // All keys released
            prevFn = false;
            prevShift = false;
        }
    }

    // Notification expiry triggers redraw
    if (ed.notification.length() > 0 && millis() - ed.notificationTime >= NOTIFICATION_MS) {
        ed.notification = "";
        needsRedraw = true;
    }

    // Cursor blink
    static uint32_t lastBlink = 0;
    if (millis() - lastBlink >= 500) {
        lastBlink = millis();
        needsRedraw = true;
    }

    if (needsRedraw) {
        drawEditor();
        needsRedraw = false;
    }

    delay(5);
}