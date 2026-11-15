/*
  Momentum FM audio graph

  dexed (mono) --> volumeMixer --> ensemble (stereo) --> ensembleEffectMixerL/R (dry + wet)
               --> reverb (stereo) --> I2S (PCM5102A) / USB audio / peak meter

  The Dexed FM engine is Synth_Dexed (https://codeberg.org/dcoredump/Synth_Dexed), GPLv3.
  The ensemble and plate reverb are the same effects as the subtractive Momentum.
*/

#ifndef MOMENTUM_AUDIO_PATCHING_H
#define MOMENTUM_AUDIO_PATCHING_H

#include <vector>
#include "Constants.h"

// Polyphony of the Dexed engine. Each note costs ~700 bytes (Dx7Note) and CPU while sounding.
// MicroDexed allows up to 32 on a Teensy 4.x; 16 leaves CPU for the ensemble and reverb. Measure before raising.
#define FM_MAX_NOTES 16

struct Global
{
public:
    AudioSynthDexed dexed{FM_MAX_NOTES, 44100};
    AudioMixer4TS volumeMixer;
    AudioEffectEnsemble ensemble;
    AudioMixer4TS ensembleEffectMixerL;
    AudioMixer4TS ensembleEffectMixerR;
    AudioEffectPlateReverb reverb; // Stereo, in series after the ensemble
    AudioOutputUSB usbAudio;
    AudioAnalyzePeak peak;
    Oscilloscope scope;
    AudioCpuGuard cpuGuard; // Drops notes if the audio processing overruns, rather than hang
    AudioOutputI2S i2s;

    AudioConnection connectionsArray[15] = {
        {dexed, 0, volumeMixer, 0},
        {volumeMixer, 0, ensemble, 0},
        {ensemble, 0, ensembleEffectMixerL, 1},
        {ensemble, 1, ensembleEffectMixerR, 1},
        {volumeMixer, 0, ensembleEffectMixerL, 0},
        {volumeMixer, 0, ensembleEffectMixerR, 0},
        {ensembleEffectMixerL, 0, reverb, 0},
        {ensembleEffectMixerR, 0, reverb, 1},
        {reverb, 0, peak, 0},
        {reverb, 0, i2s, 0},
        {reverb, 1, i2s, 1},
        {reverb, 0, usbAudio, 0},
        {reverb, 1, usbAudio, 1},
        {reverb, 0, cpuGuard, 0}, // Only so it runs every update
        // Scope taps the engine before the effects for a clean trace
        {dexed, 0, scope, 0}};

    Global(float mixerLevel)
    {
        // Fixed reverb tone settings (0.0 - 1.0). Size and mix are patch parameters.
        reverb.hidamp(0.5f);
        reverb.lodamp(0.2f);
        reverb.lowpass(0.3f);
        reverb.hipass(0.0f);
        reverb.diffusion(1.0f);

        volumeMixer.gain(0, mixerLevel); // Set by VoiceGroup setVolume(float amount)
        volumeMixer.gain(1, 0);
        volumeMixer.gain(2, 0);
        volumeMixer.gain(3, 0);
    }

    inline int maxVoices() { return FM_MAX_NOTES; }
    inline int maxTimbre() { return 1; }
    inline int maxVoicesPerGroup() { return FM_MAX_NOTES; }
    inline int maxTimbres() { return 1; }
};

#endif
