/*
  Momentum FM: DX7 SysEx from USB MIDI (device and host ports)

  - Single voice dump (VCED, 163 bytes): becomes the current voice. Save it as a patch as usual.
  - 32 voice bank dump (VMEM, 4104 bytes): the 32 voices are added to the current bank as
    new patches, named from the voices, until the bank is full (128 patches).
  - Voice parameter change (7 bytes): applied to the current voice, so an editor such as the
    Dexed plugin can edit Momentum live.

  Messages are collected by the MIDI handlers and acted on from loop(), as saving a bank
  writes 32 files to the SD card.
*/
#ifndef MOMENTUM_FM_SYSEX_H
#define MOMENTUM_FM_SYSEX_H

#define FM_SYSEX_VOICE_LEN 163
#define FM_SYSEX_BANK_LEN 4104
#define FM_PACKED_VOICE 128

DMAMEM uint8_t fmSysexBuf[FM_SYSEX_BANK_LEN]; // RAM2: only needed while a dump arrives
volatile uint16_t fmSysexLen = 0;
volatile bool fmSysexReady = false; // A complete message is waiting for fmServiceSysex()
bool fmSysexOverflow = false;

// usbMIDI and USB host MIDI deliver long SysEx in pieces
void fmSysexHandler(const uint8_t *data, uint16_t length, bool complete)
{
    if (fmSysexReady)
        return; // Still busy with the last one
    if (fmSysexLen + length > sizeof(fmSysexBuf))
        fmSysexOverflow = true;
    else
    {
        memcpy(&fmSysexBuf[fmSysexLen], data, length);
        fmSysexLen += length;
    }
    if (complete)
    {
        if (fmSysexOverflow)
            fmSysexLen = 0;
        else
            fmSysexReady = true;
        fmSysexOverflow = false;
    }
}

// DX7 checksum of the data bytes
FLASHMEM bool fmSysexChecksumOk(const uint8_t *data, uint16_t len, uint8_t checksum)
{
    uint8_t sum = 0;
    for (uint16_t i = 0; i < len; i++)
        sum += data[i];
    return ((128 - (sum & 0x7F)) & 0x7F) == checksum;
}

// The 10 character voice name, trailing spaces removed
FLASHMEM void fmVoiceName(const uint8_t *voice, char *name, size_t size)
{
    uint8_t n = 0;
    for (uint8_t i = 0; i < 10 && n + 1 < size; i++)
    {
        char c = voice[145 + i];
        name[n++] = (c >= 32 && c < 127) ? c : ' ';
    }
    while (n > 0 && name[n - 1] == ' ')
        n--;
    name[n] = 0;
    if (n == 0)
        strlcpy(name, "FM Voice", size);
}

// Makes voice (155 bytes, unpacked) the current patch, with the default effects and controls
FLASHMEM void fmLoadVoiceAsPatch(const uint8_t *voice)
{
    PatchStruct ps;
    memcpy(ps.Voice, voice, FM_PATCH_VOICE_SIZE);
    ps.VoiceValid = true;
    fmVoiceName(voice, ps.PatchName, sizeof(ps.PatchName));
    currentPatch = ps;
    setCurrentPatchData();
}

FLASHMEM void fmImportBank(const uint8_t *packed)
{
    if (!cardStatus)
    {
        showCurrentParameterOverlay(F("DX7 Bank"), F("No SD card"));
        return;
    }
    uint8_t voice[NUM_VOICE_PARAMETERS];
    uint8_t imported = 0;
    for (uint8_t v = 0; v < 32; v++)
    {
        groupvec[activeGroupIndex]->decodeVoice(voice, (uint8_t *)&packed[v * FM_PACKED_VOICE]);
        loadPatchNamesFromBank(currentBankIndex);
        if (patches.size() >= PATCHES_LIMIT && patches[patches.size() - 1].patchUID != 0)
            break; // Bank full
        if (patches[0].patchUID != 0) // Not an empty bank: add a slot at the end
            patches.push_back(PatchUIDAndName{0, EMPTYNAME});
        fmLoadVoiceAsPatch(voice);
        if (!savePatch(currentBankIndex, patches.size() - 1))
            break;
        imported++;
    }
    loadPatchNamesFromBank(currentBankIndex);
    showCurrentParameterOverlay(F("DX7 Bank"), String(imported) + F(" voices added"));
}

// From loop()
FLASHMEM void fmServiceSysex()
{
    if (!fmSysexReady)
        return;
    const uint8_t *s = fmSysexBuf;
    uint16_t len = fmSysexLen;
    if (len >= 7 && s[0] == 0xF0 && s[1] == 0x43 && s[len - 1] == 0xF7)
    {
        if (len == FM_SYSEX_VOICE_LEN && s[3] == 0x00 && fmSysexChecksumOk(&s[6], 155, s[161]))
        {
            fmLoadVoiceAsPatch(&s[6]);
            updatePatchnameAndIndex(currentPatch.PatchName, currentPatchIndex + 1, 0);
            showCurrentParameterOverlay(F("DX7 Voice"), currentPatch.PatchName);
        }
        else if (len == FM_SYSEX_BANK_LEN && s[3] == 0x09 && fmSysexChecksumOk(&s[6], 4096, s[4102]))
        {
            fmImportBank(&s[6]);
        }
        else if (len == 7 && (s[2] & 0xF0) == 0x10) // Parameter change
        {
            AudioUpdateLock lock;
            int16_t result = global.dexed.checkSystemExclusive(s, len);
            if (result >= 300 && result < 300 + FM_PATCH_VOICE_SIZE)
                setEncodersState(state); // Show the new value if it's on this page
        }
        else if (DEBUG)
            Serial.printf("SysEx not used: %u bytes\n", len);
    }
    fmSysexLen = 0;
    fmSysexReady = false;
}

#endif
