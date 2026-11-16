/*
  Momentum-Dexed: DX7 .syx files on the SD card (the voice formats, file list and page encoders).
  Importing and exporting is in FMSysex.h, run from loop().

  .syx files go in the /SYX folder of the SD card:
  - 32 voice bank (VMEM): 4104 bytes, or 4096 bytes without the SysEx header
  - single voice (VCED): 163 bytes
*/
#ifndef MOMENTUM_FM_SYX_H
#define MOMENTUM_FM_SYX_H

#include <vector>
#include <algorithm>

#define SYX_FOLDER "/SYX"
#define SYX_MAX_FILES 100
#define FM_PACKED_VOICE 128 // One voice of a 32 voice bank
#define FM_SYSEX_VOICE_LEN 163
#define FM_SYSEX_BANK_LEN 4104

std::vector<String> syxFiles; // Names of the .syx files in SYX_FOLDER, sorted
uint8_t syxFileIndex = 0;
String syxStatus = "";             // Result of the last import/export, shown on the page
volatile bool syxImportRequested = false; // Acted on from loop(): SD card writes take a while
volatile bool syxExportRequested = false;

// Packs a 155 byte voice into the 128 bytes of a DX7 bank voice (VMEM)
FLASHMEM void fmPackVoice(const uint8_t *v, uint8_t *p)
{
    for (uint8_t op = 0; op < 6; op++) // OP6 first, as in the voice
    {
        const uint8_t *o = &v[op * 21];
        uint8_t *q = &p[op * 17];
        memcpy(q, o, 11); // Rates, levels, break point, depths
        q[11] = ((o[12] & 3) << 2) | (o[11] & 3);  // Right curve, left curve
        q[12] = ((o[20] & 15) << 3) | (o[13] & 7); // Detune, rate scaling
        q[13] = ((o[15] & 7) << 2) | (o[14] & 3);  // Key velocity sensitivity, amp mod sensitivity
        q[14] = o[16];                             // Output level
        q[15] = ((o[18] & 31) << 1) | (o[17] & 1); // Coarse, mode
        q[16] = o[19];                             // Fine
    }
    memcpy(&p[102], &v[126], 8);                       // Pitch envelope
    p[110] = v[134] & 31;                              // Algorithm
    p[111] = ((v[136] & 1) << 3) | (v[135] & 7);       // Osc key sync, feedback
    memcpy(&p[112], &v[137], 4);                       // LFO speed, delay, pitch depth, amp depth
    p[116] = ((v[143] & 7) << 4) | ((v[142] & 7) << 1) | (v[141] & 1); // Pitch mod sens, wave, sync
    p[117] = v[144];                                   // Transpose
    memcpy(&p[118], &v[145], 10);                      // Name
}

// Unpacks a DX7 bank voice. (Synth_Dexed's decodeVoice() reads the LFO pitch mod
// sensitivity as 2 bits, so 4-7 became 0-3. It is 3 bits.)
FLASHMEM void fmUnpackVoice(const uint8_t *p, uint8_t *v)
{
    for (uint8_t op = 0; op < 6; op++)
    {
        const uint8_t *q = &p[op * 17];
        uint8_t *o = &v[op * 21];
        memcpy(o, q, 11);
        o[11] = q[11] & 3;
        o[12] = (q[11] >> 2) & 3;
        o[13] = q[12] & 7;
        o[20] = (q[12] >> 3) & 15;
        o[14] = q[13] & 3;
        o[15] = (q[13] >> 2) & 7;
        o[16] = q[14] & 127;
        o[17] = q[15] & 1;
        o[18] = (q[15] >> 1) & 31;
        o[19] = q[16] & 127;
    }
    memcpy(&v[126], &p[102], 8);
    v[134] = p[110] & 31;
    v[135] = p[111] & 7;
    v[136] = (p[111] >> 3) & 1;
    memcpy(&v[137], &p[112], 4);
    v[141] = p[116] & 1;
    v[142] = (p[116] >> 1) & 7;
    v[143] = (p[116] >> 4) & 7;
    v[144] = p[117] & 127;
    memcpy(&v[145], &p[118], 10);
}

// The DX7 init voice: one sine carrier (OP1), "INIT VOICE"
FLASHMEM void fmInitVoice(uint8_t *v)
{
    memset(v, 0, 155);
    for (uint8_t op = 0; op < 6; op++)
    {
        uint8_t *o = &v[op * 21];
        memset(o, 99, 4);     // Rates
        memset(&o[4], 99, 3); // L1-L3, L4 = 0
        o[8] = 39;            // Break point C3
        o[16] = op == 5 ? 99 : 0; // Output level: OP1 (last in the voice) only
        o[18] = 1;            // Coarse 1
        o[20] = 7;            // Detune 0
    }
    memset(&v[126], 99, 4); // Pitch envelope rates
    memset(&v[130], 50, 4); // Pitch envelope levels: centre
    v[136] = 1;             // Osc key sync
    v[137] = 35;            // LFO speed
    v[141] = 1;             // LFO key sync
    v[143] = 3;             // Pitch mod sensitivity
    v[144] = 24;            // Transpose C3
    memcpy(&v[145], "INIT VOICE", 10);
}

// DX7 checksum of the data bytes
FLASHMEM uint8_t fmChecksum(const uint8_t *data, uint16_t len)
{
    uint8_t sum = 0;
    for (uint16_t i = 0; i < len; i++)
        sum += data[i];
    return (128 - (sum & 0x7F)) & 0x7F;
}

FLASHMEM bool syxIsSyxName(const char *name)
{
    size_t n = strlen(name);
    return n > 4 && name[0] != '.' && strcasecmp(&name[n - 4], ".syx") == 0;
}

// A 10 character DX7 voice name, trailing spaces removed ("FM Voice" if blank)
FLASHMEM void fmNameStr(const uint8_t *chars, char *name, size_t size)
{
    uint8_t n = 0;
    for (uint8_t i = 0; i < 10 && n + 1 < size; i++)
    {
        char c = chars[i];
        name[n++] = (c >= 32 && c < 127) ? c : ' ';
    }
    while (n > 0 && name[n - 1] == ' ')
        n--;
    name[n] = 0;
    if (n == 0)
        strlcpy(name, "FM Voice", size);
}

// The name of a 155 byte voice
FLASHMEM void fmVoiceName(const uint8_t *voice, char *name, size_t size)
{
    fmNameStr(&voice[145], name, size);
}

// Reads SYX_FOLDER/name into buf (FM_SYSEX_BANK_LEN bytes) and finds the voices in it.
// Returns the number of voices: 32 (a bank, *packed true: 128 byte voices) or 1 (a single
// 155 byte voice), 0 if it isn't a DX7 bank or voice, -1 if the file can't be opened.
// *voices points to the first voice in buf.
FLASHMEM int8_t syxReadFile(const String &name, uint8_t *buf, const uint8_t **voices, bool *packed, bool *checksumOk)
{
    String path = String(SYX_FOLDER) + "/" + name;
    File file = SD.open(path.c_str());
    if (!file)
        return -1;
    uint32_t size = file.size();
    uint32_t len = size <= FM_SYSEX_BANK_LEN ? file.read(buf, size) : 0;
    file.close();
    *checksumOk = true;
    if (len == FM_SYSEX_BANK_LEN && buf[0] == 0xF0 && buf[1] == 0x43 && buf[3] == 0x09)
    {
        *voices = &buf[6];
        *packed = true;
        *checksumOk = fmChecksum(&buf[6], 4096) == buf[4102];
        return 32;
    }
    if (len == 4096) // Bank data without the SysEx header
    {
        *voices = buf;
        *packed = true;
        return 32;
    }
    if (len == FM_SYSEX_VOICE_LEN && buf[0] == 0xF0 && buf[1] == 0x43 && buf[3] == 0x00)
    {
        *voices = &buf[6];
        *packed = false;
        *checksumOk = fmChecksum(&buf[6], 155) == buf[161];
        return 1;
    }
    return 0;
}

// Lists the .syx files in SYX_FOLDER
FLASHMEM void syxScanFiles()
{
    syxFiles.clear();
    if (!cardStatus)
        return;
    File dir = SD.open(SYX_FOLDER);
    if (!dir || !dir.isDirectory())
        return;
    while (syxFiles.size() < SYX_MAX_FILES)
    {
        File entry = dir.openNextFile();
        if (!entry)
            break;
        if (!entry.isDirectory() && syxIsSyxName(entry.name()))
            syxFiles.push_back(String(entry.name()));
        entry.close();
    }
    dir.close();
    std::sort(syxFiles.begin(), syxFiles.end(), [](const String &a, const String &b)
              { return strcasecmp(a.c_str(), b.c_str()) < 0; });
    if (syxFileIndex >= syxFiles.size())
        syxFileIndex = 0;
}

FLASHMEM void configureSyxPage()
{
    syxScanFiles();
    encMap[ENC_TL].active = true;
    encMap[ENC_TL].Parameter = syxExport;
    encMap[ENC_TL].ShowValue = false;
    encMap[ENC_TL].Value = 0;
    encMap[ENC_TL].Range = 0;
    encMap[ENC_TL].ParameterStr = ParameterStrMap[syxExport];
    encMap[ENC_TL].Push = true;
    encMap[ENC_TL].PushAction = State::SYXPAGE;

    encMap[ENC_TR].active = true;
    encMap[ENC_TR].Parameter = cancel;
    encMap[ENC_TR].ShowValue = false;
    encMap[ENC_TR].Value = 0;
    encMap[ENC_TR].Range = 0;
    encMap[ENC_TR].ValueStr = "";
    encMap[ENC_TR].ParameterStr = ParameterStrMap[cancel];
    encMap[ENC_TR].Push = true;
    encMap[ENC_TR].PushAction = State::MAIN;

    encMap[ENC_BL].active = true;
    encMap[ENC_BL].Parameter = bankeditselect;
    encMap[ENC_BL].ShowValue = false;
    encMap[ENC_BL].Range = BANKS_LIMIT;
    encMap[ENC_BL].ParameterStr = ParameterStrMap[bankeditselect];
    encMap[ENC_BL].Push = true;
    encMap[ENC_BL].PushAction = State::SYXPAGE;

    encMap[ENC_BR].active = syxFiles.size() > 0;
    encMap[ENC_BR].Parameter = syxImport;
    encMap[ENC_BR].ShowValue = false;
    encMap[ENC_BR].Value = syxFileIndex;
    encMap[ENC_BR].Range = syxFiles.size() > 0 ? syxFiles.size() - 1 : 0;
    encMap[ENC_BR].Counter = 0;
    encMap[ENC_BR].ParameterStr = ParameterStrMap[syxImport];
    encMap[ENC_BR].Push = true;
    encMap[ENC_BR].PushAction = State::SYXPAGE;
}

// Encoder turned on the .syx page. Returns false if it isn't one of its parameters.
FLASHMEM bool syxEncoderTurn(EncoderMappingStruct *enc, int8_t newDelta)
{
    if (enc->Parameter != syxImport && enc->Parameter != syxExport)
        return false;
    if (enc->Parameter == syxImport && newDelta != 0)
        syxFileIndex = enc->Value;
    return true;
}

// Encoder pushed on the .syx page (or the Edit Banks DX7 .syx entry). Returns false if not one of these.
FLASHMEM bool syxEncoderPush(EncoderMappingStruct *enc)
{
    switch (enc->Parameter)
    {
    case syxPage:
        syxStatus = "";
        return true;
    case syxImport:
        if (syxFiles.size() > 0)
        {
            syxStatus = F("Importing...");
            syxImportRequested = true;
        }
        return true;
    case syxExport:
        syxStatus = F("Exporting...");
        syxExportRequested = true;
        return true;
    default:
        return false;
    }
}

// ---- Voice browser (Recall page > DX7 Voices) ----
// Plays the voices of the .syx files in SYX_FOLDER as they're chosen, without importing them.
// Turning only selects; the file is read and the voice played from loop() once the encoder has
// been still for VB_PLAY_DELAY_MS (FMVoiceBrowser.h), as patch recall does.

#define VB_PLAY_DELAY_MS 100

DMAMEM uint8_t vbBuf[FM_SYSEX_BANK_LEN]; // The file being browsed (RAM2)
int16_t vbLoadedFile = -1;               // syxFiles index of the file in vbBuf, -1 none
uint8_t vbCount = 0;                     // Voices in it: 32, 1, or 0 (not a DX7 file)
bool vbPacked = false;                   // Bank voices (128 bytes) or one 155 byte voice
const uint8_t *vbVoices = nullptr;       // First voice in vbBuf
uint8_t vbVoiceIndex = 0;                // Selected voice
String vbStatus = "";                    // Why there are no voices, shown in place of the list
volatile bool vbPending = false;         // A voice to play (and maybe a file to read) from loop()
elapsedMillis vbTimer;
PatchStruct vbBasePatch; // The patch playing when the browser opened: its effects are kept

// Name of voice v of the browsed file
FLASHMEM void vbVoiceName(uint8_t v, char *name, size_t size)
{
    if (v >= vbCount || !vbVoices)
        name[0] = 0;
    else
        fmNameStr(vbPacked ? &vbVoices[v * FM_PACKED_VOICE + 118] : &vbVoices[145], name, size);
}

// Recall page: DX7 Voices opens the browser
FLASHMEM void configureVoiceBrowserOpen(EncoderMappingStruct *enc)
{
    enc->active = cardStatus;
    enc->Parameter = vbOpen;
    enc->ShowValue = false;
    enc->Value = 0;
    enc->Range = 0;
    enc->ValueStr = "";
    enc->ParameterStr = ParameterStrMap[vbOpen];
    enc->Push = true;
    enc->PushAction = State::VOICEBROWSER;
}

FLASHMEM void configureVoiceBrowserPage()
{
    encMap[ENC_TL].active = vbCount > 0;
    encMap[ENC_TL].Parameter = vbSave;
    encMap[ENC_TL].ShowValue = false;
    encMap[ENC_TL].Value = 0;
    encMap[ENC_TL].Range = 0;
    encMap[ENC_TL].ParameterStr = ParameterStrMap[vbSave];
    encMap[ENC_TL].Push = true;
    encMap[ENC_TL].PushAction = State::PATCHSAVING;

    encMap[ENC_TR].active = true; // Cancel: back to the patch that was playing
    encMap[ENC_TR].Parameter = cancel;
    encMap[ENC_TR].ShowValue = false;
    encMap[ENC_TR].Value = 0;
    encMap[ENC_TR].Range = 0;
    encMap[ENC_TR].ValueStr = "";
    encMap[ENC_TR].ParameterStr = ParameterStrMap[cancel];
    encMap[ENC_TR].Push = true;
    encMap[ENC_TR].PushAction = State::MAIN;

    encMap[ENC_BL].active = syxFiles.size() > 0;
    encMap[ENC_BL].Parameter = vbFile;
    encMap[ENC_BL].ShowValue = false;
    encMap[ENC_BL].Value = syxFileIndex;
    encMap[ENC_BL].Range = syxFiles.size() > 0 ? syxFiles.size() - 1 : 0;
    encMap[ENC_BL].Counter = 0;
    encMap[ENC_BL].ParameterStr = ParameterStrMap[vbFile];
    encMap[ENC_BL].Push = false;

    encMap[ENC_BR].active = vbCount > 0;
    encMap[ENC_BR].Parameter = vbVoice;
    encMap[ENC_BR].ShowValue = false;
    encMap[ENC_BR].Value = vbVoiceIndex;
    encMap[ENC_BR].Range = vbCount > 0 ? vbCount - 1 : 0;
    encMap[ENC_BR].Counter = 0;
    encMap[ENC_BR].ParameterStr = ParameterStrMap[vbVoice];
    encMap[ENC_BR].Push = true; // Keep the voice (unsaved, as a SysEx voice dump)
    encMap[ENC_BR].PushAction = State::MAIN;
}

#endif
