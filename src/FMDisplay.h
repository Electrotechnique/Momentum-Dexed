/*
  Momentum FM: the FM pages and drawings. Included by Display.h.

  Page layout as the subtractive Momentum: the four encoders in the corners, title at y=150,
  page indicator below, voice grid and ARP/SEQ/CLK/KEY indicators at the top. The drawings
  go in the band between the indicators and the title (y 92-146).
*/
#ifndef MOMENTUM_FM_DISPLAY_H
#define MOMENTUM_FM_DISPLAY_H

#define FM_DRAW_TOP 92
#define FM_DRAW_BOTTOM 146

// The page title, voice grid, indicators, page indicator and encoder corners
FLASHMEM void renderFMPageChrome(String title, uint8_t pages, uint8_t no)
{
  tft.setFont(Arial_16);
  tft.setTextColor(ILI9341_BLUE);
  tft.setTextDatum(TC_DATUM);
  tft.drawString(title, 160, 150);
  renderVoiceGrid();
  renderArpIndicator();
  if (!arpRunning)
    renderSeqIndicator();
  renderMidiClk();
  renderKeyboardIndicator();
  renderPageIndicator(pages, no);
  renderCorners();
}

// A DX7 style 4 rate / 4 level envelope. It starts and ends at L4, holds at L3 while the key
// is down. Higher rates are faster, so shorter segments. levelMid: draw levels -50..+49
// about a centre line (pitch envelope). selStage 1-4 is drawn highlighted, 0 for none.
FLASHMEM void renderDXEnvelope(const uint8_t *rates, const uint8_t *levels, uint8_t selStage, bool levelMid,
                               int16_t x0, int16_t y0, int16_t w, int16_t h)
{
  const int16_t sustainW = w / 6;
  float segW[4];
  float total = 0.0f;
  for (uint8_t i = 0; i < 4; i++)
  {
    segW[i] = 3.0f + (99 - rates[i]) * (99 - rates[i]) / 99.0f; // Slow rates take much longer
    total += segW[i];
  }
  float scale = (w - sustainW) / total;
  auto yFor = [&](uint8_t level) -> int16_t
  { return y0 + h - (int16_t)(level * h / 99); };

  if (levelMid)
    tft.drawFastHLine(x0, yFor(50), w, ILI9341_DARKGREY);

  float x = x0;
  int16_t y = yFor(levels[3]);
  for (uint8_t i = 0; i < 4; i++)
  {
    if (i == 3) // Key released: hold at L3 first
    {
      tft.drawFastHLine(x, y, sustainW, ILI9341_DARKGREY);
      x += sustainW;
    }
    float nx = x + segW[i] * scale;
    int16_t ny = yFor(levels[i]);
    uint16_t colour = (i + 1 == selStage) ? ILI9341_YELLOW : ILI9341_CYAN;
    tft.drawLine(x, y, nx, ny, colour);
    if (i + 1 == selStage)
      tft.drawLine(x, y + 1, nx, ny + 1, colour); // Thicker
    // Stage number under the segment
    tft.setFont(Arial_8);
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(i + 1 == selStage ? ILI9341_YELLOW : ILI9341_MIDGREY);
    tft.drawString(String(i + 1), (x + nx) / 2, y0 + h + 2);
    x = nx;
    y = ny;
  }
}

FLASHMEM void renderOpEnvelope(uint8_t op, uint8_t selStage, int16_t x0, int16_t y0, int16_t w, int16_t h)
{
  VoiceGroup *vg = groupvec[activeGroupIndex];
  uint8_t rates[4], levels[4];
  for (uint8_t i = 0; i < 4; i++)
  {
    rates[i] = vg->getOpParam(op, DEXED_OP_EG_R1 + i);
    levels[i] = vg->getOpParam(op, DEXED_OP_EG_L1 + i);
  }
  renderDXEnvelope(rates, levels, selStage, false, x0, y0, w, h);
}

FLASHMEM void renderPitchEnvelope(uint8_t selStage, int16_t x0, int16_t y0, int16_t w, int16_t h)
{
  VoiceGroup *vg = groupvec[activeGroupIndex];
  uint8_t rates[4], levels[4];
  for (uint8_t i = 0; i < 4; i++)
  {
    rates[i] = vg->getVoiceParam(DEXED_PITCH_EG_R1 + i);
    levels[i] = vg->getVoiceParam(DEXED_PITCH_EG_L1 + i);
  }
  renderDXEnvelope(rates, levels, selStage, true, x0, y0, w, h);
}

// Keyboard level scaling of an operator: output level change across the keyboard
// either side of the break point. Curves: 0 -Lin, 1 -Exp, 2 +Exp, 3 +Lin.
FLASHMEM void renderKeyScaling(uint8_t op, int16_t x0, int16_t y0, int16_t w, int16_t h)
{
  VoiceGroup *vg = groupvec[activeGroupIndex];
  uint8_t bp = vg->getOpParam(op, DEXED_OP_LEV_SCL_BRK_PT); // 0-99 = A-1..C8
  uint8_t ld = vg->getOpParam(op, DEXED_OP_SCL_LEFT_DEPTH);
  uint8_t rd = vg->getOpParam(op, DEXED_OP_SCL_RGHT_DEPTH);
  uint8_t lc = vg->getOpParam(op, DEXED_OP_SCL_LEFT_CURVE);
  uint8_t rc = vg->getOpParam(op, DEXED_OP_SCL_RGHT_CURVE);
  int16_t mid = y0 + h / 2;
  int16_t bx = x0 + bp * w / 99;
  tft.drawFastHLine(x0, mid, w, ILI9341_DARKGREY);
  tft.drawFastVLine(bx, y0, h, ILI9341_DARKGREY);
  // Each side: distance from the break point 0..1 -> offset, linear or exponential
  auto offset = [&](float d, uint8_t depth, uint8_t curve) -> int16_t
  {
    float shape = (curve == 0 || curve == 3) ? d : d * d * d;
    float amount = shape * depth / 99.0f * (h / 2);
    return (curve < 2) ? amount : -amount; // Negative curves lower the level (drawn down)
  };
  int16_t px = x0, py = mid + offset(1.0f, ld, lc);
  for (int16_t x = x0 + 2; x <= x0 + w; x += 2)
  {
    float d = x < bx ? (bx - x) / (float)max(1, bx - x0) : (x - bx) / (float)max(1, x0 + w - bx);
    int16_t y = mid + (x < bx ? offset(d, ld, lc) : offset(d, rd, rc));
    tft.drawLine(px, py, x, y, ILI9341_CYAN);
    px = x;
    py = y;
  }
  tft.setFont(Arial_8);
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(ILI9341_YELLOW);
  tft.drawString(fmNoteName(bp + 21), bx, y0 + h + 2);
}

// Places op and, above it, what modulates it, depth first: the first modulator goes straight
// above, others in new columns. A plain recursive function (at most 6 deep) rather than a
// std::function, to keep the display thread's stack use small.
FLASHMEM void fmPlaceOp(const FMAlgorithm &a, uint8_t op, uint8_t r, int8_t *col, int8_t *row, uint8_t &cols, uint8_t &rows)
{
  col[op] = cols;
  row[op] = r;
  if (r + 1 > rows)
    rows = r + 1;
  bool first = true;
  for (uint8_t i = 0; i < a.links; i++)
  {
    uint8_t m = a.mod[i][0];
    if (a.mod[i][1] == op && col[m] < 0)
    {
      if (!first)
        cols++;
      fmPlaceOp(a, m, r + 1, col, row, cols, rows);
      first = false;
    }
  }
}

// The algorithm diagram: carriers on the bottom row, each modulator above what it modulates.
// The operator being edited is outlined yellow, operators switched off are grey.
FLASHMEM void renderAlgorithm(uint8_t algorithm, uint8_t selOp, int16_t x0, int16_t y0, int16_t w, int16_t h)
{
  const FMAlgorithm &a = FM_ALGORITHM[algorithm % FM_ALGORITHMS];
  VoiceGroup *vg = groupvec[activeGroupIndex];
  uint8_t carriers = fmCarrierMask(algorithm);
  int8_t col[7] = {-1, -1, -1, -1, -1, -1, -1};
  int8_t row[7] = {0};
  uint8_t cols = 0;
  uint8_t rows = 1;

  for (uint8_t op = 1; op <= FM_OPS; op++)
    if ((carriers & (1 << (op - 1))) && col[op] < 0)
    {
      fmPlaceOp(a, op, 0, col, row, cols, rows);
      cols++;
    }

  const int16_t boxW = 20, boxH = 11;
  int16_t pitchX = min((int16_t)34, (int16_t)(w / max((uint8_t)1, cols)));
  int16_t pitchY = min((int16_t)15, (int16_t)((h - 6) / rows));
  int16_t left = x0 + (w - pitchX * cols) / 2;
  int16_t bottom = y0 + h - 6 - boxH; // Top of the carrier row, room for the output line
  auto bx = [&](uint8_t op) -> int16_t
  { return left + col[op] * pitchX + (pitchX - boxW) / 2; };
  auto by = [&](uint8_t op) -> int16_t
  { return bottom - row[op] * pitchY; };

  // Links: down from the modulator, across, down into the target (as the DX7 chart)
  for (uint8_t i = 0; i < a.links; i++)
  {
    uint8_t m = a.mod[i][0], t = a.mod[i][1];
    int16_t x1 = bx(m) + boxW / 2, y1 = by(m) + boxH;
    int16_t x2 = bx(t) + boxW / 2, y2 = by(t);
    int16_t ym = (y1 + y2) / 2;
    tft.drawFastVLine(x1, y1, ym - y1 + 1, ILI9341_CYAN);
    tft.drawFastHLine(min(x1, x2), ym, abs(x2 - x1) + 1, ILI9341_CYAN);
    tft.drawFastVLine(x2, ym, y2 - ym, ILI9341_CYAN);
  }
  // Output line under the carriers
  int16_t outY = bottom + boxH + 3;
  int16_t firstX = 999, lastX = -1;
  for (uint8_t op = 1; op <= FM_OPS; op++)
    if (carriers & (1 << (op - 1)))
    {
      int16_t cx = bx(op) + boxW / 2;
      tft.drawFastVLine(cx, bottom + boxH, 3, ILI9341_CYAN);
      firstX = min(firstX, cx);
      lastX = max(lastX, cx);
    }
  tft.drawFastHLine(firstX, outY, lastX - firstX + 1, ILI9341_CYAN);
  // Feedback loop on the right of its operator
  uint8_t fb = a.feedbackOp;
  int16_t fx = bx(fb) + boxW, fy = by(fb);
  tft.drawFastHLine(fx, fy + boxH / 2, 4, ILI9341_CYAN);
  tft.drawFastVLine(fx + 4, fy - 3, boxH / 2 + 4, ILI9341_CYAN);
  tft.drawFastHLine(fx - boxW / 2, fy - 3, boxW / 2 + 5, ILI9341_CYAN);
  tft.drawFastVLine(fx - boxW / 2, fy - 3, 3, ILI9341_CYAN);
  // Operators
  tft.setFont(Arial_8);
  tft.setTextDatum(MC_DATUM);
  for (uint8_t op = 1; op <= FM_OPS; op++)
  {
    bool carrier = carriers & (1 << (op - 1));
    bool on = vg->getOpEnabled(op);
    uint16_t colour = on ? ILI9341_CYAN : ILI9341_DARKGREY;
    int16_t x = bx(op), y = by(op);
    tft.fillRect(x, y, boxW, boxH, carrier ? colour : ILI9341_BLACK);
    tft.drawRect(x, y, boxW, boxH, colour);
    if (op == selOp)
    {
      tft.drawRect(x - 1, y - 1, boxW + 2, boxH + 2, ILI9341_YELLOW);
      tft.drawRect(x - 2, y - 2, boxW + 4, boxH + 4, ILI9341_YELLOW);
    }
    tft.setTextColor(carrier ? ILI9341_BLACK : ILI9341_WHITE);
    tft.drawString(String(op), x + boxW / 2, y + boxH / 2 + 1);
  }
}

// Output level of each operator as a bar: carriers cyan, modulators grey, off dark grey
FLASHMEM void renderOpLevels(int16_t x0, int16_t y0, int16_t h, uint8_t selOp)
{
  VoiceGroup *vg = groupvec[activeGroupIndex];
  uint8_t carriers = vg->carrierMask();
  for (uint8_t op = 1; op <= FM_OPS; op++)
  {
    uint8_t level = min((uint8_t)99, vg->getOpParam(op, DEXED_OP_OUTPUT_LEV));
    int16_t bh = level * h / 99;
    int16_t x = x0 + (op - 1) * 10;
    uint16_t colour = !vg->getOpEnabled(op) ? ILI9341_DARKGREY : ((carriers & (1 << (op - 1))) ? ILI9341_CYAN : ILI9341_MIDGREY);
    tft.drawRect(x, y0, 8, h, ILI9341_DARKERGREY);
    tft.fillRect(x, y0 + h - bh, 8, bh, colour);
    if (op == selOp)
      tft.drawFastHLine(x, y0 + h + 2, 8, ILI9341_YELLOW);
  }
}

FLASHMEM void renderOpPage(uint8_t no)
{
  static const char *titles[6] = {"Frequency", "Output", "Sensitivity", "Envelope", "Key Scaling", "Scaling Curves"};
  tft.fillScreen(ILI9341_BLACK);
  switch (no)
  {
  case 1:
    tft.setFont(Arial_24);
    tft.setTextColor(ILI9341_WHITE);
    tft.setTextDatum(TC_DATUM);
    tft.drawString(fmFrequencyStr(fmOp), 160, 104);
    break;
  case 2:
  case 3:
    renderOpLevels(130, 98, 40, fmOp);
    break;
  case 4:
    renderOpEnvelope(fmOp, fmStage, 70, FM_DRAW_TOP + 2, 180, 40);
    break;
  case 5:
  case 6:
    renderKeyScaling(fmOp, 70, FM_DRAW_TOP + 2, 180, 40);
    break;
  }
  renderFMPageChrome("Op " + String(fmOp) + " " + titles[(no - 1) % 6], 6, no);
}

FLASHMEM void renderAlgoPage(uint8_t no)
{
  tft.fillScreen(ILI9341_BLACK);
  if (no == 1)
  {
    renderAlgorithm(groupvec[activeGroupIndex]->getVoiceParam(DEXED_ALGORITHM), fmOp, 40, FM_DRAW_TOP - 2, 240, FM_DRAW_BOTTOM - FM_DRAW_TOP + 2);
    renderFMPageChrome(F("Algorithm"), 2, no);
  }
  else
  {
    renderPitchEnvelope(fmPStage, 70, FM_DRAW_TOP + 2, 180, 40);
    renderFMPageChrome(F("Pitch Envelope"), 2, no);
  }
}

FLASHMEM void renderLfoPage(uint8_t no)
{
  tft.fillScreen(ILI9341_BLACK);
  renderFMPageChrome(F("LFO"), 2, no);
}

FLASHMEM void renderCtrlPage(uint8_t no)
{
  tft.fillScreen(ILI9341_BLACK);
  renderFMPageChrome(no == 1 ? F("Bend & Portamento") : F("Wheel & Aftertouch"), 2, no);
}

FLASHMEM void renderTonePage()
{
  tft.fillScreen(ILI9341_BLACK);
  renderFMPageChrome(F("Tone"), 1, 1);
}

// Main page: algorithm and operator levels, top right
FLASHMEM void renderMainPageFM()
{
  tft.setFont(Arial_10);
  tft.setTextColor(ILI9341_MIDGREY);
  tft.setTextDatum(TC_DATUM);
  tft.drawString("ALG " + String(groupvec[activeGroupIndex]->getVoiceParam(DEXED_ALGORITHM) + 1), 274, 48);
  renderOpLevels(245, 61, 25, 0); // Top right: clear of long bank names, below the TR encoder

}

#endif
