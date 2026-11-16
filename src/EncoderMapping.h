void setConfigurationForCC(uint8_t, uint8_t);

typedef struct EncoderMappingStruct
{
    boolean active = true;
    uint8_t Parameter = 255;        // MIDI cc (up to 127) or other code up to 255
    uint8_t ShowValue = false;      // Some parameters don't show value like patch no
    uint8_t Value = 0;              // Up to 255
    uint8_t DefaultValue = 0;       // Up to 255
    int8_t Counter = 0;             // For reduced sensitivity for small ranges - do not set
    String ValueStr = "";           // For transforming values in strings for display
    uint8_t Range = 127;            // Up to 255
    String ParameterStr = "";       // For display
    boolean Push = false;           // Pushing encoder button for further settings
    State PushAction = State::MAIN; // Next state when button pushed
} EncoderMappingStruct;

EncoderMappingStruct encMap[4] = {};

// The encoder value is a byte, but tempo goes up to 300bpm (and higher from MIDI clock).
// Only the text is shown; clamp the byte, as converting an out of range float is undefined.
inline uint8_t bpmToEncValue(float bpm)
{
    return bpm <= 0.0f ? 0 : (bpm >= 255.0f ? 255 : (uint8_t)bpm);
}

// Sets the value and value string of the encoder when this parameter is on the display
FLASHMEM boolean setEncValue(boolean act, uint8_t parameter, uint8_t value, String str, uint8_t newParameter)
{
    LockGuard lock(displayLock); // The display renders from encMap
    for (uint8_t i = 0; i < 4; i++)
    {
        if (encMap[i].Parameter == parameter)
        {
            if (act != encMap[i].active && state != State::MAIN)
                encMap[i].active = act;
            if (newParameter != encMap[i].Parameter)
                encMap[i].Parameter = newParameter;
            encMap[i].ParameterStr = ParameterStrMap[newParameter];
            if (value != encMap[i].Value)
                encMap[i].Value = value;
            encMap[i].ValueStr = str;
            return true; // True if this was set
        }
    }
    return false;
}

FLASHMEM boolean setEncValue(uint8_t parameter, uint8_t value, uint8_t range, String str)
{
    LockGuard lock(displayLock); // The display renders from encMap
    // Extra routine for settings which have variable size ranges
    for (uint8_t i = 0; i < 4; i++)
    {
        if (encMap[i].Parameter == parameter)
        {
            encMap[i].Range = range;
            break;
        }
    }
    return setEncValue(true, parameter, value, str, parameter);
}

FLASHMEM boolean setEncValue(uint8_t parameter, uint8_t value, String str)
{
    LockGuard lock(displayLock); // The display renders from encMap
    return setEncValue(true, parameter, value, str, parameter);
}

// For turning off
FLASHMEM boolean setEncInactive(uint8_t parameter)
{
    LockGuard lock(displayLock); // The display renders from encMap
    return setEncValue(false, parameter, 0, "", parameter);
}

FLASHMEM void configureOff(EncoderMappingStruct *enc)
{
    enc->active = false;
    enc->Parameter = 255;
}

FLASHMEM void configureKeyboardOct(EncoderMappingStruct *enc, State st = State::MAIN)
{
    enc->active = true;
    enc->Parameter = kbdOct;
    enc->ShowValue = true;
    enc->Value = keyboardOct;
    enc->Range = 6;
    enc->ParameterStr = ParameterStrMap[kbdOct];
    enc->ValueStr = KEYBOARD_OCT_STR[keyboardOct];
    enc->Push = true;
    enc->PushAction = st;
}

FLASHMEM void configureKeyboardScale(EncoderMappingStruct *enc, State st = State::MAIN)
{
    enc->active = true;
    enc->Parameter = kbdScale;
    enc->ShowValue = true;
    enc->Value = keyboardScale;
    enc->Range = 23;
    enc->ParameterStr = ParameterStrMap[kbdScale];
    enc->ValueStr = SCALE_STR[keyboardScale];
    enc->Push = true;
    enc->PushAction = st;
}

FLASHMEM void configureCCbankselectLSB(EncoderMappingStruct *enc, State st = State::MAIN)
{
    enc->active = true;
    enc->Parameter = CCbankselectLSB;
    enc->ShowValue = false;
    enc->Value = currentBankIndex;
    enc->Range = BANKS_LIMIT;
    enc->ParameterStr = ParameterStrMap[CCbankselectLSB];
    enc->ValueStr = "";
    enc->Push = true;
    enc->PushAction = st;
}

FLASHMEM void configurePatchselect(EncoderMappingStruct *enc, State st = State::MAIN)
{
    enc->active = true;
    enc->Parameter = patchselect;
    enc->ShowValue = false;
    enc->Value = currentPatchIndex;
    enc->Range = patches.size();
    enc->ValueStr = "";
    enc->ParameterStr = ParameterStrMap[patchselect];
    enc->Push = true;
    enc->PushAction = st;
}

FLASHMEM void configureCancel(EncoderMappingStruct *enc, State st = State::MAIN)
{
    enc->active = true;
    enc->Parameter = cancel;
    enc->ShowValue = false;
    enc->Value = 0;
    enc->Range = 0;
    enc->ValueStr = "";
    enc->ParameterStr = ParameterStrMap[cancel];
    enc->Push = true;
    enc->PushAction = st;
}






























// Filter type text: LP...HP mix, then HP fading into BP at the top of the knob, then BP
FLASHMEM String filterMixerStr(float value)
{
    if (value == BANDPASS || value >= 2.0f)
        return F("Band Pass");
    if (value > 1.0f)
        return F("High ") + String(100 - int(100 * (value - 1.0f) + 0.5f)) + F(" - ") + String(int(100 * (value - 1.0f) + 0.5f)) + F(" Band");
    if (value == LOWPASS)
        return F("Low Pass");
    if (value == HIGHPASS)
        return F("High Pass");
    return F("Low ") + String(100 - int(100 * value)) + F(" - ") + String(int(100 * value)) + F(" High");
}









// Push defaults for the FX encoders: a usable starting point rather than 0 (effect off)
#define ENSEMBLE_RATE_DEFAULT 28    // 6.0Hz
#define ENSEMBLE_MIX_DEFAULT 64     // 50%
#define ENSEMBLE_DEPTH_DEFAULT 64   // 100%, the original depth

// Ensemble depth knob (0-127) to depth: 25% - 100% - 200%, 64 = 100% (the original)
inline float ensembleDepth(uint8_t value)
{
    return value < 64 ? 0.25f + 0.75f * value / 64.0f : 1.0f + (value - 64) / 63.0f;
}
#define ENSEMBLE_DELAY_DEFAULT 64   // 11.6ms, the original delay

// Ensemble delay knob (0-127) to samples: 2 - 40ms, 64 = 512 samples (11.6ms, the original).
// Exponential either side of the middle, so each step is a similar change in character:
// flanging at the bottom, ensemble in the middle, doubling at the top.
inline float ensembleDelaySamples(uint8_t value)
{
    const float shortest = 0.002f * AUDIO_SAMPLE_RATE_EXACT; // 2ms
    const float longest = 0.040f * AUDIO_SAMPLE_RATE_EXACT;  // 40ms
    if (value < 64)
        return shortest * powf(512.0f / shortest, value / 64.0f);
    return 512.0f * powf(longest / 512.0f, (value - 64) / 63.0f);
}

// Delay text for the display: the delay in use, which the depth may lengthen at the bottom
inline String ensembleDelayStr()
{
    return String(groupvec[activeGroupIndex]->getEnsembleEffectDelayMs(), 1) + " ms";
}
#define REVERB_SIZE_DEFAULT 64      // 0.5
#define REVERB_MIX_DEFAULT 64       // 0.5 (25% wet: the mix goes up to 50:50)
#define REVERB_DAMPING_DEFAULT 64   // 50%, the previous fixed setting
#define REVERB_BASSCUT_DEFAULT 26   // 20%, the previous fixed setting

FLASHMEM void configureCCensemblefxamt(EncoderMappingStruct *enc, State st = State::MAIN)
{
    enc->active = true;
    enc->Parameter = CCensemblefxamt;
    enc->ShowValue = true;
    enc->Value = currentPatch.EnsembleEffectAmt;
    enc->DefaultValue = ENSEMBLE_RATE_DEFAULT;
    enc->ValueStr = String(ENSEMBLE_LFO[currentPatch.EnsembleEffectAmt]) + " Hz";
    enc->Range = 127;
    enc->ParameterStr = ParameterStrMap[CCensemblefxamt];
    enc->Push = true;
    enc->PushAction = st;
}

FLASHMEM void configureCCensemblefxmix(EncoderMappingStruct *enc, State st = State::MAIN)
{
    enc->active = true;
    enc->Parameter = CCensemblefxmix;
    enc->ShowValue = true;
    enc->Value = currentPatch.EnsembleEffectMix;
    enc->DefaultValue = ENSEMBLE_MIX_DEFAULT;
    enc->ValueStr = String(groupvec[activeGroupIndex]->getEnsembleEffectMix());
    enc->Range = 127;
    enc->ParameterStr = ParameterStrMap[CCensemblefxmix];
    enc->Push = true;
    enc->PushAction = st;
}

FLASHMEM void configureCCreverbfxtime(EncoderMappingStruct *enc, State st = State::MAIN)
{
    enc->active = true;
    enc->Parameter = CCreverbfxtime;
    enc->ShowValue = true;
    enc->Value = currentPatch.ReverbEffectTime;
    enc->DefaultValue = REVERB_SIZE_DEFAULT;
    enc->ValueStr = String(LINEAR[currentPatch.ReverbEffectTime]);
    enc->Range = 127;
    enc->ParameterStr = ParameterStrMap[CCreverbfxtime];
    enc->Push = true;
    enc->PushAction = st;
}

FLASHMEM void configureCCreverbfxmix(EncoderMappingStruct *enc, State st = State::MAIN)
{
    enc->active = true;
    enc->Parameter = CCreverbfxmix;
    enc->ShowValue = true;
    enc->Value = currentPatch.ReverbEffectMix;
    enc->DefaultValue = REVERB_MIX_DEFAULT;
    enc->ValueStr = String(groupvec[activeGroupIndex]->getReverbEffectMix());
    enc->Range = 127;
    enc->ParameterStr = ParameterStrMap[CCreverbfxmix];
    enc->Push = true;
    enc->PushAction = st;
}

FLASHMEM void configureCCensemblefxdepth(EncoderMappingStruct *enc, State st = State::MAIN)
{
    enc->active = true;
    enc->Parameter = CCensemblefxdepth;
    enc->ShowValue = true;
    enc->Value = currentPatch.EnsembleEffectDepth;
    enc->DefaultValue = ENSEMBLE_DEPTH_DEFAULT;
    enc->ValueStr = String(int(ensembleDepth(currentPatch.EnsembleEffectDepth) * 100 + 0.5f)) + "%";
    enc->Range = 127;
    enc->ParameterStr = ParameterStrMap[CCensemblefxdepth];
    enc->Push = true;
    enc->PushAction = st;
}

FLASHMEM void configureCCensemblefxdelay(EncoderMappingStruct *enc, State st = State::MAIN)
{
    enc->active = true;
    enc->Parameter = CCensemblefxdelay;
    enc->ShowValue = true;
    enc->Value = currentPatch.EnsembleEffectDelay;
    enc->DefaultValue = ENSEMBLE_DELAY_DEFAULT;
    enc->ValueStr = ensembleDelayStr();
    enc->Range = 127;
    enc->ParameterStr = ParameterStrMap[CCensemblefxdelay];
    enc->Push = true;
    enc->PushAction = st;
}

FLASHMEM void configureCCreverbfxdamping(EncoderMappingStruct *enc, State st = State::MAIN)
{
    enc->active = true;
    enc->Parameter = CCreverbfxdamping;
    enc->ShowValue = true;
    enc->Value = currentPatch.ReverbEffectDamping;
    enc->DefaultValue = REVERB_DAMPING_DEFAULT;
    enc->ValueStr = String(int(currentPatch.ReverbEffectDamping * 100 / 128.0f + 0.5f)) + "%";
    enc->Range = 127;
    enc->ParameterStr = ParameterStrMap[CCreverbfxdamping];
    enc->Push = true;
    enc->PushAction = st;
}

FLASHMEM void configureCCreverbfxbasscut(EncoderMappingStruct *enc, State st = State::MAIN)
{
    enc->active = true;
    enc->Parameter = CCreverbfxbasscut;
    enc->ShowValue = true;
    enc->Value = currentPatch.ReverbEffectBassCut;
    enc->DefaultValue = REVERB_BASSCUT_DEFAULT;
    enc->ValueStr = String(int(currentPatch.ReverbEffectBassCut * 100 / 128.0f + 0.5f)) + "%";
    enc->Range = 127;
    enc->ParameterStr = ParameterStrMap[CCreverbfxbasscut];
    enc->Push = true;
    enc->PushAction = st;
}













#include "FMEdit.h"

FLASHMEM void setDefaultValue(EncoderMappingStruct *enc)
{
    enc->Value = enc->DefaultValue;
}

// currentPatch.XXX stores relative values (0-127) like MIDI CC
// groupvec[activeGroupIndex]->XXX stores absolute, real values
FLASHMEM void setEncodersState(State s)
{
    LockGuard lock(displayLock); // The display renders from encMap
    if (setFMEncodersState(s))
        return;
    switch (s)
    {
    case State::MAIN:
        if (keyboardActive)
        {
            configureKeyboardOct(&encMap[ENC_TL]);
            configureKeyboardScale(&encMap[ENC_TR]);
        }
        else
        {
            configureOff(&encMap[ENC_TL]);
            configureOff(&encMap[ENC_TR]);
        }
        configureCCbankselectLSB(&encMap[ENC_BL], State::PATCHLIST);
        configurePatchselect(&encMap[ENC_BR], State::PATCHLIST);
        break;
    case State::PATCHLIST:
        configureVoiceBrowserOpen(&encMap[ENC_TL]);
        configureCancel(&encMap[ENC_TR]);
        configureCCbankselectLSB(&encMap[ENC_BL]);
        configurePatchselect(&encMap[ENC_BR]);
        break;
    case State::FXPAGE1:
        configureCCensemblefxdepth(&encMap[ENC_TL], State::FXPAGE1);
        configureCCensemblefxdelay(&encMap[ENC_TR], State::FXPAGE1);
        configureCCensemblefxamt(&encMap[ENC_BL], State::FXPAGE1);
        configureCCensemblefxmix(&encMap[ENC_BR], State::FXPAGE1);
        break;
    case State::FXPAGE2:
        configureCCreverbfxdamping(&encMap[ENC_TL], State::FXPAGE2);
        configureCCreverbfxbasscut(&encMap[ENC_TR], State::FXPAGE2);
        configureCCreverbfxtime(&encMap[ENC_BL], State::FXPAGE2);
        configureCCreverbfxmix(&encMap[ENC_BR], State::FXPAGE2);
        break;
    case State::SAVE:
        encMap[ENC_TL].active = false;

        encMap[ENC_TR].active = false;

        encMap[ENC_BL].active = true;
        encMap[ENC_BL].Parameter = savepatch;
        encMap[ENC_BL].ShowValue = false;
        encMap[ENC_BL].Value = 0;
        encMap[ENC_BL].Range = 127;
        encMap[ENC_BL].ParameterStr = ParameterStrMap[savepatch];
        encMap[ENC_BL].Push = true;
        encMap[ENC_BL].PushAction = State::MAIN;

        encMap[ENC_BR].active = true;
        encMap[ENC_BR].Parameter = renamepatch;
        encMap[ENC_BR].ShowValue = false;
        encMap[ENC_BR].Value = 0;
        encMap[ENC_BR].Range = 127;
        encMap[ENC_BR].ParameterStr = ParameterStrMap[renamepatch];
        encMap[ENC_BR].Push = true;
        encMap[ENC_BR].PushAction = State::MAIN;
        break;

    case State::PATCHSAVING:
        encMap[ENC_TL].active = true;
        encMap[ENC_TL].Parameter = editbank;
        encMap[ENC_TL].ShowValue = false;
        encMap[ENC_TL].Value = currentBankIndex;
        encMap[ENC_TL].Range = BANKS_LIMIT;
        encMap[ENC_TL].ValueStr = "";
        encMap[ENC_TL].ParameterStr = ParameterStrMap[editbank];
        encMap[ENC_TL].Push = true;
        encMap[ENC_TL].PushAction = State::EDITBANK;

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
        encMap[ENC_BL].Parameter = savebankselect;
        encMap[ENC_BL].ShowValue = false;
        encMap[ENC_BL].Value = currentBankIndex;
        encMap[ENC_BL].Range = BANKS_LIMIT;
        encMap[ENC_BL].ValueStr = "";
        encMap[ENC_BL].ParameterStr = ParameterStrMap[savebankselect];
        encMap[ENC_BL].Push = false;

        encMap[ENC_BR].active = true;
        encMap[ENC_BR].Parameter = namepatch;
        encMap[ENC_BR].ShowValue = false;
        encMap[ENC_BR].Value = currentPatchIndex;
        encMap[ENC_BR].Range = patches.size();
        encMap[ENC_BR].ValueStr = "";
        encMap[ENC_BR].ParameterStr = ParameterStrMap[namepatch];
        encMap[ENC_BR].Push = true;
        encMap[ENC_BR].PushAction = State::RENAMEPATCH;
        break;

    case State::RENAMEPATCH:
        encMap[ENC_TL].active = true;
        encMap[ENC_TL].Parameter = deleteCharacterPatch;
        encMap[ENC_TL].ShowValue = false;
        encMap[ENC_TL].Value = 0;
        encMap[ENC_TL].Range = 4; // More encoder rotation for accuracy
        encMap[ENC_TL].ValueStr = "";
        encMap[ENC_TL].ParameterStr = ParameterStrMap[deleteCharacterPatch];
        encMap[ENC_TL].Push = true;
        encMap[ENC_TL].PushAction = State::DELETECHARPATCH;

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
        encMap[ENC_BL].Parameter = choosecharacterPatch;
        encMap[ENC_BL].ShowValue = false;
        encMap[ENC_BL].Value = 0;
        encMap[ENC_BL].Range = TOTALCHARS;
        encMap[ENC_BL].ValueStr = "";
        encMap[ENC_BL].ParameterStr = ParameterStrMap[choosecharacterPatch];
        encMap[ENC_BL].Push = true;
        encMap[ENC_BL].PushAction = State::CHOOSECHARPATCH;

        encMap[ENC_BR].active = true;
        encMap[ENC_BR].Parameter = savepatch;
        encMap[ENC_BR].ShowValue = false;
        encMap[ENC_BR].ValueStr = "";
        encMap[ENC_BR].ParameterStr = ParameterStrMap[savepatch];
        encMap[ENC_BR].Push = true;
        encMap[ENC_BR].PushAction = State::MAIN;
        break;

    case State::DELETEPATCH:
        encMap[ENC_TL].active = true;
        encMap[ENC_TL].Parameter = deletepatch;
        encMap[ENC_TL].ShowValue = false;
        encMap[ENC_TL].ParameterStr = ParameterStrMap[deletepatch];
        encMap[ENC_TL].Push = true;
        encMap[ENC_TL].PushAction = State::MAIN;

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
        encMap[ENC_BL].Parameter = CCbankselectLSB;
        encMap[ENC_BL].ShowValue = false;
        encMap[ENC_BL].Value = currentBankIndex;
        encMap[ENC_BL].Range = BANKS_LIMIT;
        encMap[ENC_BL].ParameterStr = ParameterStrMap[CCbankselectLSB];
        encMap[ENC_BL].ValueStr = "";
        encMap[ENC_BL].Push = false;

        encMap[ENC_BR].active = true;
        encMap[ENC_BR].Parameter = patchselect;
        encMap[ENC_BR].ShowValue = false;
        encMap[ENC_BR].Value = currentPatchIndex;
        encMap[ENC_BR].Range = patches.size();
        encMap[ENC_BR].ValueStr = "";
        encMap[ENC_BR].ParameterStr = ParameterStrMap[patchselect];
        encMap[ENC_BR].Push = false;
        break;

    case State::MIDIPAGE:
        encMap[ENC_TL].active = true;
        encMap[ENC_TL].Parameter = MIDIThruMode;
        encMap[ENC_TL].ShowValue = true;
        encMap[ENC_TL].Value = MIDIThru;
        encMap[ENC_TL].Range = 3;
        encMap[ENC_TL].ParameterStr = ParameterStrMap[MIDIThruMode];
        encMap[ENC_TL].ValueStr = MIDIThruStr[MIDIThru];
        encMap[ENC_TL].Push = false;

        encMap[ENC_TR].active = false;

        encMap[ENC_BL].active = true;
        encMap[ENC_BL].Parameter = MIDIChIn;
        encMap[ENC_BL].ShowValue = true;
        encMap[ENC_BL].Value = midiChannel;
        encMap[ENC_BL].Range = 16;
        if (midiChannel == 0)
        {
            encMap[ENC_BL].ValueStr = "All";
        }
        else
        {
            encMap[ENC_BL].ValueStr = String(midiChannel);
        }
        encMap[ENC_BL].ParameterStr = ParameterStrMap[MIDIChIn];
        encMap[ENC_BL].Push = false;

        encMap[ENC_BR].active = true;
        encMap[ENC_BR].Parameter = MIDIChOut;
        encMap[ENC_BR].ShowValue = true;
        encMap[ENC_BR].Value = midiOutCh;
        encMap[ENC_BR].Range = 16;
        if (midiOutCh == 0)
        {
            encMap[ENC_BR].ValueStr = "Off";
        }
        else
        {
            encMap[ENC_BR].ValueStr = String(midiOutCh);
        }
        encMap[ENC_BR].ParameterStr = ParameterStrMap[MIDIChOut];
        encMap[ENC_BR].Push = false;
        break;

    case State::PERFORMANCEENCEDIT:
        encMap[ENC_TL].active = true;
        encMap[ENC_TL].Parameter = chooseEncoderTL;
        encMap[ENC_TL].ShowValue = true;
        encMap[ENC_TL].Value = getIndexForParametersForPerformanceEncoders(currentPerformance.TL);
        encMap[ENC_TL].ValueStr = ParameterStrMap[ParametersForPerformanceEncoders[encMap[ENC_TL].Value]];
        encMap[ENC_TL].DefaultValue = 0;
        encMap[ENC_TL].Range = perfParamCount - 1;
        encMap[ENC_TL].ParameterStr = "";
        encMap[ENC_TL].Push = true;
        encMap[ENC_TL].PushAction = State::PERFORMANCEENCEDIT;

        encMap[ENC_TR].active = true;
        encMap[ENC_TR].Parameter = chooseEncoderTR;
        encMap[ENC_TR].ShowValue = true;
        encMap[ENC_TR].Value = getIndexForParametersForPerformanceEncoders(currentPerformance.TR);
        encMap[ENC_TR].ValueStr = ParameterStrMap[ParametersForPerformanceEncoders[encMap[ENC_TR].Value]];
        encMap[ENC_TR].DefaultValue = 0;
        encMap[ENC_TR].Range = perfParamCount - 1;
        encMap[ENC_TR].ParameterStr = "";
        encMap[ENC_TR].Push = true;
        encMap[ENC_TR].PushAction = State::PERFORMANCEENCEDIT;

        encMap[ENC_BL].active = true;
        encMap[ENC_BL].Parameter = chooseEncoderBL;
        encMap[ENC_BL].ShowValue = true;
        encMap[ENC_BL].Value = getIndexForParametersForPerformanceEncoders(currentPerformance.BL);
        encMap[ENC_BL].ValueStr = ParameterStrMap[ParametersForPerformanceEncoders[encMap[ENC_BL].Value]];
        encMap[ENC_BL].DefaultValue = 0;
        encMap[ENC_BL].Range = perfParamCount - 1;
        encMap[ENC_BL].ParameterStr = "";
        encMap[ENC_BL].Push = true;
        encMap[ENC_BL].PushAction = State::PERFORMANCEENCEDIT;

        encMap[ENC_BR].active = true;
        encMap[ENC_BR].Parameter = chooseEncoderBR;
        encMap[ENC_BR].ShowValue = true;
        encMap[ENC_BR].Value = getIndexForParametersForPerformanceEncoders(currentPerformance.BR);
        encMap[ENC_BR].ValueStr = ParameterStrMap[ParametersForPerformanceEncoders[encMap[ENC_BR].Value]];
        encMap[ENC_BR].DefaultValue = 0;
        encMap[ENC_BR].Range = perfParamCount - 1;
        encMap[ENC_BR].ParameterStr = "";
        encMap[ENC_BR].Push = true;
        encMap[ENC_BR].PushAction = State::PERFORMANCEENCEDIT;
        break;
    case State::PERFORMANCEMIDIEDIT:
        encMap[ENC_TL].active = true;
        encMap[ENC_TL].Parameter = choosePerfMIDIThruMode;
        encMap[ENC_TL].ShowValue = true;
        encMap[ENC_TL].Value = currentPerformance.patches[0].midiThru; // Multitimbrality not supported yet
        encMap[ENC_TL].Range = 3;
        encMap[ENC_TL].ParameterStr = ParameterStrMap[choosePerfMIDIThruMode];
        encMap[ENC_TL].ValueStr = MIDIThruStr[currentPerformance.patches[0].midiThru]; // Multitimbrality not supported yet
        encMap[ENC_TL].Push = false;

        configureCancel(&encMap[ENC_TR], State::PERFORMANCERECALL);

        encMap[ENC_BL].active = true;
        encMap[ENC_BL].Parameter = choosePerfMIDIChIn;
        encMap[ENC_BL].ShowValue = true;
        encMap[ENC_BL].Value = currentPerformance.patches[0].midiCh; // Multitimbrality not supported yet
        encMap[ENC_BL].Range = 16;
        if (currentPerformance.patches[0].midiCh == 0) // Multitimbrality not supported yet
        {
            encMap[ENC_BL].ValueStr = "All";
        }
        else
        {
            encMap[ENC_BL].ValueStr = String(currentPerformance.patches[0].midiCh); // Multitimbrality not supported yet
        }
        encMap[ENC_BL].ParameterStr = ParameterStrMap[choosePerfMIDIChIn];
        encMap[ENC_BL].Push = false;

        encMap[ENC_BR].active = true;
        encMap[ENC_BR].Parameter = choosePerfMIDIChOut;
        encMap[ENC_BR].ShowValue = true;
        encMap[ENC_BR].Value = currentPerformance.patches[0].midiChOut; // Multitimbrality not supported yet
        encMap[ENC_BR].Range = 16;
        if (currentPerformance.patches[0].midiChOut == 0) // Multitimbrality not supported yet
        {
            encMap[ENC_BR].ValueStr = "Off";
        }
        else
        {
            encMap[ENC_BR].ValueStr = String(currentPerformance.patches[0].midiChOut); // Multitimbrality not supported yet
        }
        encMap[ENC_BR].ParameterStr = ParameterStrMap[choosePerfMIDIChOut];
        encMap[ENC_BR].Push = false;
        break;
    case State::PERFORMANCEPATCHEDIT:
        encMap[ENC_TL].active = false;

        configureCancel(&encMap[ENC_TR], State::PERFORMANCERECALL);

        encMap[ENC_BL].active = true;
        encMap[ENC_BL].Parameter = CCbankselectLSB;
        encMap[ENC_BL].ShowValue = false;
        encMap[ENC_BL].Value = currentBankIndex;
        encMap[ENC_BL].Range = BANKS_LIMIT;
        encMap[ENC_BL].ParameterStr = ParameterStrMap[CCbankselectLSB];
        encMap[ENC_BL].ValueStr = "";
        encMap[ENC_BL].Push = true;

        encMap[ENC_BR].active = true;
        encMap[ENC_BR].Parameter = patchselect;
        encMap[ENC_BR].ShowValue = false;
        encMap[ENC_BR].Value = currentPatchIndex;
        encMap[ENC_BR].Range = patches.size();
        encMap[ENC_BR].ValueStr = "";
        encMap[ENC_BR].ParameterStr = ParameterStrMap[patchselect];
        encMap[ENC_BR].Push = true;
        break;
    case State::RENAMEPERFORMANCE:
        encMap[ENC_TL].active = true;
        encMap[ENC_TL].Parameter = deleteCharacterPerformance;
        encMap[ENC_TL].ShowValue = false;
        encMap[ENC_TL].Value = 0;
        encMap[ENC_TL].Range = 4; // More encoder rotation for accuracy
        encMap[ENC_TL].ValueStr = "";
        encMap[ENC_TL].ParameterStr = ParameterStrMap[deleteCharacterPerformance];
        encMap[ENC_TL].Push = true;
        encMap[ENC_TL].PushAction = State::DELETECHARPERFORMANCE;

        configureCancel(&encMap[ENC_TR], State::PERFORMANCERECALL);

        encMap[ENC_BL].active = true;
        encMap[ENC_BL].Parameter = choosecharacterPerformance;
        encMap[ENC_BL].ShowValue = false;
        encMap[ENC_BL].Value = 0;
        encMap[ENC_BL].Range = TOTALCHARS;
        encMap[ENC_BL].ValueStr = "";
        encMap[ENC_BL].ParameterStr = ParameterStrMap[choosecharacterPerformance];
        encMap[ENC_BL].Push = true;
        encMap[ENC_BL].PushAction = State::CHOOSECHARPERFORMANCE;

        encMap[ENC_BR].active = false;
        break;
    case State::PERFORMANCERECALL:
        encMap[ENC_TL].active = true;
        encMap[ENC_TL].Parameter = PerfEdit;
        encMap[ENC_TL].ShowValue = false;
        encMap[ENC_TL].ValueStr = "";
        encMap[ENC_TL].Value = 0;
        encMap[ENC_TL].ParameterStr = ParameterStrMap[PerfEdit];
        encMap[ENC_TL].Push = true;
        encMap[ENC_TL].PushAction = State::PERFORMANCEPATCHEDIT;

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
        encMap[ENC_BL].Parameter = PerfSelect;
        encMap[ENC_BL].ShowValue = false;
        encMap[ENC_BL].ValueStr = "";
        encMap[ENC_BL].Value = 0;
        encMap[ENC_BL].Range = PERFORMANCES_LIMIT - 1;
        encMap[ENC_BL].ParameterStr = ParameterStrMap[PerfSelect];
        encMap[ENC_BL].Push = true;
        encMap[ENC_BL].PushAction = State::PERFORMANCEPAGE;

        encMap[ENC_BR].active = true;
        encMap[ENC_BR].Parameter = PerfDelete;
        encMap[ENC_BR].ShowValue = false;
        encMap[ENC_BR].ValueStr = "";
        encMap[ENC_BR].Value = 0;
        encMap[ENC_BR].Range = PERFORMANCES_LIMIT - 1;
        encMap[ENC_BR].ParameterStr = ParameterStrMap[PerfDelete];
        encMap[ENC_BR].Push = true;
        encMap[ENC_BR].PushAction = State::PERFORMANCERECALL;
        break;

    case State::PERFORMANCEPAGE:
        setConfigurationForCC(ENC_TL, currentPerformance.TL);
        setConfigurationForCC(ENC_TR, currentPerformance.TR);
        setConfigurationForCC(ENC_BL, currentPerformance.BL);
        setConfigurationForCC(ENC_BR, currentPerformance.BR);
        break;

    case State::SETTINGS:
        configureOff(&encMap[ENC_TL]);
        configureOff(&encMap[ENC_TR]);

        encMap[ENC_BL].active = true;
        encMap[ENC_BL].Parameter = settingoption;
        encMap[ENC_BL].ShowValue = true;
        encMap[ENC_BL].Value = 0;
        encMap[ENC_BL].Range = settings::getSettingsOptionsSize() - 1;
        encMap[ENC_BL].ParameterStr = ParameterStrMap[settingoption];
        encMap[ENC_BL].ValueStr = settings::current_setting();
        encMap[ENC_BL].Push = false;

        encMap[ENC_BR].active = true;
        encMap[ENC_BR].Parameter = settingvalue;
        encMap[ENC_BR].ShowValue = true;
        encMap[ENC_BR].Value = (int)settings::index();
        encMap[ENC_BR].Range = settings::current_setting_size() - 1;
        encMap[ENC_BR].ValueStr = settings::current_setting_value();
        encMap[ENC_BR].ParameterStr = ParameterStrMap[settingvalue];
        encMap[ENC_BR].Push = false;
        break;

    case State::ARPPAGE1:
        encMap[ENC_TL].active = true;
        encMap[ENC_TL].Parameter = SeqTempo;
        encMap[ENC_TL].ShowValue = true;
        encMap[ENC_TL].ValueStr = String(currentSequence.bpm, 1);
        encMap[ENC_TL].Value = bpmToEncValue(currentSequence.bpm);
        encMap[ENC_TL].Range = 255;
        encMap[ENC_TL].DefaultValue = 120;
        encMap[ENC_TL].ParameterStr = ParameterStrMap[SeqTempo];
        encMap[ENC_TL].Push = true;
        encMap[ENC_TL].PushAction = State::ARPPAGE1;

        encMap[ENC_TR].active = true;
        encMap[ENC_TR].Parameter = ArpStyle; // Style
        encMap[ENC_TR].ShowValue = true;
        encMap[ENC_TR].Value = arpStyle;
        encMap[ENC_TR].ValueStr = ARP_STYLES[arpStyle];
        encMap[ENC_TR].Range = 5;
        encMap[ENC_TR].DefaultValue = 0;
        encMap[ENC_TR].ParameterStr = ParameterStrMap[ArpStyle];
        encMap[ENC_TR].Push = true;
        encMap[ENC_TR].PushAction = State::ARPPAGE1;

        encMap[ENC_BL].active = true;
        encMap[ENC_BL].Parameter = ArpDivision; // Sixteenth, Eighth...
        encMap[ENC_BL].ShowValue = true;
        encMap[ENC_BL].ValueStr = ARP_DIVISION_STR[arpDivision];
        encMap[ENC_BL].Value = arpDivision;
        encMap[ENC_BL].Range = 9;
        encMap[ENC_BL].DefaultValue = 2;
        encMap[ENC_BL].ParameterStr = ParameterStrMap[ArpDivision];
        encMap[ENC_BL].Push = true;
        encMap[ENC_BL].PushAction = State::ARPPAGE1;

        encMap[ENC_BR].active = true;
        encMap[ENC_BR].Parameter = ArpCycle;
        encMap[ENC_BR].ShowValue = true;
        encMap[ENC_BR].ValueStr = ARP_CYCLES[arpCycles];
        encMap[ENC_BR].Value = arpCycles;
        encMap[ENC_BR].Range = 4;
        encMap[ENC_BR].ParameterStr = ParameterStrMap[ArpCycle];
        encMap[ENC_BR].Push = true;
        encMap[ENC_BR].PushAction = State::ARPPAGE1;
        break;
    case State::ARPPAGE2:
        encMap[ENC_TL].active = true;
        encMap[ENC_TL].Parameter = ArpRange;
        encMap[ENC_TL].ShowValue = true;
        encMap[ENC_TL].ValueStr = ARP_RANGE_STR[arpRange];
        encMap[ENC_TL].Value = arpRange;
        encMap[ENC_TL].Range = 6;
        encMap[ENC_TL].DefaultValue = 3;
        encMap[ENC_TL].ParameterStr = ParameterStrMap[ArpRange];
        encMap[ENC_TL].Push = true;
        encMap[ENC_TL].PushAction = State::ARPPAGE2;

        encMap[ENC_TR].active = true;
        encMap[ENC_TR].Parameter = ArpBasis;
        encMap[ENC_TR].ShowValue = true;
        encMap[ENC_TR].Value = arpBasis;
        encMap[ENC_TR].ValueStr = ARP_BASIS_STR[arpBasis];
        encMap[ENC_TR].Range = 4;
        encMap[ENC_TR].DefaultValue = 2;
        encMap[ENC_TR].ParameterStr = ParameterStrMap[ArpBasis];
        encMap[ENC_TR].Push = true;
        encMap[ENC_TR].PushAction = State::ARPPAGE2;

        encMap[ENC_BL].active = true;
        encMap[ENC_BL].Parameter = ArpDivision; // Sixteenth, Eighth...
        encMap[ENC_BL].ShowValue = true;
        encMap[ENC_BL].ValueStr = ARP_DIVISION_STR[arpDivision];
        encMap[ENC_BL].Value = arpDivision;
        encMap[ENC_BL].Range = 9;
        encMap[ENC_BL].DefaultValue = 2;
        encMap[ENC_BL].ParameterStr = ParameterStrMap[ArpDivision];
        encMap[ENC_BL].Push = true;
        encMap[ENC_BL].PushAction = State::ARPPAGE2;

        encMap[ENC_BR].active = true;
        encMap[ENC_BR].Parameter = ArpCycle;
        encMap[ENC_BR].ShowValue = true;
        encMap[ENC_BR].ValueStr = ARP_CYCLES[arpCycles];
        encMap[ENC_BR].Value = arpCycles;
        encMap[ENC_BR].Range = 4;
        encMap[ENC_BR].ParameterStr = ParameterStrMap[ArpCycle];
        encMap[ENC_BR].Push = true;
        encMap[ENC_BR].PushAction = State::ARPPAGE2;
        break;
    case State::SEQUENCERECALL:
        encMap[ENC_TL].active = true;
        encMap[ENC_TL].Parameter = SeqEdit;
        encMap[ENC_TL].ShowValue = false;
        encMap[ENC_TL].ValueStr = "";
        encMap[ENC_TL].Value = 0;
        encMap[ENC_TL].ParameterStr = ParameterStrMap[SeqEdit];
        encMap[ENC_TL].Push = true;
        encMap[ENC_TL].PushAction = State::SEQUENCEEDIT;

        encMap[ENC_TR].active = true;
        encMap[ENC_TR].Parameter = goback;
        encMap[ENC_TR].ShowValue = false;
        encMap[ENC_TR].Value = 0;
        encMap[ENC_TR].Range = 0;
        encMap[ENC_TR].ValueStr = "";
        encMap[ENC_TR].ParameterStr = ParameterStrMap[goback];
        encMap[ENC_TR].Push = true;
        encMap[ENC_TR].PushAction = State::MAIN;

        encMap[ENC_BL].active = true;
        encMap[ENC_BL].Parameter = SeqSelect;
        encMap[ENC_BL].ShowValue = false;
        encMap[ENC_BL].ValueStr = "";
        encMap[ENC_BL].Value = 0;
        encMap[ENC_BL].Range = SEQUENCES_LIMIT - 1;
        encMap[ENC_BL].ParameterStr = ParameterStrMap[SeqSelect];
        encMap[ENC_BL].Push = true;
        encMap[ENC_BL].PushAction = State::SEQUENCEPAGE;

        encMap[ENC_BR].active = true;
        encMap[ENC_BR].Parameter = SeqDelete;
        encMap[ENC_BR].ShowValue = false;
        encMap[ENC_BR].ValueStr = "";
        encMap[ENC_BR].Value = 0;
        encMap[ENC_BR].Range = SEQUENCES_LIMIT - 1;
        encMap[ENC_BR].ParameterStr = ParameterStrMap[SeqDelete];
        encMap[ENC_BR].Push = true;
        encMap[ENC_BR].PushAction = State::SEQUENCERECALL;
        break;
    case State::SEQUENCEPAGE:
        encMap[ENC_TL].active = true;
        encMap[ENC_TL].Parameter = SeqTempo;
        encMap[ENC_TL].ShowValue = true;
        encMap[ENC_TL].ValueStr = String(currentSequence.bpm, 1);
        encMap[ENC_TL].Value = bpmToEncValue(currentSequence.bpm);
        encMap[ENC_TL].Range = 255;
        encMap[ENC_TL].DefaultValue = 120;
        encMap[ENC_TL].ParameterStr = ParameterStrMap[SeqTempo];
        encMap[ENC_TL].Push = true;
        encMap[ENC_TL].PushAction = State::SEQUENCEPAGE;

        encMap[ENC_TR].active = true;
        encMap[ENC_TR].Parameter = goback;
        encMap[ENC_TR].ShowValue = false;
        encMap[ENC_TR].Value = 0;
        encMap[ENC_TR].Range = 127;
        encMap[ENC_TR].ValueStr = "";
        encMap[ENC_TR].ParameterStr = ParameterStrMap[goback];
        encMap[ENC_TR].Push = true;
        encMap[ENC_TR].PushAction = State::SEQUENCERECALL;

        encMap[ENC_BL].active = true;
        encMap[ENC_BL].Parameter = SeqStartStop;
        encMap[ENC_BL].ShowValue = false;
        encMap[ENC_BL].ValueStr = "";
        encMap[ENC_BL].Value = 0;
        encMap[ENC_BL].Range = 0;
        encMap[ENC_BL].ParameterStr = ParameterStrMap[SeqStartStop];
        encMap[ENC_BL].Push = true;
        encMap[ENC_BL].PushAction = State::SEQUENCEPAGE;

        encMap[ENC_BR].active = false;
        break;

    case State::SEQUENCEEDIT:
        encMap[ENC_TL].active = true;
        encMap[ENC_TL].Parameter = SeqTempo;
        encMap[ENC_TL].ShowValue = false; // value is shown in a different position
        encMap[ENC_TL].ValueStr = String(currentSequence.bpm, 1);
        encMap[ENC_TL].Value = bpmToEncValue(currentSequence.bpm);
        encMap[ENC_TL].Range = 255;
        encMap[ENC_TL].DefaultValue = 120;
        encMap[ENC_TL].ParameterStr = ParameterStrMap[SeqTempo];
        encMap[ENC_TL].Push = true;
        encMap[ENC_TL].PushAction = State::SEQUENCEEDIT;

        encMap[ENC_TR].active = true;
        encMap[ENC_TR].Parameter = SeqLength;
        encMap[ENC_TR].ShowValue = false; // value is shown in a different position
        encMap[ENC_TR].Value = currentSequence.length - 1;
        encMap[ENC_TR].Range = 63;
        encMap[ENC_TR].ValueStr = SEQPOSSTR[currentSequence.length - 1];
        encMap[ENC_TR].ParameterStr = ParameterStrMap[SeqLength];
        encMap[ENC_TR].Push = true;
        encMap[ENC_TR].PushAction = State::SEQUENCEEDIT;

        encMap[ENC_BL].active = true;
        encMap[ENC_BL].Parameter = SeqPosition;
        encMap[ENC_BL].ShowValue = false; // value is shown in a different position
        encMap[ENC_BL].ValueStr = "";
        encMap[ENC_BL].Value = 0;
        encMap[ENC_BL].Range = 0;
        encMap[ENC_BL].ParameterStr = ParameterStrMap[SeqPosition];
        encMap[ENC_BL].Push = true;
        encMap[ENC_BL].PushAction = State::SEQUENCEEDIT;

        encMap[ENC_BR].active = true;
        encMap[ENC_BR].Parameter = SeqNote;
        encMap[ENC_BR].ShowValue = false; // value is shown in a different position
        encMap[ENC_BR].ValueStr = "";
        encMap[ENC_BR].Value = 0;
        encMap[ENC_BR].Range = 0;
        encMap[ENC_BR].ParameterStr = ParameterStrMap[SeqNote];
        encMap[ENC_BR].Push = true;
        encMap[ENC_BR].PushAction = State::SEQUENCEEDIT;
        break;
    case State::EDITBANK:
        encMap[ENC_TL].active = true;
        encMap[ENC_TL].Parameter = deletebank;
        encMap[ENC_TL].ShowValue = false;
        encMap[ENC_TL].Range = BANKS_LIMIT;
        encMap[ENC_TL].ParameterStr = ParameterStrMap[deletebank];
        encMap[ENC_TL].Push = true;
        encMap[ENC_TL].PushAction = State::MAIN;

        // DX7 .syx import/export (Cancel is on that page)
        encMap[ENC_TR].active = true;
        encMap[ENC_TR].Parameter = syxPage;
        encMap[ENC_TR].ShowValue = false;
        encMap[ENC_TR].Value = 0;
        encMap[ENC_TR].Range = 0;
        encMap[ENC_TR].ValueStr = "";
        encMap[ENC_TR].ParameterStr = ParameterStrMap[syxPage];
        encMap[ENC_TR].Push = true;
        encMap[ENC_TR].PushAction = State::SYXPAGE;

        encMap[ENC_BL].active = true;
        encMap[ENC_BL].Parameter = bankeditselect;
        encMap[ENC_BL].ShowValue = false;
        encMap[ENC_BL].Range = BANKS_LIMIT;
        encMap[ENC_BL].ParameterStr = ParameterStrMap[bankeditselect];
        encMap[ENC_BL].Push = true;
        encMap[ENC_BL].PushAction = State::MAIN;

        encMap[ENC_BR].active = true;
        encMap[ENC_BR].Parameter = renamebank;
        encMap[ENC_BR].ShowValue = false;
        encMap[ENC_TL].Range = BANKS_LIMIT;
        encMap[ENC_BR].ParameterStr = ParameterStrMap[renamebank];
        encMap[ENC_BR].Push = true;
        encMap[ENC_BR].PushAction = State::RENAMEBANK;
        break;

    case State::RENAMEBANK:
        encMap[ENC_TL].active = true;
        encMap[ENC_TL].Parameter = deleteCharacterBank;
        encMap[ENC_TL].ShowValue = false;
        encMap[ENC_TL].Value = 0;
        encMap[ENC_TL].Range = 0;
        encMap[ENC_TL].ValueStr = "";
        encMap[ENC_TL].ParameterStr = ParameterStrMap[deleteCharacterBank];
        encMap[ENC_TL].Push = true;
        encMap[ENC_TL].PushAction = State::DELETECHARBANK;

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
        encMap[ENC_BL].Parameter = choosecharacterBank;
        encMap[ENC_BL].ShowValue = false;
        encMap[ENC_BL].Value = 0;
        encMap[ENC_BL].Range = TOTALCHARS;
        encMap[ENC_BL].ValueStr = "";
        encMap[ENC_BL].ParameterStr = ParameterStrMap[choosecharacterBank];
        encMap[ENC_BL].Push = true;
        encMap[ENC_BL].PushAction = State::CHOOSECHARBANK;

        encMap[ENC_BR].active = true;
        encMap[ENC_BR].Parameter = savebank;
        encMap[ENC_BR].ShowValue = false;
        encMap[ENC_BR].ValueStr = "";
        encMap[ENC_BR].ParameterStr = ParameterStrMap[savebank];
        encMap[ENC_BR].Push = true;
        encMap[ENC_BR].PushAction = State::MAIN;
        break;

    case State::RENAMESEQUENCE:
        encMap[ENC_TL].active = true;
        encMap[ENC_TL].Parameter = deleteCharacterSequence;
        encMap[ENC_TL].ShowValue = false;
        encMap[ENC_TL].Value = 0;
        encMap[ENC_TL].Range = 0;
        encMap[ENC_TL].ValueStr = "";
        encMap[ENC_TL].ParameterStr = ParameterStrMap[deleteCharacterSequence];
        encMap[ENC_TL].Push = true;
        encMap[ENC_TL].PushAction = State::DELETECHARSEQUENCE;

        encMap[ENC_TR].active = true;
        encMap[ENC_TR].Parameter = cancel;
        encMap[ENC_TR].ShowValue = false;
        encMap[ENC_TR].Value = 0;
        encMap[ENC_TR].Range = 0;
        encMap[ENC_TR].ValueStr = "";
        encMap[ENC_TR].ParameterStr = ParameterStrMap[cancel];
        encMap[ENC_TR].Push = true;
        encMap[ENC_TR].PushAction = State::SEQUENCEEDIT;

        encMap[ENC_BL].active = true;
        encMap[ENC_BL].Parameter = choosecharacterSequence;
        encMap[ENC_BL].ShowValue = false;
        encMap[ENC_BL].Value = 0;
        encMap[ENC_BL].Range = TOTALCHARS;
        encMap[ENC_BL].ValueStr = "";
        encMap[ENC_BL].ParameterStr = ParameterStrMap[choosecharacterSequence];
        encMap[ENC_BL].Push = true;
        encMap[ENC_BL].PushAction = State::CHOOSECHARSEQUENCE;

        encMap[ENC_BR].active = false;
        break;
    case State::FIRMWARECONFIRM:
        // SD card firmware update: Cancel (bottom left) or Update (bottom right)
        configureOff(&encMap[ENC_TL]);
        configureOff(&encMap[ENC_TR]);
        encMap[ENC_BL].active = true;
        encMap[ENC_BL].Parameter = firmwarecancel;
        encMap[ENC_BL].ShowValue = false;
        encMap[ENC_BL].Value = 0;
        encMap[ENC_BL].Range = 127;
        encMap[ENC_BL].ValueStr = "";
        encMap[ENC_BL].ParameterStr = ParameterStrMap[firmwarecancel];
        encMap[ENC_BL].Push = true;
        encMap[ENC_BL].PushAction = State::MAIN;
        encMap[ENC_BR].active = true;
        encMap[ENC_BR].Parameter = firmwareupdate;
        encMap[ENC_BR].ShowValue = false;
        encMap[ENC_BR].Value = 0;
        encMap[ENC_BR].Range = 127;
        encMap[ENC_BR].ValueStr = "";
        encMap[ENC_BR].ParameterStr = ParameterStrMap[firmwareupdate];
        encMap[ENC_BR].Push = true;
        encMap[ENC_BR].PushAction = State::FIRMWAREUPDATE;
        break;
    case State::FIRMWAREUPDATE:
    case State::FIRMWAREINSTALL:
    case State::FIRMWAREUPDATE_ABORT:
    case State::FIRMWAREDELETE:
        configureOff(&encMap[ENC_TL]);
        configureOff(&encMap[ENC_TR]);
        configureOff(&encMap[ENC_BL]);
        configureOff(&encMap[ENC_BR]);
        break;
    default:
        break;
    }
}

FLASHMEM void setConfigurationForCC(uint8_t enc, uint8_t cc)
{
    LockGuard lock(displayLock); // The display renders from encMap
    if (isFMParam(cc))
    {
        configureFM(&encMap[enc], cc - FM_PARAM_BASE, State::PERFORMANCEPAGE);
        return;
    }
    switch (cc)
    {
    case CCglide:
        configurePortamento(&encMap[enc], State::PERFORMANCEPAGE);
        break;
    case CCfilterfreq:
        configureCutoff(&encMap[enc], State::PERFORMANCEPAGE);
        break;
    case CCfilterres:
        configureResonance(&encMap[enc], State::PERFORMANCEPAGE);
        break;
    case pitchbendrange:
        configurePitchBendRange(&encMap[enc], State::PERFORMANCEPAGE);
        break;
    case CCmonomode:
        configureMonoMode(&encMap[enc], State::PERFORMANCEPAGE);
        break;
    case CCensemblefxamt:
        configureCCensemblefxamt(&encMap[enc], State::PERFORMANCEPAGE);
        break;
    case CCensemblefxmix:
        configureCCensemblefxmix(&encMap[enc], State::PERFORMANCEPAGE);
        break;
    case CCensemblefxdepth:
        configureCCensemblefxdepth(&encMap[enc], State::PERFORMANCEPAGE);
        break;
    case CCensemblefxdelay:
        configureCCensemblefxdelay(&encMap[enc], State::PERFORMANCEPAGE);
        break;
    case CCreverbfxtime:
        configureCCreverbfxtime(&encMap[enc], State::PERFORMANCEPAGE);
        break;
    case CCreverbfxmix:
        configureCCreverbfxmix(&encMap[enc], State::PERFORMANCEPAGE);
        break;
    case CCreverbfxdamping:
        configureCCreverbfxdamping(&encMap[enc], State::PERFORMANCEPAGE);
        break;
    case CCreverbfxbasscut:
        configureCCreverbfxbasscut(&encMap[enc], State::PERFORMANCEPAGE);
        break;
    default:
        configureOff(&encMap[enc]);
        break;
    }
}
