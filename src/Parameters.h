#pragma once
#include <stdint.h>
#include <MIDI.h>
#include <Constants.h>

// Globals used for Settings
extern byte midiChannel;
extern byte midiOutCh;
extern midi::Thru::Mode MIDIThru;
extern String patchName;
extern boolean encCW;
extern boolean vuMeter;
extern boolean sendCC;
extern boolean syncToMIDIClk;
extern int8_t tuningCents;
extern float tuningCentsFrac;

// Global patch modifiers
extern float lfoSyncFreq;
extern float lfoFilterTempoValue;
extern float lfoPitchTempoValue;

// Exponential envelopes
extern int8_t envTypeAmp;
extern int8_t envTypeFilt;
// Glide shape
extern int8_t glideShape;

extern uint8_t currentVolume;

extern int8_t charCursor;
extern int8_t nameCursor;

// extern uint8_t currentSettingsOption;
// extern uint8_t currentSettingsValue;

// *** FM patch: the DX7 voice plus Momentum's effects and performance controls ***
// *** Initialised with a default patch to store when there are no patches on SD card ***
#define FM_PATCH_VOICE_SIZE 155 // Unpacked DX7 voice (as in a DX7 single voice dump), name at 145-154
typedef struct PatchStruct
{
    uint32_t UID = 0;
    char PatchName[64] = "Init"; // INITPATCHNAME;
    uint8_t Voice[FM_PATCH_VOICE_SIZE] = {};
    bool VoiceValid = false;  // false: use the Dexed init voice
    uint8_t OpEnabled = 0x3F; // Operator on/off, bit (n-1) = OP n
    uint8_t EnvMacro[4] = {50, 50, 50, 50}; // Envelope macros, 50 = 0 (rates as in the voice)
    // Controllers (not part of a DX7 voice)
    uint8_t PitchBend = 2;        // Semitones
    uint8_t ModWheelRange = 50;   // 0-99
    uint8_t ModWheelTarget = 1;   // Bitmapped: 1 Pitch, 2 Amp, 4 EG bias
    uint8_t AftertouchRange = 0;  // 0-99
    uint8_t AftertouchTarget = 0; // Bitmapped as the mod wheel
    uint8_t MonophonicMode = 0;
    uint8_t PortamentoTime = 0;   // 0-99, 0 = off
    // Tone: Dexed output filter
    uint8_t FilterFreq = 127; // 0-127, 127 = open
    uint8_t FilterRes = 0;    // 0-127
    // Effects
    uint8_t EnsembleEffectAmt = 8;
    uint8_t EnsembleEffectMix = 0;
    uint8_t ReverbEffectTime = 8;
    uint8_t ReverbEffectMix = 0;
    uint8_t EnsembleEffectDepth = 64; // 64 = original depth (25-200%)
    uint8_t EnsembleEffectDelay = 64; // 64 = 11.6ms, the original (2-40ms)
    uint8_t ReverbEffectDamping = 64; // 64 = 0.5
    uint8_t ReverbEffectBassCut = 26; // 26 = 0.2
} PatchStruct;