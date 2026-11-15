/*
  Momentum FM: editing the DX7 voice with the four encoders.

  On the Op pages the top left encoder always selects the operator (1-6), and the page
  stays the same when the operator changes. Envelopes are edited one stage at a time:
  one encoder selects the stage (1-4), two set its rate and level.

  FM parameters use encoder codes FM_PARAM_BASE + id (see FM_PARAMS). They don't go
  through myControlChange(): fmEncoderTurn() and fmEncoderPush() handle them.
*/
#ifndef MOMENTUM_FM_EDIT_H
#define MOMENTUM_FM_EDIT_H

#include "FMCodes.h"

uint8_t fmOp = 1;     // Operator being edited, 1-6
uint8_t fmStage = 1;  // Operator envelope stage, 1-4
uint8_t fmPStage = 1; // Pitch envelope stage, 1-4

typedef enum FMKind : uint8_t
{
    FMK_OP,       // Operator parameter, index = DexedVoiceOPParameters
    FMK_OP_RATE,  // Operator envelope rate of fmStage
    FMK_OP_LEVEL, // Operator envelope level of fmStage
    FMK_OP_ON,    // Operator on/off
    FMK_VOICE,    // Voice parameter, index = DexedVoiceParameters
    FMK_P_RATE,   // Pitch envelope rate of fmPStage
    FMK_P_LEVEL,  // Pitch envelope level of fmPStage
    FMK_MW_RANGE, // Mod wheel range (patch, not voice data)
    FMK_MW_TARGET,
    FMK_AT_RANGE, // Aftertouch range
    FMK_AT_TARGET,
    FMK_ENGINE, // Dexed engine type
} FMKind;

typedef enum FMFormat : uint8_t
{
    FMF_NUM,       // 0..max
    FMF_ONOFF,     // Off/On
    FMF_MODE,      // Ratio/Fixed
    FMF_COARSE,    // Coarse frequency (0 is 0.50 in ratio mode)
    FMF_DETUNE,    // -7..+7
    FMF_NOTE,      // Break point: A-1..C8
    FMF_CURVE,     // -Lin -Exp +Exp +Lin
    FMF_PLUS1,     // Algorithm 1-32
    FMF_TRANSPOSE, // C1..C5
    FMF_CENTRE50,  // Pitch envelope level -50..+49
    FMF_LFOWAVE,   // Triangle...S/Hold
    FMF_TARGET,    // Controller target bits
    FMF_ENGINE,    // Modern / Mark I / OPL
} FMFormat;

typedef struct FMParamDef
{
    const char *name;
    FMKind kind;
    uint8_t index;
    uint8_t max;
    uint8_t defaultValue; // Encoder push, as in the Dexed init voice
    FMFormat format;
} FMParamDef;


const FMParamDef FM_PARAMS[FMP_COUNT] = {
    {"Mode", FMK_OP, DEXED_OP_OSC_MODE, 1, 0, FMF_MODE},
    {"Coarse", FMK_OP, DEXED_OP_FREQ_COARSE, 31, 1, FMF_COARSE},
    {"Fine", FMK_OP, DEXED_OP_FREQ_FINE, 99, 0, FMF_NUM},
    {"Detune", FMK_OP, DEXED_OP_OSC_DETUNE, 14, 7, FMF_DETUNE},
    {"Level", FMK_OP, DEXED_OP_OUTPUT_LEV, 99, 0, FMF_NUM},
    {"On/Off", FMK_OP_ON, 0, 1, 1, FMF_ONOFF},
    {"Velocity", FMK_OP, DEXED_OP_KEY_VEL_SENS, 7, 0, FMF_NUM},
    {"Amp Mod", FMK_OP, DEXED_OP_AMP_MOD_SENS, 3, 0, FMF_NUM},
    {"Rate Scale", FMK_OP, DEXED_OP_OSC_RATE_SCALE, 7, 0, FMF_NUM},
    {"Rate", FMK_OP_RATE, 0, 99, 99, FMF_NUM},
    {"Level", FMK_OP_LEVEL, 0, 99, 99, FMF_NUM},
    {"Break Point", FMK_OP, DEXED_OP_LEV_SCL_BRK_PT, 99, 39, FMF_NOTE},
    {"Left Depth", FMK_OP, DEXED_OP_SCL_LEFT_DEPTH, 99, 0, FMF_NUM},
    {"Right Depth", FMK_OP, DEXED_OP_SCL_RGHT_DEPTH, 99, 0, FMF_NUM},
    {"Left Curve", FMK_OP, DEXED_OP_SCL_LEFT_CURVE, 3, 0, FMF_CURVE},
    {"Right Curve", FMK_OP, DEXED_OP_SCL_RGHT_CURVE, 3, 0, FMF_CURVE},
    {"Algorithm", FMK_VOICE, DEXED_ALGORITHM, 31, 0, FMF_PLUS1},
    {"Feedback", FMK_VOICE, DEXED_FEEDBACK, 7, 0, FMF_NUM},
    {"Osc Sync", FMK_VOICE, DEXED_OSC_KEY_SYNC, 1, 1, FMF_ONOFF},
    {"Transpose", FMK_VOICE, DEXED_TRANSPOSE, 48, 24, FMF_TRANSPOSE},
    {"Rate", FMK_P_RATE, 0, 99, 99, FMF_NUM},
    {"Level", FMK_P_LEVEL, 0, 99, 50, FMF_CENTRE50},
    {"Speed", FMK_VOICE, DEXED_LFO_SPEED, 99, 35, FMF_NUM},
    {"Delay", FMK_VOICE, DEXED_LFO_DELAY, 99, 0, FMF_NUM},
    {"Pitch Depth", FMK_VOICE, DEXED_LFO_PITCH_MOD_DEP, 99, 0, FMF_NUM},
    {"Amp Depth", FMK_VOICE, DEXED_LFO_AMP_MOD_DEP, 99, 0, FMF_NUM},
    {"Key Sync", FMK_VOICE, DEXED_LFO_SYNC, 1, 1, FMF_ONOFF},
    {"Wave", FMK_VOICE, DEXED_LFO_WAVE, 5, 0, FMF_LFOWAVE},
    {"Pitch Sens", FMK_VOICE, DEXED_LFO_PITCH_MOD_SENS, 7, 3, FMF_NUM},
    {"Wheel Range", FMK_MW_RANGE, 0, 99, 50, FMF_NUM},
    {"Wheel Dest", FMK_MW_TARGET, 0, 7, 1, FMF_TARGET},
    {"AT Range", FMK_AT_RANGE, 0, 99, 0, FMF_NUM},
    {"AT Dest", FMK_AT_TARGET, 0, 7, 0, FMF_TARGET},
    {"Engine", FMK_ENGINE, 0, 2, 0, FMF_ENGINE},
};

const char *FM_NOTE_NAMES[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
const char *FM_CURVE_STR[4] = {"-Lin", "-Exp", "+Exp", "+Lin"};
const char *FM_LFOWAVE_STR[6] = {"Triangle", "Saw Down", "Saw Up", "Square", "Sine", "S/Hold"};
const char *FM_TARGET_STR[8] = {"Off", "Pitch", "Amp", "Pitch+Amp", "EG", "Pitch+EG", "Amp+EG", "All"};
const char *FM_ENGINE_STR[3] = {"Modern", "Mark I", "OPL"};

// MIDI note name with Yamaha octaves (C3 = middle C, note 60)
FLASHMEM String fmNoteName(uint8_t midiNote)
{
    return String(FM_NOTE_NAMES[midiNote % 12]) + String(int(midiNote / 12) - 2);
}

FLASHMEM uint8_t fmGet(uint8_t id)
{
    VoiceGroup *vg = groupvec[activeGroupIndex];
    const FMParamDef &p = FM_PARAMS[id];
    switch (p.kind)
    {
    case FMK_OP:
        return vg->getOpParam(fmOp, p.index);
    case FMK_OP_RATE:
        return vg->getOpParam(fmOp, DEXED_OP_EG_R1 + fmStage - 1);
    case FMK_OP_LEVEL:
        return vg->getOpParam(fmOp, DEXED_OP_EG_L1 + fmStage - 1);
    case FMK_OP_ON:
        return vg->getOpEnabled(fmOp) ? 1 : 0;
    case FMK_VOICE:
        return vg->getVoiceParam(p.index);
    case FMK_P_RATE:
        return vg->getVoiceParam(DEXED_PITCH_EG_R1 + fmPStage - 1);
    case FMK_P_LEVEL:
        return vg->getVoiceParam(DEXED_PITCH_EG_L1 + fmPStage - 1);
    case FMK_MW_RANGE:
        return currentPatch.ModWheelRange;
    case FMK_MW_TARGET:
        return currentPatch.ModWheelTarget;
    case FMK_AT_RANGE:
        return currentPatch.AftertouchRange;
    case FMK_AT_TARGET:
        return currentPatch.AftertouchTarget;
    case FMK_ENGINE:
        return vg->getEngineType();
    }
    return 0;
}

FLASHMEM void fmSet(uint8_t id, uint8_t value)
{
    VoiceGroup *vg = groupvec[activeGroupIndex];
    const FMParamDef &p = FM_PARAMS[id];
    if (value > p.max)
        value = p.max;
    AudioUpdateLock lock; // Dexed renders from the voice data
    switch (p.kind)
    {
    case FMK_OP:
        vg->setOpParam(fmOp, p.index, value);
        break;
    case FMK_OP_RATE:
        vg->setOpParam(fmOp, DEXED_OP_EG_R1 + fmStage - 1, value);
        break;
    case FMK_OP_LEVEL:
        vg->setOpParam(fmOp, DEXED_OP_EG_L1 + fmStage - 1, value);
        break;
    case FMK_OP_ON:
        vg->setOpEnabled(fmOp, value);
        currentPatch.OpEnabled = vg->getOpMask();
        break;
    case FMK_VOICE:
        vg->setVoiceParam(p.index, value);
        break;
    case FMK_P_RATE:
        vg->setVoiceParam(DEXED_PITCH_EG_R1 + fmPStage - 1, value);
        break;
    case FMK_P_LEVEL:
        vg->setVoiceParam(DEXED_PITCH_EG_L1 + fmPStage - 1, value);
        break;
    case FMK_MW_RANGE:
    case FMK_MW_TARGET:
        p.kind == FMK_MW_RANGE ? currentPatch.ModWheelRange = value : currentPatch.ModWheelTarget = value;
        vg->setModWheelController(currentPatch.ModWheelRange, currentPatch.ModWheelTarget);
        break;
    case FMK_AT_RANGE:
    case FMK_AT_TARGET:
        p.kind == FMK_AT_RANGE ? currentPatch.AftertouchRange = value : currentPatch.AftertouchTarget = value;
        vg->setAftertouchController(currentPatch.AftertouchRange, currentPatch.AftertouchTarget);
        break;
    case FMK_ENGINE:
        vg->setEngineType(value);
        break;
    }
}

FLASHMEM String fmValueStr(uint8_t id, uint8_t v)
{
    switch (FM_PARAMS[id].format)
    {
    case FMF_ONOFF:
        return v ? F("On") : F("Off");
    case FMF_MODE:
        return v ? F("Fixed") : F("Ratio");
    case FMF_COARSE:
        if (groupvec[activeGroupIndex]->getOpParam(fmOp, DEXED_OP_OSC_MODE))
            return String(v & 3); // Fixed: 1, 10, 100, 1000Hz
        return v == 0 ? String(F("0.50")) : String(v);
    case FMF_DETUNE:
        return (v > 7 ? "+" : "") + String(int(v) - 7);
    case FMF_NOTE:
        return fmNoteName(v + 21); // 0 = A-1
    case FMF_CURVE:
        return FM_CURVE_STR[v & 3];
    case FMF_PLUS1:
        return String(v + 1);
    case FMF_TRANSPOSE:
        return fmNoteName(v + 36); // 24 = C3
    case FMF_CENTRE50:
        return (v > 50 ? "+" : "") + String(int(v) - 50);
    case FMF_LFOWAVE:
        return FM_LFOWAVE_STR[v < 6 ? v : 0];
    case FMF_TARGET:
        return FM_TARGET_STR[v & 7];
    case FMF_ENGINE:
        return FM_ENGINE_STR[v < 3 ? v : 0];
    default:
        return String(v);
    }
}

// The frequency of operator fmOp, as on a DX7: "x1.00" ratio or "440 Hz" fixed
FLASHMEM String fmFrequencyStr(uint8_t op)
{
    VoiceGroup *vg = groupvec[activeGroupIndex];
    uint8_t coarse = vg->getOpParam(op, DEXED_OP_FREQ_COARSE);
    uint8_t fine = vg->getOpParam(op, DEXED_OP_FREQ_FINE);
    if (vg->getOpParam(op, DEXED_OP_OSC_MODE))
    {
        float hz = powf(10.0f, coarse & 3) * powf(10.0f, fine / 100.0f);
        return String(hz, hz < 10.0f ? 2 : (hz < 100.0f ? 1 : 0)) + F(" Hz");
    }
    float ratio = (coarse == 0 ? 0.5f : coarse) * (1.0f + fine / 100.0f);
    return String(F("x")) + String(ratio, 2);
}

FLASHMEM void fmAssignStrings()
{
    for (uint8_t i = 0; i < FMP_COUNT; i++)
        ParameterStrMap[FM_PARAM_BASE + i] = FM_PARAMS[i].name;
    ParameterStrMap[fmOpSelect] = "Operator";
    ParameterStrMap[fmStageSelect] = "Stage";
    ParameterStrMap[fmPStageSelect] = "Stage";
    ParameterStrMap[CCglide] = "Portamento";
    ParameterStrMap[CCfilterfreq] = "Cutoff";
    ParameterStrMap[CCfilterres] = "Resonance";
    ParameterStrMap[pitchbendrange] = "Bend Range";
    ParameterStrMap[CCmonomode] = "Mono";
    ParameterStrMap[syxPage] = "DX7 .syx";
    ParameterStrMap[syxImport] = "Import File";
    ParameterStrMap[syxExport] = "Export Bank";
}

FLASHMEM void configureFM(EncoderMappingStruct *enc, uint8_t id, State st)
{
    const FMParamDef &p = FM_PARAMS[id];
    enc->active = true;
    enc->Parameter = FM_PARAM_BASE + id;
    enc->ShowValue = true;
    enc->Value = fmGet(id);
    enc->DefaultValue = p.defaultValue;
    enc->Range = p.max;
    enc->Counter = 0;
    enc->ValueStr = fmValueStr(id, enc->Value);
    enc->ParameterStr = p.name;
    enc->Push = true;
    enc->PushAction = st;
}

// Selectors: operator 1-6 or stage 1-4, Value is 0 based
FLASHMEM void configureFMSelect(EncoderMappingStruct *enc, uint8_t code, State st)
{
    uint8_t current = code == fmOpSelect ? fmOp : (code == fmStageSelect ? fmStage : fmPStage);
    enc->active = true;
    enc->Parameter = code;
    enc->ShowValue = true;
    enc->Value = current - 1;
    enc->DefaultValue = 0;
    enc->Range = code == fmOpSelect ? 5 : 3;
    enc->Counter = 0;
    enc->ValueStr = String(current);
    enc->ParameterStr = ParameterStrMap[code];
    enc->Push = true;
    enc->PushAction = st;
}

// Patch level controls on the Ctrl and Tone pages (MIDI CCs, through myControlChange())
FLASHMEM void configureFMCC(EncoderMappingStruct *enc, uint8_t cc, uint8_t value, uint8_t range, uint8_t defaultValue, String str, State st)
{
    enc->active = true;
    enc->Parameter = cc;
    enc->ShowValue = true;
    enc->Value = value;
    enc->DefaultValue = defaultValue;
    enc->Range = range;
    enc->Counter = 0;
    enc->ValueStr = str;
    enc->ParameterStr = ParameterStrMap[cc];
    enc->Push = true;
    enc->PushAction = st;
}

FLASHMEM void configurePitchBendRange(EncoderMappingStruct *enc, State st)
{
    configureFMCC(enc, pitchbendrange, currentPatch.PitchBend, 12, 2, String(currentPatch.PitchBend), st);
}

FLASHMEM void configureMonoMode(EncoderMappingStruct *enc, State st)
{
    configureFMCC(enc, CCmonomode, currentPatch.MonophonicMode, 1, 0, currentPatch.MonophonicMode ? F("On") : F("Off"), st);
}

FLASHMEM void configurePortamento(EncoderMappingStruct *enc, State st)
{
    uint8_t v = (currentPatch.PortamentoTime * 127 + 98) / 99;
    configureFMCC(enc, CCglide, v, 127, 0, currentPatch.PortamentoTime == 0 ? String(F("Off")) : String(currentPatch.PortamentoTime), st);
}

FLASHMEM void configureCutoff(EncoderMappingStruct *enc, State st)
{
    configureFMCC(enc, CCfilterfreq, currentPatch.FilterFreq, 127, 127, String(int(currentPatch.FilterFreq * 100 / 127.0f + 0.5f)) + F("%"), st);
}

FLASHMEM void configureResonance(EncoderMappingStruct *enc, State st)
{
    configureFMCC(enc, CCfilterres, currentPatch.FilterRes, 127, 0, String(int(currentPatch.FilterRes * 100 / 127.0f + 0.5f)) + F("%"), st);
}

#include "FMSyx.h"

// The encoders of the FM pages
FLASHMEM bool setFMEncodersState(State s)
{
    switch (s)
    {
    case State::SYXPAGE:
        configureSyxPage();
        return true;
    case State::OPPAGE1:
        configureFMSelect(&encMap[ENC_TL], fmOpSelect, s);
        configureFM(&encMap[ENC_TR], FMP_MODE, s);
        configureFM(&encMap[ENC_BL], FMP_COARSE, s);
        configureFM(&encMap[ENC_BR], FMP_FINE, s);
        return true;
    case State::OPPAGE2:
        configureFMSelect(&encMap[ENC_TL], fmOpSelect, s);
        configureFM(&encMap[ENC_TR], FMP_ON, s);
        configureFM(&encMap[ENC_BL], FMP_DETUNE, s);
        configureFM(&encMap[ENC_BR], FMP_LEVEL, s);
        return true;
    case State::OPPAGE3:
        configureFMSelect(&encMap[ENC_TL], fmOpSelect, s);
        configureFM(&encMap[ENC_TR], FMP_VELSENS, s);
        configureFM(&encMap[ENC_BL], FMP_AMS, s);
        configureFM(&encMap[ENC_BR], FMP_RATESCALE, s);
        return true;
    case State::OPPAGE4:
        configureFMSelect(&encMap[ENC_TL], fmOpSelect, s);
        configureFMSelect(&encMap[ENC_TR], fmStageSelect, s);
        configureFM(&encMap[ENC_BL], FMP_EGRATE, s);
        configureFM(&encMap[ENC_BR], FMP_EGLEVEL, s);
        return true;
    case State::OPPAGE5:
        configureFMSelect(&encMap[ENC_TL], fmOpSelect, s);
        configureFM(&encMap[ENC_TR], FMP_BREAKPT, s);
        configureFM(&encMap[ENC_BL], FMP_LDEPTH, s);
        configureFM(&encMap[ENC_BR], FMP_RDEPTH, s);
        return true;
    case State::OPPAGE6:
        configureFMSelect(&encMap[ENC_TL], fmOpSelect, s);
        configureOff(&encMap[ENC_TR]);
        configureFM(&encMap[ENC_BL], FMP_LCURVE, s);
        configureFM(&encMap[ENC_BR], FMP_RCURVE, s);
        return true;
    case State::ALGOPAGE1:
        configureFM(&encMap[ENC_TL], FMP_ALGORITHM, s);
        configureFM(&encMap[ENC_TR], FMP_FEEDBACK, s);
        configureFM(&encMap[ENC_BL], FMP_OSCSYNC, s);
        configureFM(&encMap[ENC_BR], FMP_TRANSPOSE, s);
        return true;
    case State::ALGOPAGE2:
        configureFMSelect(&encMap[ENC_TL], fmPStageSelect, s);
        configureOff(&encMap[ENC_TR]);
        configureFM(&encMap[ENC_BL], FMP_PEGRATE, s);
        configureFM(&encMap[ENC_BR], FMP_PEGLEVEL, s);
        return true;
    case State::LFOPAGE1:
        configureFM(&encMap[ENC_TL], FMP_LFOSPEED, s);
        configureFM(&encMap[ENC_TR], FMP_LFODELAY, s);
        configureFM(&encMap[ENC_BL], FMP_LFOPMD, s);
        configureFM(&encMap[ENC_BR], FMP_LFOAMD, s);
        return true;
    case State::LFOPAGE2:
        configureFM(&encMap[ENC_TL], FMP_LFOWAVE, s);
        configureFM(&encMap[ENC_TR], FMP_LFOSYNC, s);
        configureFM(&encMap[ENC_BL], FMP_LFOPMS, s);
        configureOff(&encMap[ENC_BR]);
        return true;
    case State::CTRLPAGE1:
        configurePitchBendRange(&encMap[ENC_TL], s);
        configureMonoMode(&encMap[ENC_TR], s);
        configurePortamento(&encMap[ENC_BL], s);
        configureOff(&encMap[ENC_BR]);
        return true;
    case State::CTRLPAGE2:
        configureFM(&encMap[ENC_TL], FMP_MWRANGE, s);
        configureFM(&encMap[ENC_TR], FMP_MWTARGET, s);
        configureFM(&encMap[ENC_BL], FMP_ATRANGE, s);
        configureFM(&encMap[ENC_BR], FMP_ATTARGET, s);
        return true;
    case State::TONEPAGE1:
        configureCutoff(&encMap[ENC_TL], s);
        configureResonance(&encMap[ENC_TR], s);
        configureFM(&encMap[ENC_BL], FMP_ENGINE, s);
        configureOff(&encMap[ENC_BR]);
        return true;
    default:
        return false;
    }
}

// Shows a changed FM value on the encoder that has it (stage-dependent values share an id)
FLASHMEM void fmShowValue(uint8_t id)
{
    for (uint8_t i = 0; i < 4; i++)
        if (encMap[i].Parameter == FM_PARAM_BASE + id)
        {
            encMap[i].Value = fmGet(id);
            encMap[i].ValueStr = fmValueStr(id, encMap[i].Value);
        }
}

// Turning an encoder with an FM parameter or selector. Returns false if it isn't one.
// newDelta is from encScaling(), which has already moved enc->Value within its range.
FLASHMEM bool fmEncoderTurn(EncoderMappingStruct *enc, int8_t newDelta)
{
    uint8_t code = enc->Parameter;
    if (code == fmOpSelect || code == fmStageSelect || code == fmPStageSelect)
    {
        if (newDelta == 0)
            return true;
        uint8_t v = enc->Value + 1;
        code == fmOpSelect ? fmOp = v : (code == fmStageSelect ? fmStage = v : fmPStage = v);
        setFMEncodersState(state); // Other encoders now show this operator/stage
        return true;
    }
    if (!isFMParam(code))
        return false;
    if (newDelta == 0)
        return true;
    uint8_t id = code - FM_PARAM_BASE;
    fmSet(id, enc->Value);
    enc->ValueStr = fmValueStr(id, enc->Value);
    if (id == FMP_MODE)
        setFMEncodersState(state); // Coarse shows differently in fixed mode
    return true;
}

// Pushing an encoder with an FM parameter: back to its init voice value. Returns false if it isn't one.
FLASHMEM bool fmEncoderPush(EncoderMappingStruct *enc)
{
    uint8_t code = enc->Parameter;
    if (code == fmOpSelect || code == fmStageSelect || code == fmPStageSelect)
        return true; // Nothing to reset
    if (!isFMParam(code))
        return false;
    uint8_t id = code - FM_PARAM_BASE;
    enc->Value = enc->DefaultValue;
    fmSet(id, enc->Value);
    enc->ValueStr = fmValueStr(id, enc->Value);
    return true;
}

#endif
