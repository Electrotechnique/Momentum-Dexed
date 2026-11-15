#include <Arduino.h>
#include "Constants.h"
#include "MidiCC.h"
#include "FMCodes.h"

const uint8_t PARAMETERSFORENCS = 60;
const char *ParameterStrMap[256] = {};
uint8_t ParametersForPerformanceEncoders[PARAMETERSFORENCS] = {};
uint8_t perfParamCount = 1; // Entries used in ParametersForPerformanceEncoders

FLASHMEM void assignStrings()
{
    ParameterStrMap[CCosclfoamt] = "Pitch LFO Amount";
    ParameterStrMap[CCglide] = "Glide Time";
    ParameterStrMap[CCvolume] = "Volume";
    ParameterStrMap[CCoscwaveformA] = "Waveform 1";
    ParameterStrMap[CCoscwaveformB] = "Waveform 2";
    ParameterStrMap[CCfilterenv] = "Filter Envelope";
    ParameterStrMap[CCfiltermixer] = "Filter Type";
    ParameterStrMap[CCoscLevelA] = "Level 1";
    ParameterStrMap[CCoscLevelB] = "Level 2";
    ParameterStrMap[CCnoiseLevel] = "Noise Level";
    ParameterStrMap[CCoscfx] = "Osc Effect";
    ParameterStrMap[CCpitchA] = "Pitch 1";
    ParameterStrMap[CCpitchB] = "Pitch 2";
    ParameterStrMap[CCpitchenv] = "Pitch Envelope";
    ParameterStrMap[CCosclforetrig] = "Pitch LFO Retrig";
    ParameterStrMap[CCfilterlforetrig] = "Filter LFO Retrig";
    ParameterStrMap[CCfilterres] = "Resonance";
    ParameterStrMap[CCamprelease] = "Release";
    ParameterStrMap[CCampattack] = "Attack";
    ParameterStrMap[CCfilterfreq] = "Cutoff Freq";
    ParameterStrMap[filterfreq256] = "Cutoff Freq";
    ParameterStrMap[CCampdecay] = "Decay";
    ParameterStrMap[CCfilterlforate] = "Filter LFO Rate";
    ParameterStrMap[CCfilterlfoamt] = "Filter LFO Level";
    ParameterStrMap[CCampsustain] = "Sustain";
    ParameterStrMap[CCfilterattack] = "Filter Attack";
    ParameterStrMap[CCfilterdecay] = "Filter Decay";
    ParameterStrMap[CCfiltersustain] = "Filter Sustain";
    ParameterStrMap[CCfilterrelease] = "Filter Release";
    ParameterStrMap[CCpwA] = "Pulse Width";
    ParameterStrMap[CCpwB] = "Pulse Width";
    ParameterStrMap[CCpwmRateA] = "PWM LFO Rate 1";
    ParameterStrMap[CCpwmRateB] = "PWM LFO Rate 2";
    ParameterStrMap[CCpwmAmtA] = "PWM Level 1";
    ParameterStrMap[CCpwmAmtB] = "PWM Level 2";
    ParameterStrMap[CCkeytracking] = "Key Tracking";
    ParameterStrMap[CCfilterlfowaveform] = "Filter LFO Wave";
    ParameterStrMap[CCensemblefxamt] = "Ensemble Rate";
    ParameterStrMap[CCensemblefxmix] = "Ensemble Mix";
    ParameterStrMap[CCreverbfxtime] = "Room Size";
    ParameterStrMap[CCreverbfxmix] = "Reverb Mix";
    ParameterStrMap[CCensemblefxdepth] = "Ensemble Depth";
    ParameterStrMap[CCensemblefxdelay] = "Ensemble Delay";
    ParameterStrMap[CCreverbfxdamping] = "Reverb Damping";
    ParameterStrMap[CCreverbfxbasscut] = "Reverb Bass Cut";
    ParameterStrMap[firmwareupdate] = "Update";
    ParameterStrMap[firmwarecancel] = "Cancel";
    ParameterStrMap[CCdetune] = "Detune";
    ParameterStrMap[CCoscLfoRate] = "Pitch LFO Rate";
    ParameterStrMap[CCoscLfoWaveform] = "Pitch LFO Wave";
    ParameterStrMap[CCfilterLFOMidiClkSync] = "Filter LFO Sync";
    ParameterStrMap[CCoscLFOMidiClkSync] = "Pitch LFO Sync";
    ParameterStrMap[CCpwmSourceA] = "PWM Source 1";
    ParameterStrMap[CCpwmSourceB] = "PWM Source 2";
    ParameterStrMap[CCunison] = "Unison";
    ParameterStrMap[CCmonomode] = "Monophonic";
    ParameterStrMap[patchselect] = "Patch";
    ParameterStrMap[CCbankselectLSB] = "Bank";
    ParameterStrMap[savepatch] = "Save";
    ParameterStrMap[deletepatch] = "Delete";
    ParameterStrMap[renamepatch] = "Rename";
    ParameterStrMap[CCvelocitySens] = "Velocity Sens";
    ParameterStrMap[ampenvshape] = "Amp. Env. Shape";
    ParameterStrMap[filterenvshape] = "Filter Env. Shape";
    ParameterStrMap[CCFilterVelocitySens] = "Filter Vel. Sens";
    ParameterStrMap[glideshape] = "Glide Shape";
    ParameterStrMap[pitchbendrange] = "Pitch Bend";
    ParameterStrMap[pitchmodwheeldepth] = "Pitch Mod W. Dep";
    ParameterStrMap[filtermodwheeldepth] = "Filter Mod W. Dep";
    ParameterStrMap[savebankselect] = "Bank";
    ParameterStrMap[namepatch] = "Name Patch";
    ParameterStrMap[MIDIChIn] = "Channel In";
    ParameterStrMap[MIDIChOut] = "Channel Out";
    ParameterStrMap[MIDIThruMode] = "MIDI Thru Mode";
    ParameterStrMap[choosePerfMIDIChIn] = "Channel In";
    ParameterStrMap[choosePerfMIDIChOut] = "Channel Out";
    ParameterStrMap[choosePerfMIDIThruMode] = "MIDI Thru Mode";
    ParameterStrMap[PerfSelect] = "Performance";
    ParameterStrMap[PerfEdit] = "Edit";
    ParameterStrMap[deleteCharacterPerformance] = "Del. Char";
    ParameterStrMap[choosecharacterPerformance] = "Character";
    ParameterStrMap[cancel] = "Cancel";
    ParameterStrMap[goback] = "Back";
    ParameterStrMap[settingoption] = "Setting";
    ParameterStrMap[settingvalue] = "Value";
    ParameterStrMap[editbank] = "Edit Banks";
    ParameterStrMap[deletepatches] = "Delete";
    ParameterStrMap[deletebank] = "Del Bank";
    ParameterStrMap[renamebank] = "Rename Bank";
    ParameterStrMap[savebank] = "Save Bank";
    ParameterStrMap[bankeditselect] = "Bank";
    ParameterStrMap[choosecharacterPatch] = "Character";
    ParameterStrMap[choosecharacterBank] = "Character";
    ParameterStrMap[choosecharacterSequence] = "Character";
    ParameterStrMap[deleteCharacterPatch] = "Del. Char";
    ParameterStrMap[deleteCharacterBank] = "Del. Char";
    ParameterStrMap[deleteCharacterSequence] = "Del. Char";
    ParameterStrMap[SeqEdit] = "Edit";
    ParameterStrMap[SeqSelect] = "Sequence";
    ParameterStrMap[SeqTempo] = "Tempo";
    ParameterStrMap[SeqStartStop] = "Start/Stop";
    ParameterStrMap[SeqNote] = "Note";
    ParameterStrMap[SeqPosition] = "Position";
    ParameterStrMap[SeqLength] = "Length";
    ParameterStrMap[SeqDelete] = "Delete";
    ParameterStrMap[PerfDelete] = "Delete";
    ParameterStrMap[ArpDivision] = "Division";
    ParameterStrMap[ArpStyle] = "Style";
    ParameterStrMap[ArpRange] = "Range";
    ParameterStrMap[ArpBasis] = "Basis";
    ParameterStrMap[ArpCycle] = "Cycles";
    ParameterStrMap[kbdOct] = "Keyboard Basis";
    ParameterStrMap[kbdScale] = "Keyboard Scale";
    ParameterStrMap[noencoder] = "None";
}

FLASHMEM void assignParametersForPerformanceEncoders()
{
    // Voice-wide parameters only: operator parameters follow the operator selected on the Op pages
    const uint8_t params[] = {noencoder, FM_PARAM_BASE + FMP_ALGORITHM, FM_PARAM_BASE + FMP_FEEDBACK,
                              FM_PARAM_BASE + FMP_LFOSPEED, FM_PARAM_BASE + FMP_LFODELAY, FM_PARAM_BASE + FMP_LFOPMD,
                              FM_PARAM_BASE + FMP_LFOAMD, FM_PARAM_BASE + FMP_LFOWAVE, FM_PARAM_BASE + FMP_TRANSPOSE,
                              FM_PARAM_BASE + FMP_MWRANGE, FM_PARAM_BASE + FMP_ATRANGE,
                              FM_PARAM_BASE + FMP_MACRO_ATTACK, FM_PARAM_BASE + FMP_MACRO_DECAY,
                              FM_PARAM_BASE + FMP_MACRO_DECAY2, FM_PARAM_BASE + FMP_MACRO_RELEASE, CCglide, CCfilterfreq, CCfilterres,
                              pitchbendrange, CCmonomode, CCensemblefxamt, CCensemblefxmix, CCensemblefxdepth,
                              CCensemblefxdelay, CCreverbfxtime, CCreverbfxmix, CCreverbfxdamping, CCreverbfxbasscut};
    for (uint8_t i = 0; i < PARAMETERSFORENCS; i++)
        ParametersForPerformanceEncoders[i] = i < sizeof(params) ? params[i] : noencoder;
    perfParamCount = sizeof(params);
}

FLASHMEM uint8_t getIndexForParametersForPerformanceEncoders(uint8_t cc)
{
    for (uint8_t i = 0; i < PARAMETERSFORENCS; i++)
    {
        if (ParametersForPerformanceEncoders[i] == cc)
            return i;
    }
    if (DEBUG)
        Serial.println(F("getIndexForParametersForPerformanceEncoders() not matched:") + String(cc));
    return 0; // noencoder
}


const char *CDT_STR[128] = {"Major", "Major", "Major", "Major", "Major", "Major", "Major", "Minor", "Minor", "Minor", "Minor", "Minor", "Minor", "Minor", "Diminished", "Diminished", "Diminished", "Diminished", "Diminished", "Diminished", "Diminished", "Augmented", "Augmented", "Augmented", "Augmented", "Augmented", "Augmented", "Augmented", "Sus 2nd", "Sus 2nd", "Sus 2nd", "Sus 2nd", "Sus 2nd", "Sus 2nd", "Sus 2nd", "Sus 4th", "Sus 4th", "Sus 4th", "Sus 4th", "Sus 4th", "Sus 4th", "Sus 4th", "7th Sus 2nd", "7th Sus 2nd", "7th Sus 2nd", "7th Sus 2nd", "7th Sus 2nd", "7th Sus 2nd", "7th Sus 2nd", "7th Sus 4th", "7th Sus 4th", "7th Sus 4th", "7th Sus 4th", "7th Sus 4th", "7th Sus 4th", "7th Sus 4th", "6th", "6th", "6th", "6th", "6th", "6th", "6th", "7th", "7th", "7th", "7th", "7th", "7th", "7th", "9th", "9th", "9th", "9th", "9th", "9th", "9th", "Major 7th", "Major 7th", "Major 7th", "Major 7th", "Major 7th", "Major 7th", "Major 7th", "Major 9th", "Major 9th", "Major 9th", "Major 9th", "Major 9th", "Major 9th", "Major 9th", "Major 11th", "Major 11th", "Major 11th", "Major 11th", "Major 11th", "Major 11th", "Major 11th", "Minor 6th", "Minor 6th", "Minor 6th", "Minor 6th", "Minor 6th", "Minor 6th", "Minor 6th", "Minor 7th", "Minor 7th", "Minor 7th", "Minor 7th", "Minor 7th", "Minor 7th", "Minor 7th", "Minor 9th", "Minor 9th", "Minor 9th", "Minor 9th", "Minor 9th", "Minor 9th", "Minor 9th", "Minor 11th", "Minor 11th", "Minor 11th", "Minor 11th", "Minor 11th", "Minor 11th", "Minor 11th", "All 12", "All 12"};
