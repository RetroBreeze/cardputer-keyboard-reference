# M5Stack Cardputer ADV — Definitive Keyboard Technical Reference v1.0

**For AI agents and developers implementing keyboard input in Arduino/C++ projects.**
All data verified from M5Cardputer library source code (v1.1.1+) and empirical testing on hardware (February 2026).

Note: This project was created in collaboration with Anthropic Claude Opus 4.6.

---

## 1. HARDWARE ARCHITECTURE

The Cardputer ADV uses a **TCA8418** I²C keypad scan controller (replacing the original Cardputer's GPIO matrix).

| Parameter | Value |
|---|---|
| Scanning IC | TCA8418RTWR (Texas Instruments) |
| I²C address | 0x34 (7-bit, fixed) |
| I²C SDA / SCL | GPIO8 / GPIO9 |
| Interrupt pin | GPIO11 (active-low) |
| Electrical matrix | 7 rows × 8 columns |
| Physical layout | 4 rows × 14 columns (56 keys) |
| N-key rollover | **10+ keys confirmed** (tested simultaneously) |
| Library | M5Cardputer v1.1.1+ (auto-detects ADV vs original) |

The TCA8418 scans autonomously and fires an interrupt on key events. The library maintains a running list of pressed keys (add on press, remove on release). Unlike the original Cardputer's GPIO scanning, rollover is NOT limited to 3 keys — the ADV hardware supports at least 10 simultaneous non-modifier keys with no ghosting on the home row.

The same library API works on both original Cardputer and Cardputer ADV. The hardware variant is detected at runtime in `Keyboard_Class::begin()` via `M5.getBoard()`.

---

## 2. INITIALIZATION

```cpp
#include <M5Cardputer.h>

void setup() {
    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true);  // true = enable keyboard
    // Keyboard is now ready. No additional init needed.
}
```

### Required library versions

| Library | Minimum version |
|---|---|
| M5Cardputer | **1.1.1** (1.1.0 has z/c/b/m uppercase bug) |
| M5Unified | 0.2.8+ |
| M5GFX | 0.2.10+ |

---

## 3. CORE API — THE POLLING LOOP

Every project MUST follow this pattern:

```cpp
void loop() {
    M5Cardputer.update();                          // Step 1: poll hardware

    if (M5Cardputer.Keyboard.isChange()) {         // Step 2: state changed?
        if (M5Cardputer.Keyboard.isPressed()) {    // Step 3: key(s) down?
            Keyboard_Class::KeysState status =
                M5Cardputer.Keyboard.keysState();  // Step 4: get full state

            // ... process status ...
        } else {
            // All keys released
        }
    }
    delay(5);  // Prevent tight-loop; keep short to avoid missing events
}
```

**Critical**: `M5Cardputer.update()` must be called frequently. Long `delay()` or blocking I/O will cause the TCA8418's 10-event FIFO to overflow, silently losing keystrokes.

### API methods

| Method | Returns | Description |
|---|---|---|
| `M5Cardputer.update()` | void | Polls keyboard hardware. Call every loop. |
| `.Keyboard.isChange()` | bool | True if key count changed since last call |
| `.Keyboard.isPressed()` | uint8_t | Count of keys currently pressed (0 = none) |
| `.Keyboard.keysState()` | KeysState& | Complete resolved keyboard state |
| `.Keyboard.isKeyPressed(char c)` | bool | Is specific key currently down? |
| `.Keyboard.keyList()` | vector<Point2D_t>& | Raw matrix positions of pressed keys |
| `.Keyboard.capslocked()` | bool | Current caps lock state |
| `.Keyboard.setCapsLocked(bool)` | void | Set caps lock on/off (app must manage this) |

---

## 4. KeysState STRUCT — COMPLETE DEFINITION

```cpp
struct KeysState {
    // === Boolean flags (one per special/modifier key) ===
    bool tab;        // Tab key pressed
    bool fn;         // Fn key pressed
    bool shift;      // Aa (Shift) key pressed
    bool ctrl;       // Ctrl key pressed
    bool opt;        // Opt key pressed
    bool alt;        // Alt key pressed
    bool del;        // Backspace key pressed
    bool enter;      // Enter/OK key pressed
    bool space;      // Space bar pressed

    // === HID modifier bitmask ===
    uint8_t modifiers;  // Standard USB HID modifier byte

    // === Key data vectors ===
    std::vector<char>    word;          // Printable characters (modifier-resolved)
    std::vector<uint8_t> hid_keys;      // HID usage IDs for non-modifier keys
    std::vector<uint8_t> modifier_keys; // KEY_* codes of pressed modifier keys
};
```

### What goes WHERE — the critical routing table

This table defines exactly where each physical key's data appears. Verified empirically.

| Physical key | Boolean flag | In `word`? | In `hid_keys`? | In `modifier_keys`? |
|---|---|---|---|---|
| **a–z** | — | ✅ `'a'`–`'z'` (or `'A'`–`'Z'` if shifted/ctrl/caps) | ✅ HID usage ID | — |
| **0–9** | — | ✅ `'0'`–`'9'` (or shifted symbol) | ✅ HID usage ID | — |
| **Symbols** (`` ` `` `-` `=` `[` `]` `\` `;` `'` `,` `.` `/`) | — | ✅ base or shifted char | ✅ HID usage ID | — |
| **Space** | `space=true` | ✅ `' '` | ✅ `0x2C` | — |
| **Tab** | `tab=true` | ❌ empty | ✅ `0x2B` | — |
| **Enter** | `enter=true` | ❌ empty | ✅ `0x28` | — |
| **Backspace** | `del=true` | ❌ empty | ✅ `0x2A` | — |
| **Fn** | `fn=true` | ❌ empty | ❌ empty | ❌ |
| **Aa (Shift)** | `shift=true` | ❌ empty | ❌ empty | ✅ `0x81` |
| **Ctrl** | `ctrl=true` | ❌ empty | ❌ empty | ✅ `0x80` |
| **Alt** | `alt=true` | ❌ empty | ❌ empty | ✅ `0x82` |
| **Opt** | `opt=true` | ❌ empty | ❌ empty | ❌ |

### modifiers bitmask values

| Modifier | Bit | Value when pressed alone |
|---|---|---|
| Ctrl | bit 0 | `0x01` |
| Shift | bit 1 | `0x02` |
| Alt | bit 2 | `0x04` |

Formula: `modifiers |= (1 << (KEY_CODE - 0x80))` where KEY_LEFT_CTRL=0x80, KEY_LEFT_SHIFT=0x81, KEY_LEFT_ALT=0x82.

**Fn and Opt do NOT set any modifier bits.** They are boolean flags only.

---

## 5. KEY CONSTANTS (Keyboard_def.h) — COMPLETE

```cpp
#define KEY_LEFT_CTRL  0x80
#define KEY_LEFT_SHIFT 0x81
#define KEY_LEFT_ALT   0x82
#define KEY_FN         0xFF
#define KEY_OPT        0x00   // ⚠️ Maps to NUL — see warning below
#define KEY_BACKSPACE  0x2A
#define KEY_TAB        0x2B
#define KEY_ENTER      0x28
```

**That's ALL of them.** There is no KEY_ESC, no KEY_UP_ARROW, no KEY_DELETE, no KEY_F1–F12. These do not exist in the library.

### Modifier stacking behavior (empirically confirmed)

When multiple modifiers are held simultaneously (e.g., Fn+Shift+key), all applicable modifier flags are set and the character in `word` is resolved by the Shift/Ctrl/Caps logic (value_second). Example: Fn+Shift+`;` produces `fn=true`, `shift=true`, word=`':'`, hid=`0x33`. Your application's Fn handler should take priority over text input (process the Fn layer first, skip normal text input). The shifted character in word is a side effect — ignore it when Fn is active.

### ⚠️ KEY_OPT = 0x00 WARNING

`KEY_OPT` is defined as `0x00` (NUL). This means:
- **Never use `isKeyPressed(KEY_OPT)`** — it will match zero/null entries unpredictably
- Always detect Opt via `status.opt` boolean
- Opt produces nothing in word, hid_keys, or modifier_keys

---

## 6. PHYSICAL LAYOUT AND KEYMAP

### Key value map (from source: `_key_value_map[4][14]`)

Each entry is `{value_first, value_second}` where value_first = base layer, value_second = shifted/ctrl/caps layer.

```
Row 0: {'`','~'} {'1','!'} {'2','@'} {'3','#'} {'4','$'} {'5','%'} {'6','^'} {'7','&'} {'8','*'} {'9','('} {'0',')'} {'-','_'} {'=','+'} {BKSP,BKSP}
Row 1: {TAB,TAB} {'q','Q'} {'w','W'} {'e','E'} {'r','R'} {'t','T'} {'y','Y'} {'u','U'} {'i','I'} {'o','O'} {'p','P'} {'[','{'} {']','}'} {'\\','|'}
Row 2: {FN,FN}   {SHF,SHF} {'a','A'} {'s','S'} {'d','D'} {'f','F'} {'g','G'} {'h','H'} {'j','J'} {'k','K'} {'l','L'} {';',':'} {'\'','"'} {ENT,ENT}
Row 3: {CTL,CTL} {OPT,OPT} {ALT,ALT} {'z','Z'} {'x','X'} {'c','C'} {'v','V'} {'b','B'} {'n','N'} {'m','M'} {',','<'} {'.','>'} {'/','?'} {' ',' '}
```

### `-` / `_` mapping is STANDARD

Base layer = `-`, Shift layer = `_`. This follows normal US keyboard convention. The keycap printing on the device shows `_` prominently, which can be misleading, but the firmware maps it conventionally: `-` unshifted, `_` shifted.

---

## 7. THE Fn LAYER — THE LIBRARY DOES NOTHING

**This is the single most important section.** The library sets `status.fn = true` when Fn is held and does absolutely nothing else. No remapping. No character substitution. No HID code translation.

When Fn + any key is pressed:
- `status.fn` = `true`
- `status.word` contains the **base layer character** (e.g., Fn+`;` → word contains `';'`)
- `status.hid_keys` contains the **base key's HID code** (e.g., Fn+`;` → hid contains `0x33`)
- No arrow codes, no F-key codes, no ESC code — ever

### Fn layer remapping — YOUR application must implement this

#### Arrow keys

The `;` `,` `.` `/` keys double as ↑ ← ↓ → when Fn is held (as printed on the keycaps).

```cpp
// Detect arrow keys from Fn + base key
if (status.fn) {
    for (auto hid : status.hid_keys) {
        switch (hid) {
            case 0x33: /* Fn+; = UP    */ handleArrow(UP);    break;
            case 0x36: /* Fn+, = LEFT  */ handleArrow(LEFT);  break;
            case 0x37: /* Fn+. = DOWN  */ handleArrow(DOWN);  break;
            case 0x38: /* Fn+/ = RIGHT */ handleArrow(RIGHT); break;
        }
    }
}
```

Or equivalently using `status.word`:

```cpp
if (status.fn) {
    for (auto c : status.word) {
        switch (c) {
            case ';': handleArrow(UP);    break;
            case ',': handleArrow(LEFT);  break;
            case '.': handleArrow(DOWN);  break;
            case '/': handleArrow(RIGHT); break;
        }
    }
}
```

#### ESC

There is no dedicated ESC key in the firmware. The top-left key produces `` ` `` (backtick). ESC is accessed as Fn + backtick:

```cpp
if (status.fn) {
    for (auto c : status.word) {
        if (c == '`') handleEscape();
    }
}
```

#### Forward Delete

Backspace is always Backspace. Fn + Backspace = forward delete:

```cpp
if (status.del) {
    if (status.fn) {
        handleForwardDelete();
    } else {
        handleBackspace();
    }
}
```

Note: when Fn+Backspace is pressed, `status.del` is still `true` and `status.fn` is also `true`. The HID code is still `0x2A` (backspace). The library does not change it to the HID DELETE code.

#### F-keys (F1–F10)

Fn + number keys 1–0 = F1–F10. The library gives you the base number character:

```cpp
if (status.fn) {
    for (auto c : status.word) {
        if (c >= '1' && c <= '9') {
            int fkey = c - '0';  // F1–F9
            handleFunctionKey(fkey);
        } else if (c == '0') {
            handleFunctionKey(10);  // F10
        }
    }
}
```

#### USB HID output with Fn remapping

For projects that output USB HID (e.g., using the device as a USB keyboard), you must remap HID codes when Fn is held:

```cpp
if (status.fn) {
    for (auto& hid : status.hid_keys) {
        switch (hid) {
            // Arrow keys
            case 0x33: hid = 0x52; break;  // ; → HID Up Arrow
            case 0x36: hid = 0x50; break;  // , → HID Left Arrow
            case 0x37: hid = 0x51; break;  // . → HID Down Arrow
            case 0x38: hid = 0x4F; break;  // / → HID Right Arrow
            // F-keys
            case 0x1E: hid = 0x3A; break;  // 1 → F1
            case 0x1F: hid = 0x3B; break;  // 2 → F2
            case 0x20: hid = 0x3C; break;  // 3 → F3
            case 0x21: hid = 0x3D; break;  // 4 → F4
            case 0x22: hid = 0x3E; break;  // 5 → F5
            case 0x23: hid = 0x3F; break;  // 6 → F6
            case 0x24: hid = 0x40; break;  // 7 → F7
            case 0x25: hid = 0x41; break;  // 8 → F8
            case 0x26: hid = 0x42; break;  // 9 → F9
            case 0x27: hid = 0x43; break;  // 0 → F10
            // ESC
            case 0x35: hid = 0x29; break;  // ` → Escape
            // Delete (forward)
            case 0x2A: hid = 0x4C; break;  // Backspace → Delete
        }
    }
}
```

---

## 8. SHIFT/CTRL LAYER BEHAVIOR

The library resolves `value_second` (the shifted character) when **any** of these conditions are true:
- `status.shift` (Aa key held)
- `status.ctrl` (Ctrl key held)
- Caps Lock is enabled via `setCapsLocked(true)`

This means **Ctrl+a produces `'A'` in `status.word`**, not `'a'`. This is by design in the source code:

```cpp
if (_keys_state_buffer.ctrl || _keys_state_buffer.shift || _is_caps_locked) {
    _keys_state_buffer.word.push_back(key_value.value_second);
} else {
    _keys_state_buffer.word.push_back(key_value.value_first);
}
```

### Implications for Ctrl+key shortcuts

If your app checks for Ctrl+C by looking for `status.ctrl && word contains 'c'`, it will **never match** because Ctrl forces uppercase. You must check for uppercase:

```cpp
// WRONG — will never match
if (status.ctrl) {
    for (auto c : status.word) {
        if (c == 'c') handleCopy();  // ❌ Never fires
    }
}

// CORRECT
if (status.ctrl) {
    for (auto c : status.word) {
        if (c == 'C') handleCopy();  // ✅ Works
    }
}

// ALSO CORRECT — normalize with tolower()
if (status.ctrl) {
    for (auto c : status.word) {
        if (tolower(c) == 'c') handleCopy();  // ✅ Works
    }
}
```

This applies to ALL Ctrl+key combinations. It also applies to symbols: Ctrl+`-` produces `_`, Ctrl+`=` produces `+`, Ctrl+`;` produces `:`, etc.

---

## 9. CAPS LOCK — APPLICATION MANAGED

The library does **not** auto-toggle caps lock. The Aa key is a momentary shift — it only works while held. To implement caps lock toggle:

```cpp
// Example: Fn+Shift toggles caps lock
static bool prevFn = false, prevShift = false;

if (status.fn && status.shift && !(prevFn && prevShift)) {
    bool caps = !M5Cardputer.Keyboard.capslocked();
    M5Cardputer.Keyboard.setCapsLocked(caps);
}
prevFn = status.fn;
prevShift = status.shift;
```

When caps lock is on, all letter keys produce uppercase in `status.word` (value_second). The `capslocked()` method returns the current state.

---

## 10. isKeyPressed() REFERENCE

```cpp
bool isKeyPressed(char c);
```

Accepts printable ASCII characters and KEY_* constants. Checks the raw matrix — does NOT account for modifiers.

| Argument | Works? | Notes |
|---|---|---|
| `'a'` through `'z'` | ✅ | Always checks base layer value |
| `'A'` through `'Z'` | ✅ | Checks shifted layer value |
| `'0'` through `'9'` | ✅ | |
| `' '` (space) | ✅ | |
| `';'` `','` `'.'` `'/'` etc. | ✅ | |
| `KEY_LEFT_SHIFT` (0x81) | ✅ | |
| `KEY_LEFT_CTRL` (0x80) | ✅ | |
| `KEY_LEFT_ALT` (0x82) | ✅ | |
| `KEY_BACKSPACE` (0x2A) | ✅ | |
| `KEY_TAB` (0x2B) | ✅ | |
| `KEY_ENTER` (0x28) | ✅ | |
| `KEY_FN` (0xFF) | ✅ | |
| `KEY_OPT` (0x00) | ⚠️ **AVOID** | Maps to NUL — unreliable matching |

---

## 11. HID USAGE ID LOOKUP TABLE

For USB HID output or Fn-layer remapping. These are the values that appear in `status.hid_keys`.

### Printable keys

| Char | HID | Char | HID | Char | HID |
|---|---|---|---|---|---|
| a | 0x04 | n | 0x11 | 0 | 0x27 |
| b | 0x05 | o | 0x12 | - | 0x2D |
| c | 0x06 | p | 0x13 | = | 0x2E |
| d | 0x07 | q | 0x14 | [ | 0x2F |
| e | 0x08 | r | 0x15 | ] | 0x30 |
| f | 0x09 | s | 0x16 | \ | 0x31 |
| g | 0x0A | t | 0x17 | ; | 0x33 |
| h | 0x0B | u | 0x18 | ' | 0x34 |
| i | 0x0C | v | 0x19 | ` | 0x35 |
| j | 0x0D | w | 0x1A | , | 0x36 |
| k | 0x0E | x | 0x1B | . | 0x37 |
| l | 0x0F | y | 0x1C | / | 0x38 |
| m | 0x10 | z | 0x1D | space | 0x2C |
| 1 | 0x1E | 4 | 0x21 | 7 | 0x24 |
| 2 | 0x1F | 5 | 0x22 | 8 | 0x25 |
| 3 | 0x20 | 6 | 0x23 | 9 | 0x26 |

Note: When shift/ctrl/caps is active, the library still puts the **base key's HID code** in `hid_keys` (e.g., Shift+a → hid_keys contains `0x04`, not `0x84`). The SHIFT bit is conveyed separately via `status.modifiers`.

### Special keys

| Key | HID in hid_keys |
|---|---|
| Backspace | 0x2A |
| Tab | 0x2B |
| Enter | 0x28 |

### Fn-remapped HID codes (for USB HID output)

| Fn + key | Target HID | Description |
|---|---|---|
| Fn + `;` (0x33) | 0x52 | Up Arrow |
| Fn + `,` (0x36) | 0x50 | Left Arrow |
| Fn + `.` (0x37) | 0x51 | Down Arrow |
| Fn + `/` (0x38) | 0x4F | Right Arrow |
| Fn + `1` (0x1E) | 0x3A | F1 |
| Fn + `2` (0x1F) | 0x3B | F2 |
| Fn + `3` (0x20) | 0x3C | F3 |
| Fn + `4` (0x21) | 0x3D | F4 |
| Fn + `5` (0x22) | 0x3E | F5 |
| Fn + `6` (0x23) | 0x3F | F6 |
| Fn + `7` (0x24) | 0x40 | F7 |
| Fn + `8` (0x25) | 0x41 | F8 |
| Fn + `9` (0x26) | 0x42 | F9 |
| Fn + `0` (0x27) | 0x43 | F10 |
| Fn + `` ` `` (0x35) | 0x29 | Escape |
| Fn + Bksp (0x2A) | 0x4C | Delete (forward) |

---

## 12. COMPLETE TEXT INPUT TEMPLATE

Copy-paste starting point for any text input application:

```cpp
#include <M5Cardputer.h>

String textBuffer = "";
int cursorPos = 0;
bool capsLock = false;

void setup() {
    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true);
    M5Cardputer.Display.setRotation(1);
}

// Track previous modifier state for toggle detection
bool prevFn = false, prevShift = false;

void loop() {
    M5Cardputer.update();

    if (M5Cardputer.Keyboard.isChange()) {
        if (M5Cardputer.Keyboard.isPressed()) {
            Keyboard_Class::KeysState s = M5Cardputer.Keyboard.keysState();

            // --- Caps Lock toggle (Fn+Shift) ---
            if (s.fn && s.shift && !(prevFn && prevShift)) {
                capsLock = !capsLock;
                M5Cardputer.Keyboard.setCapsLocked(capsLock);
            }
            prevFn = s.fn;
            prevShift = s.shift;

            // --- Fn layer: arrows, ESC, delete, F-keys ---
            if (s.fn) {
                for (auto c : s.word) {
                    switch (c) {
                        case ';': cursorUp();     continue;
                        case ',': cursorLeft();   continue;
                        case '.': cursorDown();   continue;
                        case '/': cursorRight();  continue;
                        case '`': handleEscape(); continue;
                    }
                    // F-keys
                    if (c >= '1' && c <= '9') { handleFKey(c - '0'); continue; }
                    if (c == '0') { handleFKey(10); continue; }
                }
                // Fn+Backspace = forward delete
                if (s.del) { handleForwardDelete(); }
                continue;  // Skip normal processing when Fn is held
            }

            // --- Ctrl shortcuts ---
            if (s.ctrl) {
                for (auto c : s.word) {
                    switch (tolower(c)) {  // ⚠️ Ctrl forces uppercase — normalize
                        case 's': handleSave();   break;
                        case 'z': handleUndo();   break;
                        case 'c': handleCopy();   break;
                        case 'v': handlePaste();  break;
                        case 'a': handleSelectAll(); break;
                    }
                }
                continue;  // Skip normal text input when Ctrl is held
            }

            // --- Special keys ---
            if (s.del)   { handleBackspace(); }
            if (s.enter) { handleEnter(); }
            if (s.tab)   { handleTab(); }

            // --- Printable text input ---
            for (auto c : s.word) {
                insertCharacter(c);  // Characters already resolved by library
            }

        } else {
            // All keys released
            prevFn = false;
            prevShift = false;
        }
    }

    updateDisplay();
    delay(5);
}
```

---

## 13. COMMON GOTCHAS SUMMARY

| # | Gotcha | Details |
|---|---|---|
| 1 | **Library v1.1.1 minimum** | v1.1.0 has broken uppercase for z, c, b, m |
| 2 | **Fn layer is 100% your job** | Library only sets `status.fn = true`. No remapping. |
| 3 | **No KEY_ESC constant exists** | Top-left key is backtick, not ESC. Use Fn+backtick. |
| 4 | **Ctrl forces uppercase in word** | Ctrl+a → word contains `'A'`. Use `tolower()`. |
| 5 | **KEY_OPT = 0x00 (NUL)** | Never use with `isKeyPressed()`. Use `status.opt` only. |
| 6 | **Tab/Enter/Backspace not in word** | These only set boolean flags + appear in hid_keys. |
| 7 | **Space IS in word** | Space appears as `' '` in word AND sets `status.space`. |
| 8 | **Caps lock is app-managed** | Library does not auto-toggle. Call `setCapsLocked()`. |
| 9 | **isPressed() returns count, not bool** | Works in boolean context but is actually uint8_t. |
| 10 | **`-`/`_` mapping is standard** | Base = `-`, Shift = `_`. Keycap printing is misleading. |
| 11 | **Opt produces nothing except boolean** | No word, no hid_keys, no modifier_keys entry. |
| 12 | **Fn and Opt don't set modifier bits** | Only Ctrl (0x01), Shift (0x02), Alt (0x04) do. |

---

## 14. SOURCE FILES REFERENCE

All files in `M5Cardputer/src/utility/`:

| File | Contains |
|---|---|
| `Keyboard.h` | KeysState struct, _key_value_map[4][14], Keyboard_Class |
| `Keyboard.cpp` | updateKeysState() logic, begin(), isKeyPressed() |
| `Keyboard_def.h` | KEY_* constants, _kb_asciimap[128] HID lookup |
| `KeyboardReader/KeyboardReader.h` | Abstract base class, Point2D_t |
| `KeyboardReader/TCA8418.cpp` | ADV hardware driver, interrupt handler, remap logic |
| `KeyboardReader/TCA8418.h` | TCA8418KeyboardReader class |
| `KeyboardReader/IOMatrix.cpp` | Original Cardputer GPIO matrix driver |
| `KeyboardReader/IOMatrix.h` | IOMatrixKeyboardReader class |
