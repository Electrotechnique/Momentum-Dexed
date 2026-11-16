/*
  Momentum-Dexed: voice browser. From the Recall page (TL: DX7 Voices), plays the voices of the
  .syx files in the SD card's SYX folder without importing them.

  - File (BL): the .syx file. Voice (BR): the voice, played once the encoder stops; push to keep it
  - Save (TL), or the Save button: save the voice as a patch (bank, slot and name as usual)
  - Cancel (TR): back to the patch that was playing

  The voice plays with the effects of the patch that was playing when the browser opened, and
  the defaults for the rest, as an imported voice (fmVoicePatch()). Data and encoders are in FMSyx.h.
*/
#ifndef MOMENTUM_FM_VOICE_BROWSER_H
#define MOMENTUM_FM_VOICE_BROWSER_H

// Voice v of the browsed file, unpacked to 155 bytes
FLASHMEM void vbGetVoice(uint8_t v, uint8_t *voice)
{
    if (vbPacked)
        fmUnpackVoice(&vbVoices[v * FM_PACKED_VOICE], voice);
    else
        memcpy(voice, vbVoices, FM_PATCH_VOICE_SIZE);
}

// Plays the selected voice now (reading the file first if it isn't the one in vbBuf)
FLASHMEM void vbPlaySelected()
{
    vbPending = false;
    if (!cardStatus || syxFiles.size() == 0)
        return;
    LockGuard lock(displayLock); // The display lists the voices from vbBuf
    if (vbLoadedFile != syxFileIndex)
    {
        bool checksumOk = true;
        int8_t n = syxReadFile(syxFiles[syxFileIndex], vbBuf, &vbVoices, &vbPacked, &checksumOk);
        vbLoadedFile = syxFileIndex;
        vbCount = n > 0 ? n : 0;
        if (vbVoiceIndex >= vbCount)
            vbVoiceIndex = 0;
        vbStatus = n < 0 ? F("Can't open the file") : (n == 0 ? F("Not a DX7 voice or bank") : F(""));
        if (n > 0 && !checksumOk) // Common in downloaded files, and the voices are usually fine
            Serial.printf("Voice browser: %s has a bad checksum\n", syxFiles[syxFileIndex].c_str());
        setEncodersState(state); // Voice range, Save
    }
    if (vbCount == 0)
        return;
    uint8_t voice[FM_PATCH_VOICE_SIZE];
    vbGetVoice(vbVoiceIndex, voice);
    silence(); // As a patch recall: no notes hanging on with the old voice
    currentPatch = fmVoicePatch(voice, vbBasePatch); // As an import: the effects of that patch
    setCurrentPatchData();
}

// From loop(): plays the chosen voice once the encoder has been still
FLASHMEM void fmServiceVoiceBrowser()
{
    if (!vbPending || vbTimer < VB_PLAY_DELAY_MS)
        return;
    if (state != State::VOICEBROWSER)
    {
        vbPending = false; // Left the page meanwhile (Cancel)
        return;
    }
    vbPlaySelected();
}

FLASHMEM void vbSchedulePlay()
{
    vbPending = true;
    vbTimer = 0;
}

FLASHMEM void vbOpenBrowser()
{
    syxScanFiles();
    vbBasePatch = currentPatch;
    vbLoadedFile = -1; // Read again: the card or the file may have changed
    vbCount = 0;
    vbVoiceIndex = 0;
    vbStatus = "";
    if (syxFiles.size() > 0)
    {
        vbPending = true;
        vbTimer = VB_PLAY_DELAY_MS; // Play the first voice straight away
    }
}

// Encoder turned on the voice browser. Returns false if it isn't one of its parameters.
FLASHMEM bool vbEncoderTurn(EncoderMappingStruct *enc, int8_t newDelta)
{
    switch (enc->Parameter)
    {
    case vbFile:
        if (newDelta != 0 && enc->Value != syxFileIndex)
        {
            syxFileIndex = enc->Value;
            vbVoiceIndex = 0;
            vbLoadedFile = -1; // The list is empty until the file has been read
            vbCount = 0;
            vbStatus = "";
            encMap[ENC_BR].active = false;
            encMap[ENC_TL].active = false;
            vbSchedulePlay();
        }
        return true;
    case vbVoice:
        if (newDelta != 0 && enc->Value != vbVoiceIndex)
        {
            vbVoiceIndex = enc->Value;
            vbSchedulePlay();
        }
        return true;
    case vbOpen:
    case vbSave:
        return true;
    default:
        return false;
    }
}

// Encoder pushed on the Recall page (DX7 Voices) or the voice browser. Returns false if not one
// of these; the caller then goes to enc->PushAction.
FLASHMEM bool vbEncoderPush(EncoderMappingStruct *enc)
{
    switch (enc->Parameter)
    {
    case vbOpen:
        vbOpenBrowser();
        return true;
    case vbVoice: // Keep the voice
        if (vbPending)
            vbPlaySelected();
        ledsOff();
        return true;
    case vbSave:
        if (vbPending)
            vbPlaySelected();
        if (!startPatchSaving()) // Lights Save's LED in place of Recall's
        {
            showCurrentParameterOverlay(F("Save"), F("The bank is full"));
            singleLED(ledColour::RED, 8);
        }
        enc->PushAction = state; // PATCHSAVING, or still the browser
        return true;
    default:
        return false;
    }
}

#endif
