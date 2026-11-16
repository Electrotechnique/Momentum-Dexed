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

// Makes voice (155 bytes, unpacked) the current patch, keeping the current effects (fmVoicePatch())
FLASHMEM void fmLoadVoiceAsPatch(const uint8_t *voice)
{
    currentPatch = fmVoicePatch(voice, currentPatch);
    setCurrentPatchData();
}

// Adds voice to bank as a new patch at the end. patches must hold the bank's patches (savePatch()
// loads them again after saving). Returns false if the bank is full or saving failed.
FLASHMEM bool fmAppendVoice(uint8_t bank, const uint8_t *voice)
{
    LockGuard lock(displayLock); // The display lists the bank's patches
    if (patches.size() >= PATCHES_LIMIT && patches[patches.size() - 1].patchUID != 0)
        return false; // Bank full
    if (patches[0].patchUID != 0) // Not an empty bank: add a slot at the end
        patches.push_back(PatchUIDAndName{0, EMPTYNAME});
    fmLoadVoiceAsPatch(voice);
    return savePatch(bank, patches.size() - 1);
}

FLASHMEM void fmLoadBankPatchNames(uint8_t bank)
{
    LockGuard lock(displayLock);
    loadPatchNamesFromBank(bank);
}

// Adds voice to bank as a new patch at the end. Returns false if the bank is full or saving failed.
FLASHMEM bool fmAddVoiceToBank(uint8_t bank, const uint8_t *voice)
{
    fmLoadBankPatchNames(bank);
    return fmAppendVoice(bank, voice);
}

// Adds count voices (packed, as in a bank) to bank, showing progress under title. Returns the
// number added. The bank's names are read once: each was read twice per voice before, and
// reading them takes about 1 ms per patch in the bank.
FLASHMEM uint8_t fmAddPackedVoicesToBank(uint8_t bank, const uint8_t *packed, uint8_t count, const char *title)
{
    uint8_t voice[FM_PATCH_VOICE_SIZE];
    uint8_t added = 0;
    fmProgressStart(title, count);
    fmLoadBankPatchNames(bank);
    for (uint8_t v = 0; v < count; v++)
    {
        fmUnpackVoice(&packed[v * FM_PACKED_VOICE], voice);
        if (!fmAppendVoice(bank, voice))
            break;
        added++;
        fmProgressStep(added);
    }
    fmProgressEnd();
    return added;
}

FLASHMEM void fmImportBank(const uint8_t *packed)
{
    if (!cardStatus)
    {
        showCurrentParameterOverlay(F("DX7 Bank"), F("No SD card"));
        return;
    }
    uint8_t added = fmAddPackedVoicesToBank(currentBankIndex, packed, 32, "Saving received bank");
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
    const uint8_t *first = nullptr;
    bool packed = false;
    bool checksumOk = true;
    int8_t voices = syxReadFile(syxFiles[syxFileIndex], syxFileBuf, &first, &packed, &checksumOk);
    if (voices < 0)
    {
        syxStatus = F("Can't open the file");
        return;
    }
    if (voices == 0)
    {
        syxStatus = F("Not a DX7 voice or bank");
        return;
    }
    uint8_t bank = tempBankIndex;
    uint8_t added = packed ? fmAddPackedVoicesToBank(bank, first, voices, "Importing .syx bank") : (fmAddVoiceToBank(bank, first) ? 1 : 0);
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

// The envelope macros (-50 to +50, + is slower) put into the rates of a voice: they're part of the
// sound, and a DX7 voice has no place for them
FLASHMEM void fmApplyMacros(uint8_t *voice, const int8_t *macro)
{
    for (uint8_t op = 0; op < 6; op++)
        for (uint8_t r = 0; r < 4; r++)
            voice[op * 21 + r] = constrain((int16_t)voice[op * 21 + r] - macro[r], 0, 99);
}

// The patch name as the voice name, 10 characters
FLASHMEM void fmSetVoiceName(uint8_t *voice, const char *name)
{
    size_t len = strlen(name);
    for (uint8_t c = 0; c < 10; c++)
    {
        char ch = c < len ? name[c] : ' ';
        voice[145 + c] = (ch >= 32 && ch < 127) ? ch : ' ';
    }
}

// The patches of a bank, for building bank dumps. Returns how many (0 for an empty bank).
FLASHMEM uint8_t fmBankPatches(uint8_t bank, Patches &bankPatches)
{
    LockGuard lock(displayLock); // The display lists the bank's patches
    loadPatchNamesFromBank(bank);
    bankPatches = patches;
    return bankPatches[0].patchUID == 0 ? 0 : bankPatches.size();
}

// A 32 voice bank dump (FM_SYSEX_BANK_LEN bytes in buf) of patches first to first + 31 of the
// bank, padded with init voices. loadPatch() reads into currentPatch: the caller puts it back.
FLASHMEM void fmBuildBankDump(uint8_t bank, const Patches &bankPatches, uint8_t count, uint8_t first, uint8_t *buf)
{
    uint8_t voice[FM_PATCH_VOICE_SIZE];
    uint8_t *packed = &buf[6];
    for (uint8_t v = 0; v < 32; v++)
    {
        uint8_t i = first + v;
        if (i < count && loadPatch(bank, bankPatches[i].patchUID) && currentPatch.VoiceValid)
        {
            memcpy(voice, currentPatch.Voice, FM_PATCH_VOICE_SIZE);
            int8_t macro[4];
            for (uint8_t r = 0; r < 4; r++)
                macro[r] = (int8_t)currentPatch.EnvMacro[r] - 50;
            fmApplyMacros(voice, macro);
            fmSetVoiceName(voice, currentPatch.PatchName);
        }
        else
            fmInitVoice(voice); // Empty slot, or a patch without an FM voice
        fmPackVoice(voice, &packed[v * FM_PACKED_VOICE]);
    }
    const uint8_t header[6] = {0xF0, 0x43, 0x00, 0x09, 0x20, 0x00};
    memcpy(buf, header, 6);
    buf[4102] = fmChecksum(packed, 4096);
    buf[4103] = 0xF7;
}

// A single voice dump (FM_SYSEX_VOICE_LEN bytes in buf) of the voice playing now, with any
// unsaved edits, the envelope macros and the patch name
FLASHMEM void fmBuildVoiceDump(uint8_t *buf)
{
    VoiceGroup *vg = groupvec[activeGroupIndex];
    uint8_t *voice = &buf[6];
    vg->getVoiceData(voice);
    int8_t macro[4];
    for (uint8_t r = 0; r < 4; r++)
        macro[r] = vg->getEnvMacro(r);
    fmApplyMacros(voice, macro);
    fmSetVoiceName(voice, currentPatch.PatchName);
    const uint8_t header[6] = {0xF0, 0x43, 0x00, 0x00, 0x01, 0x1B};
    memcpy(buf, header, 6);
    buf[161] = fmChecksum(voice, 155);
    buf[162] = 0xF7;
}

// To the computer (USB-C port) and to a device on the USB host port
FLASHMEM void fmSendSysex(const uint8_t *buf, uint32_t len)
{
    usbMIDI.sendSysEx(len, buf, true);
    usbMIDI.send_now();
    usbHostMIDI.sendSysEx(len, buf, true);
}

FLASHMEM void fmSendVoice()
{
    fmBuildVoiceDump(syxFileBuf);
    fmSendSysex(syxFileBuf, FM_SYSEX_VOICE_LEN);
}

// Sends 32 patches of the bank from first as a bank dump. Returns false if the bank has none there.
FLASHMEM bool fmSendBank(uint8_t bank, uint8_t first)
{
    Patches bankPatches;
    uint8_t count = fmBankPatches(bank, bankPatches);
    if (first >= count)
        return false;
    PatchStruct current = currentPatch;
    fmBuildBankDump(bank, bankPatches, count, first, syxFileBuf);
    currentPatch = current;
    fmSendSysex(syxFileBuf, FM_SYSEX_BANK_LEN);
    return true;
}

// Writes the selected bank (tempBankIndex) as 32 voice .syx files, padded with init voices
FLASHMEM void syxExportBank()
{
    if (!cardStatus)
        return;
    uint8_t bank = tempBankIndex;
    Patches bankPatches;
    uint8_t count = fmBankPatches(bank, bankPatches);
    if (count == 0)
    {
        syxStatus = F("The bank is empty");
        return;
    }
    if (!SD.exists(SYX_FOLDER))
        SD.mkdir(SYX_FOLDER);
    PatchStruct current = currentPatch; // loadPatch() reads into currentPatch: put it back after
    uint8_t parts = (count + 31) / 32;
    String lastName;
    bool ok = true;
    for (uint8_t part = 0; part < parts && ok; part++)
    {
        fmBuildBankDump(bank, bankPatches, count, part * 32, syxFileBuf);
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
    if (syxSendRequested >= 0)
    {
        uint8_t action = syxSendRequested;
        syxSendRequested = -1;
        if (action == SYX_SEND_VOICE)
        {
            fmSendVoice();
            syxStatus = String(F("Sent ")) + currentPatch.PatchName;
        }
        else
        {
            uint8_t first = (action - SYX_SEND_BANK) * 32;
            if (fmSendBank(tempBankIndex, first))
                syxStatus = String(F("Sent patches ")) + String(first + 1) + "-" + String(first + 32);
            else
                syxStatus = String(F("No patch ")) + String(first + 1) + F(" in the bank");
        }
    }
}

// From loop()
FLASHMEM void fmServiceSysex()
{
    if (!fmSysexReady)
        return;
    const uint8_t *s = fmSysexBuf;
    uint16_t len = fmSysexLen;
    if (len == 5 && s[0] == 0xF0 && s[1] == 0x43 && (s[2] & 0xF0) == 0x20 && s[4] == 0xF7)
    {
        // Dump request from an editor or librarian, as a DX7 answers it
        if (s[3] == 0x00)
        {
            fmSendVoice();
            showCurrentParameterOverlay(F("DX7 SysEx"), F("Voice sent"));
        }
        else if (s[3] == 0x09 && fmSendBank(currentBankIndex, 0)) // Patches 1-32 of the current bank
            showCurrentParameterOverlay(F("DX7 SysEx"), F("Bank sent"));
    }
    else if (len >= 7 && s[0] == 0xF0 && s[1] == 0x43 && s[len - 1] == 0xF7)
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
