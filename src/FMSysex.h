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

FLASHMEM bool fmSysexChecksumOk(const uint8_t *data, uint16_t len, uint8_t checksum)
{
    return fmChecksum(data, len) == checksum;
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

// Adds voice to bank as a new patch at the end. Returns false if the bank is full or saving failed.
FLASHMEM bool fmAddVoiceToBank(uint8_t bank, const uint8_t *voice)
{
    LockGuard lock(displayLock); // The display lists the bank's patches
    loadPatchNamesFromBank(bank);
    if (patches.size() >= PATCHES_LIMIT && patches[patches.size() - 1].patchUID != 0)
        return false; // Bank full
    if (patches[0].patchUID != 0) // Not an empty bank: add a slot at the end
        patches.push_back(PatchUIDAndName{0, EMPTYNAME});
    fmLoadVoiceAsPatch(voice);
    return savePatch(bank, patches.size() - 1);
}

// Adds count voices (packed, as in a bank) to bank. Returns the number added.
FLASHMEM uint8_t fmAddPackedVoicesToBank(uint8_t bank, const uint8_t *packed, uint8_t count)
{
    uint8_t voice[FM_PATCH_VOICE_SIZE];
    uint8_t added = 0;
    for (uint8_t v = 0; v < count; v++)
    {
        fmUnpackVoice(&packed[v * FM_PACKED_VOICE], voice);
        if (!fmAddVoiceToBank(bank, voice))
            break;
        added++;
    }
    return added;
}

FLASHMEM void fmImportBank(const uint8_t *packed)
{
    if (!cardStatus)
    {
        showCurrentParameterOverlay(F("DX7 Bank"), F("No SD card"));
        return;
    }
    uint8_t added = fmAddPackedVoicesToBank(currentBankIndex, packed, 32);
    loadPatchNamesFromBank(currentBankIndex);
    showCurrentParameterOverlay(F("DX7 Bank"), String(added) + F(" voices added"));
}

// ---- .syx files on the SD card (see FMSyx.h) ----

DMAMEM uint8_t syxFileBuf[FM_SYSEX_BANK_LEN]; // Separate from fmSysexBuf: a USB dump can arrive meanwhile

// Imports syxFiles[syxFileIndex] into the bank selected on the page (tempBankIndex)
FLASHMEM void syxImportFile()
{
    if (!cardStatus || syxFileIndex >= syxFiles.size())
        return;
    String path = String(SYX_FOLDER) + "/" + syxFiles[syxFileIndex];
    File file = SD.open(path.c_str());
    if (!file)
    {
        syxStatus = F("Can't open the file");
        return;
    }
    uint32_t size = file.size();
    uint32_t len = size <= sizeof(syxFileBuf) ? file.read(syxFileBuf, size) : 0;
    file.close();
    const uint8_t *b = syxFileBuf;
    uint8_t bank = tempBankIndex;
    const uint8_t *packed = nullptr;
    bool checksumOk = true;
    uint8_t added = 0;
    uint8_t voices = 0;

    if (len == FM_SYSEX_BANK_LEN && b[0] == 0xF0 && b[1] == 0x43 && b[3] == 0x09)
    {
        packed = &b[6];
        checksumOk = fmChecksum(packed, 4096) == b[4102];
    }
    else if (len == 4096) // Bank data without the SysEx header
        packed = b;
    if (packed)
    {
        voices = 32;
        added = fmAddPackedVoicesToBank(bank, packed, 32);
    }
    else if (len == FM_SYSEX_VOICE_LEN && b[0] == 0xF0 && b[1] == 0x43 && b[3] == 0x00)
    {
        voices = 1;
        checksumOk = fmChecksum(&b[6], 155) == b[161];
        added = fmAddVoiceToBank(bank, &b[6]) ? 1 : 0;
    }
    else
    {
        syxStatus = F("Not a DX7 voice or bank");
        return;
    }
    {
        LockGuard lock(displayLock);
        loadPatchNamesFromBank(bank);
        syxStatus = String(added) + F(" of ") + String(voices) + F(" added");
        if (added < voices)
            syxStatus += F(": bank full");
        if (!checksumOk)
            syxStatus += F(" (bad checksum)");
        // Play the first new patch
        if (added > 0)
        {
            currentBankIndex = bank;
            currentPatchIndex = patches.size() - added;
            recallPatch(bank, patches[currentPatchIndex].patchUID);
        }
    }
}

// A file name from the bank name: characters FAT can't have replaced, and not overwriting a file
FLASHMEM String syxExportName(uint8_t bank, uint8_t part, uint8_t parts)
{
    String base = bankNames[bank];
    for (uint16_t i = 0; i < base.length(); i++)
    {
        char c = base[i];
        if (c < 32 || strchr("\\/:*?\"<>|", c))
            base.setCharAt(i, '_');
    }
    base.trim();
    if (base.length() == 0)
        base = String(F("Bank ")) + String(bank + 1);
    if (parts > 1)
        base += " " + String(part + 1);
    String path = String(SYX_FOLDER) + "/" + base + ".syx";
    for (uint8_t n = 2; SD.exists(path.c_str()) && n < 100; n++)
        path = String(SYX_FOLDER) + "/" + base + " (" + String(n) + ").syx";
    return path;
}

// Writes the selected bank (tempBankIndex) as 32 voice .syx files, padded with init voices
FLASHMEM void syxExportBank()
{
    if (!cardStatus)
        return;
    uint8_t bank = tempBankIndex;
    Patches bankPatches;
    {
        LockGuard lock(displayLock);
        loadPatchNamesFromBank(bank);
        bankPatches = patches;
    }
    uint8_t count = bankPatches[0].patchUID == 0 ? 0 : bankPatches.size();
    if (count == 0)
    {
        syxStatus = F("The bank is empty");
        return;
    }
    if (!SD.exists(SYX_FOLDER))
        SD.mkdir(SYX_FOLDER);
    PatchStruct current = currentPatch; // loadPatch() reads into currentPatch: put it back after
    uint8_t parts = (count + 31) / 32;
    uint8_t voice[FM_PATCH_VOICE_SIZE];
    String lastName;
    bool ok = true;
    for (uint8_t part = 0; part < parts && ok; part++)
    {
        uint8_t *packed = &syxFileBuf[6];
        for (uint8_t v = 0; v < 32; v++)
        {
            uint8_t i = part * 32 + v;
            if (i < count && loadPatch(bank, bankPatches[i].patchUID) && currentPatch.VoiceValid)
            {
                memcpy(voice, currentPatch.Voice, FM_PATCH_VOICE_SIZE);
                // The envelope macros are part of the sound: put them into the rates
                for (uint8_t op = 0; op < 6; op++)
                    for (uint8_t r = 0; r < 4; r++)
                        voice[op * 21 + r] = constrain((int16_t)voice[op * 21 + r] - ((int16_t)currentPatch.EnvMacro[r] - 50), 0, 99);
                // Voice name from the patch name, 10 characters
                for (uint8_t c = 0; c < 10; c++)
                {
                    char ch = c < strlen(currentPatch.PatchName) ? currentPatch.PatchName[c] : ' ';
                    voice[145 + c] = (ch >= 32 && ch < 127) ? ch : ' ';
                }
            }
            else
                fmInitVoice(voice); // Empty slot, or a patch without an FM voice
            fmPackVoice(voice, &packed[v * FM_PACKED_VOICE]);
        }
        const uint8_t header[6] = {0xF0, 0x43, 0x00, 0x09, 0x20, 0x00};
        memcpy(syxFileBuf, header, 6);
        syxFileBuf[4102] = fmChecksum(packed, 4096);
        syxFileBuf[4103] = 0xF7;
        String path = syxExportName(bank, part, parts);
        File file = SD.open(path.c_str(), FILE_WRITE);
        ok = file && file.write(syxFileBuf, FM_SYSEX_BANK_LEN) == FM_SYSEX_BANK_LEN;
        if (file)
            file.close();
        lastName = path.substring(strlen(SYX_FOLDER) + 1);
    }
    currentPatch = current;
    LockGuard lock(displayLock);
    loadPatchNamesFromBank(bank);
    syxScanFiles();
    if (!ok)
        syxStatus = F("Couldn't write the file");
    else if (parts == 1)
        syxStatus = String(F("Saved ")) + lastName;
    else
        syxStatus = String(F("Saved ")) + String(parts) + F(" files");
    setEncodersState(state); // The file list changed
}

// From loop()
FLASHMEM void fmServiceSyxFiles()
{
    if (syxImportRequested)
    {
        syxImportRequested = false;
        syxImportFile();
    }
    if (syxExportRequested)
    {
        syxExportRequested = false;
        syxExportBank();
    }
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
            {
                groupvec[activeGroupIndex]->voiceDataChanged(result - 300); // Keeps the macros on rates
                setEncodersState(state); // Show the new value if it's on this page
            }
        }
        else if (DEBUG)
            Serial.printf("SysEx not used: %u bytes\n", len);
    }
    fmSysexLen = 0;
    fmSysexReady = false;
}

#endif
