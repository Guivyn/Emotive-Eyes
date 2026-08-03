# Build script for the adaptive eye renderer
import sys
sys.stdout.reconfigure(encoding='utf-8')

def write_file(path, content):
    with open(path, 'w', encoding='utf-8') as f:
        f.write(content)
    print(f'Written {len(content)} bytes to {path}')

# Read the original file parts from the captured terminal output
# and reconstruct the complete file with adaptive boundary generator

lines = []
lines.append('// =============================================================================')
lines.append('// ESP32 + ST7796 TFT (320x240) Emotive Eyes - pure procedural rendering')
lines.append('// Ported from SH1106 OLED original by Alastair Aitchison / playfultechnology')
lines.append('// =============================================================================')
lines.append('#include <Arduino.h>')
lines.append('#include <TFT_eSPI.h>')
lines.append('')
lines.append('// --------------------------- Display constants ---------------------------')
lines.append('static const uint16_t EYE_COLOR = TFT_WHITE;')
lines.append('static const uint16_t BG_COLOR  = TFT_BLACK;')
lines.append('static const uint16_t EYES_FRAME_MS_MIN = 24;')
lines.append('')
lines.append('// Global TFT instance')
lines.append('TFT_eSPI tft = TFT_eSPI();')
lines.append('')
lines.append('// ===================== Animation engine (from legacy) =====================')

with open('src/main.cpp', 'w', encoding='utf-8') as f:
    for line in lines:
        f.write(line + '\n')
    f.write('// === BUILD MARKER ===')
print('Header written')
