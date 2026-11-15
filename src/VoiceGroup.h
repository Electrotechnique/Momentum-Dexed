/*
  Momentum FM: the voice group is one Dexed engine (Synth_Dexed, GPLv3).

  It keeps the interface the rest of Momentum uses (arpeggiator, sequencer, MIDI, performance,
  patch and settings code) and adds access to the DX7 voice data for the FM editing pages.

  Operators are numbered 1-6 as on a DX7. In the voice data OP6 comes first, so operator n's
  21 parameters start at (6 - n) * 21.
*/
#ifndef MOMENTUM_VOICE_GROUP_H
#define MOMENTUM_VOICE_GROUP_H

#include <Arduino.h>
#include <vector>
#include <stdint.h>
#include <stddef.h>
#include "Constants.h"
#include "AudioPatching.h"
#include "FMAlgorithms.h"

#define MONOPHONIC_OFF 0
#define MONOPHONIC_LAST 1

#define FM_OPS 6
#define FM_OP_PARAMS 21
#define FM_VOICE_SIZE 155 // Unpacked DX7 voice, including the 10 character name at 145
#define FM_ENGINE_GAIN 0.3f // Dexed output level, see the constructor

class VoiceGroup
{
private:
    String patchName;
    uint32_t patchIndex = 0;
    uint32_t UID = 0;
    Global &g;
    uint8_t monophonic = MONOPHONIC_OFF;
    uint8_t opMask = 0x3F; // Operator on/off, bit (n-1) = OP n
    float ensembleEffectAmount = 0.0f;
    float ensembleEffectMix = 0.0f;
    float reverbEffectTime = 0.0f;
    float reverbEffectMix = 0.0f;

    static inline uint8_t opBase(uint8_t op) { return (FM_OPS - op) * FM_OP_PARAMS; }

    // Dexed numbers the operator switch bits in voice data order (bit 0 = OP6)
    void applyOpMask()
    {
        uint8_t dexedMask = 0;
        for (uint8_t op = 1; op <= FM_OPS; op++)
            if (opMask & (1 << (op - 1)))
                dexedMask |= 1 << (FM_OPS - op);
        g.dexed.setOPAll(dexedMask);
    }

public:
    VoiceGroup(Global &global) : g(global)
    {
        // Each Dexed note can reach half of full scale, and the notes are summed and converted
        // to 16 bits inside Dexed, before Momentum's volume: two loud notes clipped. 0.3 leaves
        // room for about six full level notes (more for most patches), like VOICEMIXERLEVEL does
        // for the subtractive engine, and keeps the ensemble and reverb out of clipping.
        g.dexed.setGain(FM_ENGINE_GAIN);
        g.dexed.loadInitVoice();
        applyOpMask();
        setEnsembleEffectMix(0.0f);
    }

    // ---- Patch identity ----
    inline String getPatchName() { return patchName; }
    inline void setPatchName(String name) { patchName = name; }
    inline uint32_t getPatchIndex() { return patchIndex; }
    inline void setPatchIndex(uint32_t index) { patchIndex = index; }
    inline uint32_t getUID() { return UID; }
    inline void setUID(uint32_t uid) { UID = uid; }

    // ---- FM voice data ----
    uint8_t getOpParam(uint8_t op, uint8_t param) { return g.dexed.getVoiceDataElement(opBase(op) + param); }
    void setOpParam(uint8_t op, uint8_t param, uint8_t value) { g.dexed.setVoiceDataElement(opBase(op) + param, value); }
    uint8_t getVoiceParam(uint8_t param) { return g.dexed.getVoiceDataElement(DEXED_VOICE_OFFSET + param); }
    void setVoiceParam(uint8_t param, uint8_t value) { g.dexed.setVoiceDataElement(DEXED_VOICE_OFFSET + param, value); }

    void getVoiceData(uint8_t *data) // FM_VOICE_SIZE bytes
    {
        uint8_t all[NUM_VOICE_PARAMETERS];
        g.dexed.getVoiceData(all);
        memcpy(data, all, FM_VOICE_SIZE);
    }

    void setVoiceData(uint8_t *data) // FM_VOICE_SIZE bytes. Stops sounding notes.
    {
        g.dexed.loadVoiceParameters(data);
    }

    void loadInitVoice() { g.dexed.loadInitVoice(); }

    // Unpacks a 128 byte voice of a DX7 bank (VMEM) into 155 bytes. Stops sounding notes.
    void decodeVoice(uint8_t *voice, uint8_t *packed) { g.dexed.decodeVoice(voice, packed); }

    bool getOpEnabled(uint8_t op) { return opMask & (1 << (op - 1)); }
    uint8_t getOpMask() { return opMask; }
    void setOpMask(uint8_t mask)
    {
        opMask = mask & 0x3F;
        applyOpMask();
    }
    void setOpEnabled(uint8_t op, bool on)
    {
        on ? opMask |= (1 << (op - 1)) : opMask &= ~(1 << (op - 1));
        applyOpMask();
    }

    // Carriers of the current algorithm, bit (n-1) = OP n
    uint8_t carrierMask() { return fmCarrierMask(getVoiceParam(DEXED_ALGORITHM)); }

    // ---- Notes ----
    void noteOn(uint8_t note, uint8_t velocity) { g.dexed.keydown(note, velocity); }
    void noteOff(uint8_t note) { g.dexed.keyup(note); }
    void allNotesOff() { g.dexed.notesOff(); }
    void panic() { g.dexed.panic(); }
    void closeEnvelopes() { g.dexed.notesOff(); }
    void setSustain(bool on) { g.dexed.setSustain(on); }
    void pitchBend(int16_t bend) { g.dexed.setPitchbend(bend); } // -8192 to 8191
    void setPitchbendRange(uint8_t semitones) { g.dexed.setPitchbendRange(semitones); }
    void setModWheel(uint8_t value) { g.dexed.setModWheel(value); }
    // Range 0-99, target bitmapped: 1 pitch, 2 amp, 4 EG bias. (Dexed's setModWheelRange() limits
    // the range to 12, so the controller is set in one call.)
    void setModWheelController(uint8_t range, uint8_t target) { g.dexed.setMWController(range, target, 0); }
    void setAftertouch(uint8_t value) { g.dexed.setAftertouch(value); }
    void setAftertouchController(uint8_t range, uint8_t target) { g.dexed.setATController(range, target, 0); }
    void setBreathController(uint8_t value) { g.dexed.setBreathController(value); }
    void setFootController(uint8_t value) { g.dexed.setFootController(value); }
    void setPortamento(uint8_t mode, uint8_t glissando, uint8_t time) { g.dexed.setPortamento(mode, glissando, time); }
    void setMasterTune(int8_t tune) { g.dexed.setMasterTune(tune); }
    void setFilterCutoff(float cutoff) { g.dexed.setFilterCutoff(cutoff); }
    void setFilterResonance(float reso) { g.dexed.setFilterResonance(reso); }
    void setEngineType(uint8_t engine) { g.dexed.setEngineType(engine); }
    uint8_t getEngineType() { return g.dexed.getEngineType(); }

    inline void setMonophonic(uint8_t mode)
    {
        monophonic = mode;
        g.dexed.setMonoMode(mode != MONOPHONIC_OFF);
    }
    inline uint8_t getMonophonicMode() { return monophonic; }
    void setSameVoiceRetrig(bool) {} // Dexed always reuses the voice of a repeated note

    // Is voice i sounding, and which note (for the voice grid)
    bool voiceOn(uint8_t i) { return i < FM_MAX_NOTES && g.dexed.voices[i].live; }
    uint8_t size() { return FM_MAX_NOTES; }

    // The audio processing overran: stop the oldest sounding note
    void shedVoice()
    {
        int8_t oldest = -1;
        for (uint8_t i = 0; i < FM_MAX_NOTES; i++)
            if (g.dexed.voices[i].live && (oldest < 0 || g.dexed.voices[i].key_pressed_timer < g.dexed.voices[oldest].key_pressed_timer))
                oldest = i;
        if (oldest >= 0)
            g.dexed.voices[oldest].live = false;
    }

    void serviceVoices() {}
    void updateVoices() {}

    void setVolume(float amount) { g.volumeMixer.gain(0, amount); }

    // ---- MIDI clock: the DX7 LFO has no clock sync ----
    void midiClockStart() {}
    void midiClockStop() {}
    void midiClockBeat(uint32_t, float) {}

    // ---- Effects (as the subtractive Momentum) ----
    float getEnsebleEffectAmount() { return ensembleEffectAmount; }
    float getEnsembleEffectMix() { return ensembleEffectMix; }
    float getReverbEffectTime() { return reverbEffectTime; }
    float getReverbEffectMix() { return reverbEffectMix; }

    void setEnsembleEffectAmount(float value)
    {
        ensembleEffectAmount = value;
        g.ensemble.lfoRate(value);
    }

    void setEnsembleEffectMix(float value)
    {
        ensembleEffectMix = value;
        g.ensembleEffectMixerL.gain(0, 1.0f - value); // Dry
        g.ensembleEffectMixerL.gain(1, value);        // Wet
        g.ensembleEffectMixerR.gain(0, 1.0f - value);
        g.ensembleEffectMixerR.gain(1, value);
    }

    void setEnsembleEffectDepth(float value) { g.ensemble.depth(value); }
    void setEnsembleEffectDelay(float samples) { g.ensemble.baseDelay(samples); }
    float getEnsembleEffectDelayMs() { return g.ensemble.effectiveDelaySamples() * 1000.0f / AUDIO_SAMPLE_RATE_EXACT; }

    void setReverbEffectTime(float value)
    {
        reverbEffectTime = value;
        g.reverb.size(value);
    }
    void setReverbEffectDamping(float value) { g.reverb.hidamp(value); }
    void setReverbEffectBassCut(float value) { g.reverb.lodamp(value); }
    void setReverbEffectMix(float value)
    {
        reverbEffectMix = value;
        g.reverb.mix(value / 2.0f);             // Up to 50:50
        g.reverb.bypass_set(value <= 0.0f);     // Bypassed at zero mix to save CPU time
    }
};

#endif
