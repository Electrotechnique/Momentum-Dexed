/*
   MIT License

  Copyright (c) 2023 ElectroTechnique

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.

------------------------------------------------
            ELECTROTECHNIQUE MOMENTUM
  Teensy MicroMod based Synthesizer - 12 voices
  Firmware Rev 2.00
------------------------------------------------

  Arduino IDE Tools Settings:
    Board: "Teensy MicroMod"
    USB Type: "Serial + MIDI + Audio"
    CPU Speed: "600MHz"
    Optimize: "Faster"

  This code is based on TSynth V2.32 firmware - ElectroTechnique 2021
  and includes code by:
    Dave Benn -  A few bits and original inspiration  https://www.notesandvolts.com/2019/01/teensy-synth-part-10-hardware.html
    Alexander Davis / Vince R. Pearson - Stereo ensemble chorus effect https://github.com/quarterturn/teensy3-ensemble-chorus
    Will Winder - Major refactoring and monophonic mode
    Vince R. Pearson - Exponential envelopes & glide
    Github members fab672000 & CDW2000 - General improvements to code
    Github/PJRC forum member luni64 - EncoderTool library modified https://github.com/luni64/EncoderTool
    PJRC forum member Mark Tillotson - Special thanks for band-limiting the waveforms in the Audio Library

    Any problems with code design and functionality are my own.

  Additional libraries:
    Janila Array, Adafruit_GFX, ArduinoJSON, TeensyTimerTool, TeensyThreads, RokkitHash (available in Arduino libraries manager)
*/
#define DEBUG 0

#include <Adafruit_GFX.h>
#include <ILI9341_t3n.h>
#include <vector>
#include "Audio.h" //Using local version to override Teensyduino version
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <MIDI.h>
#include <USBHost_t36.h>
#include <TeensyThreads.h>
#include "ThreadSafety.h"

// Momentum's audio code works with blocks of any multiple of 16 samples (64 or 128)
static_assert(AUDIO_BLOCK_SAMPLES % 16 == 0 && AUDIO_BLOCK_SAMPLES >= 32, "AUDIO_BLOCK_SAMPLES must be a multiple of 16");
#include <TeensyTimerTool.h>
using namespace TeensyTimerTool;
#include "MidiCC.h"
#include "SettingsService.h"
#include "AudioPatching.h"
#include "Constants.h"
#include "Parameters.h"

FLASHMEM void silence();
volatile boolean cardStatus = false;
volatile boolean sdCardInterrupt = false;
boolean usbHostPluggedIn = false;
elapsedMillis usb_host_wait_timer;
boolean MIDIClkSignal = false;
// MIDI clock counts as lost after this long without a tick (24ppq at 20bpm is 125ms per tick)
#define MIDI_CLOCK_TIMEOUT_MS 500
elapsedMillis sinceMidiClock;

State state = State::MAIN;
boolean keyboardActive = false;

#include "PatchMgr.h"
#include "HWControls.h"
#include "EepromMgr.h"
#include "Keyboard.h"
#include "utils.h"
#include "VoiceGroup.h"

uint8_t majMin = MajMin::MAJOR;
uint8_t keyboardOct = 3;
uint8_t keyboardScale = 0;

// Initialize the audio configuration.
Global global{VOICEMIXERLEVEL};
std::vector<VoiceGroup *> groupvec;
uint8_t activeGroupIndex = 0;

#include "StringMapping.h"
#include "PerformanceMgr.h"

PeriodicTimer sequencer_timer(TCK); // TCK PIT GPT2
#include "SequenceMgr.h"
#include "EncoderMapping.h"

extern void sequencer();
extern void noteOnRoutine();

// USB HOST MIDI Class Compliant
USBHost myusb;
// MIDIDevice usbHostMIDI(myusb);
MIDIDevice_BigBuffer usbHostMIDI(myusb); // Try this if your MIDI Compliant controller has problems

// MIDI 5 Pin DIN - (TRS MIDI)
MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, MIDI);

FLASHMEM void changeMIDIThruMode()
{
    MIDI.turnThruOn(MIDIThru);
}

FLASHMEM void myProgramChange(byte channel, byte program);
void myPitchBend(byte channel, int bend);
void myNoteOff(byte channel, byte note, byte velocity);
void myNoteOn(byte channel, byte note, byte velocity);
FLASHMEM void playNote(uint8_t note);
FLASHMEM void setCurrentPatchData();
FLASHMEM void updatePatchnameAndIndex(String name, uint32_t index, uint32_t UID);
FLASHMEM void storeVoiceInCurrentPatch();
FLASHMEM void recallPatch(uint8_t bank, long patchUID);
FLASHMEM void recallPerformance(uint8_t filename);
FLASHMEM void myControlChange(byte channel, byte control, byte value);
FLASHMEM void usbHostControlChange(byte channel, byte control, byte value);
void myMIDIClock();
FLASHMEM void myMIDIClockContinue();
FLASHMEM void myMIDIClockStart();
FLASHMEM void myMIDIClockStop();
FLASHMEM void sequencerStart(boolean cont = false);
FLASHMEM void sequencerStop();
FLASHMEM void setSeqTimerPeriod(float bpm);

void midiCCOut(byte cc, byte value);
void encoderCallback(unsigned enc_idx, int value, int delta);
void encoderButtonCallback(unsigned button_idx, int state);
void buttonCallback(unsigned button_idx, int state);

#include "Display.h"
#include "Settings.h"
#include "FirmwareUpdate.h"
#include "FMSysex.h"

boolean firstPatchLoaded = false;

uint32_t previousMillis = millis(); // For MIDI Clk Sync
uint32_t timeNow = millis();        // For MIDI Clk Sync

uint8_t midiClkCount = 0;    // For MIDI Clk Sync
uint32_t midiBeatCount = 0;      // MIDI clock beats since Start
uint32_t midiLastBeatMicros = 0; // time of the last beat
uint8_t midiClkArpCount = 0; // For MIDI Clk Sync with Arp
int voiceToReturn = -1;      // Initialise

uint8_t seq_last_step = 0;
uint8_t loopCount = 0;
#define MIDI_READS_PER_LOOP 16 // Max pending messages handled per MIDI input each time they're read
// Set while flushing MIDI input on Panic: incoming messages are read and dropped
volatile boolean midiDiscard = false;


FLASHMEM void setup()
{
    while (DEBUG == 1 && !Serial)
    {
    }
    AudioMemory(60);
    sequencer_timer.begin(sequencer, currentSequence.tempo_us / 2.0f, false);
    checkFirstRun();
    // One voice group: the Dexed FM engine
    groupvec.push_back(new VoiceGroup{global});
    setUpSettings();
    SD.setMediaDetectPin(pinCD);
    cardStatus = SD.begin(BUILTIN_SDCARD);
    if (SD.mediaPresent()) // Card is detected by CD pin
    {
        // ++++ For older sd cards - keep trying
        if (!cardStatus)
            cardStatus = SD.begin(BUILTIN_SDCARD);
        if (!cardStatus)
            cardStatus = SD.begin(BUILTIN_SDCARD);
        if (!cardStatus)
            cardStatus = SD.begin(BUILTIN_SDCARD);
        //++++
    }
    if (cardStatus)
    {
        if (DEBUG)
            Serial.println(F("SD card is connected"));
        loadLastPatchUsed();
        currentSequenceIndex = getLastSequence();
        // Get patch numbers and names from SD card
        checkSDCardStructure();
        loadBankNames();
        loadPatchNamesFromBank(currentBankIndex);
        // If current patch is empty in this bank, go to previous bank with valid patch
        while (patches[0].patchUID == 0)
        {
            tempBankIndex = decTempBankIndex();
            loadPatchNamesFromBank(tempBankIndex);
            currentBankIndex = tempBankIndex;
            currentPatchIndex = 0;
        }
        recallPatch(currentBankIndex, patches[currentPatchIndex].patchUID);
    }
    else
    {
        setCurrentPatchData(); // Initialise to default
        if (DEBUG)
            Serial.println(F("SD card is not connected or unusable"));
    }

    // Read MIDI Channel from EEPROM
    midiChannel = getMIDIChannel();
    if (DEBUG)
        Serial.println(F("MIDI In Channel:") + String(midiChannel) + F(" (0 is Omni On)"));

    // USB Client MIDI
    usbMIDI.setHandleControlChange(myControlChange);
    usbMIDI.setHandleNoteOff(myNoteOff);
    usbMIDI.setHandleNoteOn(myNoteOn);
    usbMIDI.setHandlePitchChange(myPitchBend);
    usbMIDI.setHandleProgramChange(myProgramChange);
    usbMIDI.setHandleClock(myMIDIClock);
    usbMIDI.setHandleContinue(myMIDIClockContinue);
    usbMIDI.setHandleStart(myMIDIClockStart);
    usbMIDI.setHandleStop(myMIDIClockStop);
    usbMIDI.setHandleSystemExclusive(fmSysexHandler); // DX7 voices and banks
    if (DEBUG)
        Serial.println(F("USB Client MIDI Listening"));

    // MIDI 5 Pin DIN
    MIDI.begin();
    MIDI.setHandleNoteOn(myNoteOn);
    MIDI.setHandleNoteOff(myNoteOff);
    MIDI.setHandlePitchBend(myPitchBend);
    MIDI.setHandleControlChange(myControlChange);
    MIDI.setHandleProgramChange(myProgramChange);
    MIDI.setHandleClock(myMIDIClock);
    MIDI.setHandleContinue(myMIDIClockContinue);
    MIDI.setHandleStart(myMIDIClockStart);
    MIDI.setHandleStop(myMIDIClockStop);
    if (DEBUG)
        Serial.println(F("MIDI In DIN Listening"));

    // USB HOST MIDI Class Compliant
    myusb.begin();
    usbHostMIDI.setHandleControlChange(usbHostControlChange);
    usbHostMIDI.setHandleNoteOff(myNoteOff);
    usbHostMIDI.setHandleNoteOn(myNoteOn);
    usbHostMIDI.setHandlePitchChange(myPitchBend);
    usbHostMIDI.setHandleProgramChange(myProgramChange);
    usbHostMIDI.setHandleClock(myMIDIClock);
    usbHostMIDI.setHandleContinue(myMIDIClockContinue);
    usbHostMIDI.setHandleStart(myMIDIClockStart);
    usbHostMIDI.setHandleStop(myMIDIClockStop);
    usbHostMIDI.setHandleSystemExclusive(fmSysexHandler); // DX7 voices and banks
    if (DEBUG)
        Serial.println(F("USB HOST MIDI Class Compliant Listening"));

    // Read MIDI Out Channel from EEPROM
    midiOutCh = getMIDIOutCh();
    // Read MIDI Thru mode from EEPROM
    MIDIThru = getMidiThru();
    changeMIDIThruMode();
    // Read oscilloscope enable from EEPROM
    enableScope(getScopeEnable());
    // Read VU enable from EEPROM
    vuMeter = getVUEnable();
    // Read volume  from EEPROM
    currentVolume = getVolume();
    // Read global tuning from EEPROM
    tuningCents = getTuningCents();
    tuningCentsFrac = 1.0f + (tuningCents * CENTSFRAC);
    // Read arpeggiator settings from EEPROM
    arpDivision = getArpDivision();
    arpRange = getArpRange();
    arpStyle = getArpStyle();
    arpCycles = getArpCycles();
    if (arpCycles == ARP_HOLD)
        arpHold = true;
    arpBasis = getArpBasis();
    keyboardScale = getKeyboardScale();
    keyboardOct = getKeyboardBasis();
    sendCC = getSendCC();
    syncToMIDIClk = getSyncToMIDIClk();
    // Read note retrigger mode from EEPROM
    groupvec[activeGroupIndex]->setSameVoiceRetrig(getSameVoiceRetrig());
    // If the audio processing ever overruns, drop a voice rather than hang
    global.cpuGuard.onOverload = []()
    { groupvec[activeGroupIndex]->shedVoice(); };

    assignStrings();
    fmAssignStrings();
    assignParametersForPerformanceEncoders();
    setupDisplay();
    setEncodersState(state);
    setupHardware(encoderCallback, encoderButtonCallback, buttonCallback);
    // Firmware update file on the SD card?
    checkFirmwareFile();
}

// Specifically for Sequencer, bypassing recording note in and arp
// Note handlers run from RAM: flash cache misses add latency/jitter (also called by the sequencer/arp)
void noteOn(byte channel, byte note, byte velocity)
{
    AudioUpdateLock lock;
    groupvec[activeGroupIndex]->noteOn(note, velocity);
}

// Internal clock: a new arpeggio starts on the key press, instead of waiting up to a whole
// division for the timer's next step. On MIDI clock it stays on the clock's grid.
void arpStartNow()
{
    if (seqMidiSync || !currentSequence.running)
        return;
    if (!seqSwapper)
        noteOffRoutine(); // The previous arpeggio's last note is still on
    sequencer_timer.start();                                 // Restart the step period from now
    sequencer_timer.setPeriod(currentSequence.tempo_us / 2); // and apply any pending tempo change
    noteOnRoutine();
}

void myNoteOn(byte channel, byte note, byte velocity)
{
    if (midiDiscard)
        return; // Panic is flushing the MIDI inputs
    // Seed random() (Random arp style) from the CPU cycle count at the first key press, so
    // it doesn't play the same sequence after every power-up
    static bool randomSeeded = false;
    if (!randomSeeded)
    {
        randomSeed(ARM_DWT_CYCCNT | 1);
        randomSeeded = true;
    }
    if (arpRunning)
    {
        // If it's in hold mode and you are not holding any notes down,
        // it continues to play the previous arpeggio. Once you press
        // a new note, it resets the arpeggio and starts a new one.
        // Also resets arpeggio from starting note.
        bool newArpeggio = arpNotesHeld == 0;
        if (newArpeggio)
            resetNotes();
        if (isArpKeyHeld(note))
            return; // Duplicate note-on (e.g. two merged MIDI sources)
        if (arpNotesHeld >= sizeof(arpNotes) - 1)
            return; // Arpeggio full
        arpNotesHeld++;
        setArpKeyHeld(note, true);
        addArpNote(note, velocity);
        if (newArpeggio)
            arpStartNow();
        return;
    }
    else
    {
        if (currentSequence.recording && currentSequence.running)
        {
            currentSequence.note_in = note;
            currentSequence.note_in_velocity = velocity;
        }
        else if (currentSequence.recording && !currentSequence.running && state == SEQUENCEEDIT)
        {
            currentSequence.Notes[currentSeqPosition] = note;
            currentSequence.Velocities[currentSeqPosition] = velocity;
            if (currentSeqPosition < currentSequence.length - 1)
                currentSeqPosition++;
            else
                currentSeqPosition = 0;

            currentSeqNote = note;
            if (currentSeqNote < 108)
                seqCurrentOctPos = nearbyint((currentSeqNote + 4) / 12) - 2;
        }
        noteOn(midiChannel, note, velocity);
    }
}

void noteOff(byte channel, byte note, byte velocity)
{
    AudioUpdateLock lock;
    groupvec[activeGroupIndex]->noteOff(note);
}

void myNoteOff(byte channel, byte note, byte velocity)
{
    if (midiDiscard)
        return; // Panic is flushing the MIDI inputs
    // Only keys the arp counted: not ones held before it started or rejected when it was full
    if (arpRunning && isArpKeyHeld(note))
    {
        setArpKeyHeld(note, false);
        if (arpNotesHeld > 0)
            arpNotesHeld--;
        // Remove exactly the released note (the old shift-based removal assumed a sorted
        // arpeggio, so it removed the wrong note in Play Order)
        if (!arpHold)
            removeArpNote(note);
        // The arp plays its own notes: this key never sounded, and a note-off here cut
        // off an arp note of the same pitch
        return;
    }
    noteOff(channel, note, velocity); // Includes keys held before the arp started
}

FLASHMEM void updatePatchnameAndIndex(String name, uint32_t index, uint32_t UID)
{
    groupvec[activeGroupIndex]->setPatchName(name);
    groupvec[activeGroupIndex]->setPatchIndex(index);
    groupvec[activeGroupIndex]->setUID(UID);
    setPatchNoAndNameForDisp(String(index), name);
}

void myPitchBend(byte channel, int bend)
{
    if (midiDiscard)
        return; // Panic is flushing the MIDI inputs
    groupvec[activeGroupIndex]->pitchBend(bend); // Range is set in Dexed (setPitchbendRange)
}

// MIDI CC
FLASHMEM void myControlChange(byte channel, byte control, byte value)
{
    if (midiDiscard)
        return; // Panic is flushing the MIDI inputs
    LockGuard lock(displayLock); // The display renders from the state changed here
    if (DEBUG)
        Serial.printf("Ch:%u: cc=%d, v=%d\n", channel, control, value);
    if (sendCC)
        midiCCOut(control, value); // Send patch parameters

    switch (control)
    {
    case CCvolume:
        currentVolume = value;
        storeVolumeToEEPROM(value);
        // Audio taper: level = knob^2, so half way is -12dB rather than -6dB (most of the
        // change used to be in the bottom fifth of the travel). Maximum is unchanged.
        groupvec[activeGroupIndex]->setVolume(LINEAR[currentVolume] * LINEAR[currentVolume] * MAX_VOL);
        showCurrentParameterOverlay(ParameterStrMap[CCvolume], LINEAR[currentVolume]);
        break;
    case CCbankselectLSB:
        if (value < BANKS_LIMIT)
        {
            currentBankIndex = value;
            if (!cardStatus)
                return;
            loadPatchNamesFromBank(currentBankIndex);
            if (patches.size() > 0 && patches[0].patchUID > 0)
            {
                currentPatchIndex = 0;
                recallPatch(currentBankIndex, patches[0].patchUID);
            }
        }
        break;

    case CCmodwheel:
        groupvec[activeGroupIndex]->setModWheel(value);
        break;
    case CCbreath:
        groupvec[activeGroupIndex]->setBreathController(value);
        break;
    case CCfoot:
        groupvec[activeGroupIndex]->setFootController(value);
        break;
    case CCsustain:
        groupvec[activeGroupIndex]->setSustain(value >= 64);
        break;
    case CCglide:
    {
        // Portamento time 0-99, 0 = off
        currentPatch.PortamentoTime = value * 99 / 127;
        groupvec[activeGroupIndex]->setPortamento(0, 0, currentPatch.PortamentoTime);
        String str = currentPatch.PortamentoTime == 0 ? String(F("Off")) : String(currentPatch.PortamentoTime);
        if (!setEncValue(CCglide, value, str))
            showCurrentParameterOverlay(ParameterStrMap[CCglide], str);
        break;
    }
    case CCfilterfreq:
    {
        currentPatch.FilterFreq = value;
        groupvec[activeGroupIndex]->setFilterCutoff(value / 127.0f);
        String str = String(int(value * 100 / 127.0f + 0.5f)) + F("%");
        if (!setEncValue(CCfilterfreq, value, str))
            showCurrentParameterOverlay(ParameterStrMap[CCfilterfreq], str);
        break;
    }
    case CCfilterres:
    {
        currentPatch.FilterRes = value;
        groupvec[activeGroupIndex]->setFilterResonance(value / 127.0f);
        String str = String(int(value * 100 / 127.0f + 0.5f)) + F("%");
        if (!setEncValue(CCfilterres, value, str))
            showCurrentParameterOverlay(ParameterStrMap[CCfilterres], str);
        break;
    }
    case CCallnotesoff:
        groupvec[activeGroupIndex]->allNotesOff();
        break;
    case CCensemblefxamt:
        currentPatch.EnsembleEffectAmt = value;
        groupvec[activeGroupIndex]->setEnsembleEffectAmount(ENSEMBLE_LFO[value]);
        if (!setEncValue(CCensemblefxamt, value, String(ENSEMBLE_LFO[value]) + F(" Hz")))
            showCurrentParameterOverlay(ParameterStrMap[CCensemblefxamt], String(ENSEMBLE_LFO[value]) + F(" Hz"));
        break;
    case CCensemblefxmix:
        currentPatch.EnsembleEffectMix = value;
        groupvec[activeGroupIndex]->setEnsembleEffectMix(LINEAR[value]);
        if (!setEncValue(CCensemblefxmix, value, String(LINEAR[value])))
            showCurrentParameterOverlay(ParameterStrMap[CCensemblefxmix], String(LINEAR[value]));
        break;
    case CCreverbfxtime:
        currentPatch.ReverbEffectTime = value;
        groupvec[activeGroupIndex]->setReverbEffectTime((LINEAR[value]) + 0.01f); // Doesn't like 0.0
        if (!setEncValue(CCreverbfxtime, value, String(LINEAR[value])))
            showCurrentParameterOverlay(ParameterStrMap[CCreverbfxtime], String(LINEAR[value]));
        break;
    case CCreverbfxmix:
        currentPatch.ReverbEffectMix = value;
        groupvec[activeGroupIndex]->setReverbEffectMix(LINEAR[value]);
        if (!setEncValue(CCreverbfxmix, value, String(LINEAR[value])))
            showCurrentParameterOverlay(ParameterStrMap[CCreverbfxmix], String(LINEAR[value]));
        break;
    case CCensemblefxdepth:
    {
        currentPatch.EnsembleEffectDepth = value;
        groupvec[activeGroupIndex]->setEnsembleEffectDepth(ensembleDepth(value)); // 64 = original
        String str = String(int(ensembleDepth(value) * 100 + 0.5f)) + F("%");
        if (!setEncValue(CCensemblefxdepth, value, str))
            showCurrentParameterOverlay(ParameterStrMap[CCensemblefxdepth], str);
        // The depth sets the shortest delay, so the delay shown can change too
        setEncValue(CCensemblefxdelay, currentPatch.EnsembleEffectDelay, ensembleDelayStr());
        break;
    }
    case CCensemblefxdelay:
    {
        currentPatch.EnsembleEffectDelay = value;
        groupvec[activeGroupIndex]->setEnsembleEffectDelay(ensembleDelaySamples(value)); // 64 = original
        String str = ensembleDelayStr();
        if (!setEncValue(CCensemblefxdelay, value, str))
            showCurrentParameterOverlay(ParameterStrMap[CCensemblefxdelay], str);
        break;
    }
    case CCreverbfxdamping:
    {
        currentPatch.ReverbEffectDamping = value;
        groupvec[activeGroupIndex]->setReverbEffectDamping(value / 128.0f);
        String str = String(int(value * 100 / 128.0f + 0.5f)) + F("%");
        if (!setEncValue(CCreverbfxdamping, value, str))
            showCurrentParameterOverlay(ParameterStrMap[CCreverbfxdamping], str);
        break;
    }
    case CCreverbfxbasscut:
    {
        currentPatch.ReverbEffectBassCut = value;
        groupvec[activeGroupIndex]->setReverbEffectBassCut(value / 128.0f);
        String str = String(int(value * 100 / 128.0f + 0.5f)) + F("%");
        if (!setEncValue(CCreverbfxbasscut, value, str))
            showCurrentParameterOverlay(ParameterStrMap[CCreverbfxbasscut], str);
        break;
    }
    case CCmonomode:
    {
        // Dexed mono mode is on or off. MIDI CC 126 (mono operation) also turns it on.
        currentPatch.MonophonicMode = value ? 1 : 0;
        groupvec[activeGroupIndex]->setMonophonic(currentPatch.MonophonicMode);
        String str = currentPatch.MonophonicMode ? F("On") : F("Off");
        if (!setEncValue(CCmonomode, currentPatch.MonophonicMode, str))
            showCurrentParameterOverlay(ParameterStrMap[CCmonomode], str);
        break;
    }
    case pitchbendrange:
        currentPatch.PitchBend = value;
        groupvec[activeGroupIndex]->setPitchbendRange(value);
        if (!setEncValue(pitchbendrange, value, String(value)))
            showCurrentParameterOverlay(ParameterStrMap[pitchbendrange], String(value));
        break;
    case MIDIThruMode:
        MIDIThru = value;
        storeMidiThru(MIDIThru);
        changeMIDIThruMode();
        if (!setEncValue(MIDIThruMode, value, MIDIThruStr[MIDIThru]))
            showCurrentParameterOverlay(ParameterStrMap[MIDIThruMode], MIDIThruStr[MIDIThru]);
        break;
    case MIDIChOut:
    {
        midiOutCh = value;
        storeMidiOutCh(midiOutCh);
        String sOut = F("Off");
        if (midiOutCh > 0)
            sOut = String(midiOutCh);
        if (!setEncValue(MIDIChOut, value, sOut))
            showCurrentParameterOverlay(ParameterStrMap[MIDIChOut], sOut);
    }
    break;
    case MIDIChIn:
    {
        midiChannel = value;
        storeMidiChannel(midiChannel);
        String sIn = F("All");
        if (midiChannel > MIDI_CHANNEL_OMNI)
            sIn = String(midiChannel);
        if (!setEncValue(MIDIChIn, value, sIn))
            showCurrentParameterOverlay(ParameterStrMap[MIDIChIn], sIn);
    }
    break;
    }
}

FLASHMEM void myProgramChange(byte channel, byte program)
{
    if (midiDiscard)
        return; // Panic is flushing the MIDI inputs
    LockGuard lock(displayLock); // The display renders from the state changed here
    if (state == State::PERFORMANCEPAGE || state == State::PERFORMANCERECALL)
    {
        if (program < PERFORMANCES_LIMIT)
        {
            if (state == State::PERFORMANCERECALL)
                state = State::PERFORMANCEPAGE;
            currentPerformanceIndex = program;
            recallPerformance(currentPerformanceIndex + 1);
            setEncodersState(State::PERFORMANCEPAGE);
            if (DEBUG)
                Serial.print(F("MIDI Pgm Change Performance:"));
            if (DEBUG)
                Serial.println(performances[currentPerformanceIndex]);
        }
    }
    else
    {
        if (program < patches.size())
        {
            currentPatchIndex = program;
            recallPatch(currentBankIndex, patches[currentPatchIndex].patchUID);
            if (DEBUG)
                Serial.print(F("MIDI Pgm Change:"));
            if (DEBUG)
                Serial.println(patches[currentPatchIndex].patchUID);
        }
    }
}

// Arp/Sequencer
FLASHMEM void playNote(uint8_t note)
{
    noteOn(midiChannel, note, 90);
    delay(300);
    noteOff(midiChannel, note, 0);
}

FLASHMEM void setSeqTimerPeriod(float bpm)
{
    currentSequence.bpm = bpm;
    currentSequence.tempo_us = 15'000'000 / bpm; // beats per bar microseconds
    if (arpRunning)
        currentSequence.tempo_us = currentSequence.tempo_us * (ARP_DIVISION_24PPQ[arpDivision] / 6.0f);
    sequencer_timer.setNextPeriod(currentSequence.tempo_us / 2);

    lfoSyncFreq = 60.0f / currentSequence.bpm;
}

FLASHMEM void sequencerStart(boolean cont = false)
{
    setSeqTimerPeriod(currentSequence.bpm);
    if (!cont)
    {
        currentSequence.step = 0;
    }

    currentSequence.running = true;

    if (!seqMidiSync)
    {
        sequencer_timer.start();
    }
}

FLASHMEM void sequencerStop()
{
    sequencer_timer.stop();
    groupvec[activeGroupIndex]->allNotesOff();
    currentSequence.running = false;
    if (state == State::SEQUENCEEDIT || state == State::SEQUENCEPAGE || state == State::SEQUENCERECALL)
        singleLED(ledColour::GREEN, 3);
    // currentSequence.recording = false;
    currentSequence.note_in = 0;
}

// The arp carries on with its own timer when MIDI clock stops or is lost
FLASHMEM void arpToInternalClock()
{
    if (arpRunning && !seqMidiSync)
    {
        midiClkArpCount = 0;
        sequencerStart(); // Starts the internal timer, as seqMidiSync is false
    }
}

// MIDI Stop, or the clock was lost (see loop()). seqMidiSync is only ever set while
// "sync to MIDI clock" is on, and is cleared here so the internal clock works again.
FLASHMEM void midiClockStopped()
{
    bool wasSynced = seqMidiSync;
    if (seqMidiSync)
    {
        sequencerStop();
        groupvec[activeGroupIndex]->allNotesOff();
        resetNotes();
    }
    seqMidiSync = false;
    MIDIClkSignal = false;
    if (wasSynced)
        arpToInternalClock();
}

FLASHMEM void myMIDIClockContinue()
{
    if (midiDiscard)
        return; // Panic is flushing the MIDI inputs
    sinceMidiClock = 0;
    if (!syncToMIDIClk)
        return;
    if (!currentSequence.running)
    {
        seqMidiSync = true;
        sequencerStart(true);
    }
}

FLASHMEM void myMIDIClockStart()
{
    if (midiDiscard)
        return; // Panic is flushing the MIDI inputs
    midiClkCount = 0;
    midiBeatCount = 0;
    midiLastBeatMicros = micros();
    sinceMidiClock = 0;
    // Only follow the clock when "sync to MIDI clock" is on: the arp used to switch to it
    // regardless, and was then left with no timer at all after a MIDI Stop
    if (syncToMIDIClk)
    {
        if ((!currentSequence.running && !seqMidiSync) || arpRunning)
            seqMidiSync = true;
        if ((state == State::SEQUENCEPAGE || state == State::SEQUENCEEDIT) && seqMidiSync)
            sequencerStart();
        else if (arpRunning)
        {
            sequencer_timer.stop();
            midiClkArpCount = 0;
            seqSwapper = true;
        }
    }
    // Resync LFOs when MIDI Clock starts.
    // When there's a jump to a different
    // part of a track, such as in a DAW, the DAW must have same
    // rhythmic quantisation as Tempo Div.

    // TODO: Apply to all groupvec[activeGroupIndex]-> Maybe check channel?
    groupvec[activeGroupIndex]->midiClockStart();
    MIDIClkSignal = true;
}

FLASHMEM void myMIDIClockStop()
{
    if (midiDiscard)
        return; // Panic is flushing the MIDI inputs
    midiClockStopped();
}

void myMIDIClock()
{
    if (midiDiscard)
        return; // Panic is flushing the MIDI inputs
    sinceMidiClock = 0;
    MIDIClkSignal = true;
    if (seqMidiSync && !syncToMIDIClk)
        midiClockStopped(); // "Sync to MIDI clock" turned off while following the clock
    // 24ppq this is for 4/4 time, alternates between noteon and noteoff, 8 times
    if (seqMidiSync && syncToMIDIClk && currentSequence.running && (midiClkCount % 3) == 0)
    {
        sequencer();
        if (state == SEQUENCEPAGE || state == SEQUENCEEDIT || state == ARPPAGE1 || state == ARPPAGE2)
            setEncValue(SeqTempo, bpmToEncValue(lfoSyncFreq * 60), String(lfoSyncFreq * 60, 1));
    }

    if (seqMidiSync && arpRunning)
    {
        const uint8_t division = ARP_DIVISION_24PPQ[arpDivision];
        // The division can be shortened while running: the count used to run on to 255
        // (seconds of silence, with the last note left on)
        if (midiClkArpCount >= division)
            midiClkArpCount = 0;
        if (midiClkArpCount == 0)
        {
            if (!seqSwapper)
                noteOffRoutine(); // Note-off was skipped by a division change
            noteOnRoutine();
        }
        else if (midiClkArpCount == division - 2 && !seqSwapper)
        {
            noteOffRoutine();
        }

        if (++midiClkArpCount >= division)
            midiClkArpCount = 0;
    }

    // Every beat (24 clocks): the tempo is measured in microseconds (was milliseconds) and
    // synced LFOs are kept in phase with the beat count since Start (they used to only get
    // the rate, so they kept an arbitrary offset and drifted against the music)
    if (++midiClkCount > 23)
    {
        // TODO: Multitimbral Most of this needs to move into the VoiceGroup
        const uint32_t now = micros();
        const uint32_t beatMicros = now - midiLastBeatMicros;
        const bool haveTempo = midiLastBeatMicros != 0 && beatMicros > 0 && beatMicros < 4000000; // 15bpm and up
        midiLastBeatMicros = now;
        midiBeatCount++;
        if (haveTempo)
        {
            lfoSyncFreq = 1000000.0f / beatMicros;
            groupvec[activeGroupIndex]->midiClockBeat(midiBeatCount, beatMicros * 0.000001f);
        }
        midiClkCount = 0;
    }
}

FLASHMEM void silence()
{
    groupvec[activeGroupIndex]->allNotesOff();
    groupvec[activeGroupIndex]->closeEnvelopes();
}

// Debounced patch recall, see selectPatchForRecall()
#define PATCH_RECALL_DELAY_MS 150
boolean patchRecallPending = false;
boolean patchRecallUpdatesPerformance = false;
elapsedMillis patchRecallTimer;

FLASHMEM void recallPatch(uint8_t bank, long patchUID)
{
    LockGuard lock(displayLock); // The display renders from the state changed here
    patchRecallPending = false;  // A direct recall replaces a pending one
    if (patchUID == 0)
        return;
    silence();
    if (loadPatch(bank, patchUID))
    {
        setCurrentPatchData();
        previousPatchIndex = currentPatchIndex;
        previousBankIndex = currentBankIndex;
        storeLastPatchToEEPROM(currentPatchIndex);
        storeLastBankToEEPROM(currentBankIndex);
        // Reset Midi channels - if coming from performance mode
        midiChannel = getMIDIChannel();
        midiOutCh = getMIDIOutCh();
        MIDIThru = getMidiThru();
        changeMIDIThruMode();
    }
}

// Turning the patch or bank encoder only selects and shows the patch. It is recalled once the
// encoder has been still for PATCH_RECALL_DELAY_MS, rather than on every detent: each recall
// reads the SD card, sets every parameter and writes EEPROM, which a fast turn did dozens of
// times a second (freezes on TSynth).
FLASHMEM void selectPatchForRecall(boolean updatePerformance)
{
    setPatchNoAndNameForDisp(String(currentPatchIndex + 1), patches[currentPatchIndex].patchName);
    patchRecallPending = true;
    patchRecallUpdatesPerformance = updatePerformance;
    patchRecallTimer = 0;
}

FLASHMEM void checkPendingPatchRecall()
{
    if (!patchRecallPending || patchRecallTimer < PATCH_RECALL_DELAY_MS)
        return;
    LockGuard lock(displayLock);
    patchRecallPending = false;
    recallPatch(currentBankIndex, patches[currentPatchIndex].patchUID);
    if (patchRecallUpdatesPerformance)
    {
        currentPerformance.patches[0].bankIndex = currentBankIndex;
        currentPerformance.patches[0].UID = patches[currentPatchIndex].patchUID;
        currentPerformance.patches[0].patchName = currentPatchName;
    }
}

FLASHMEM void recallPerformance(uint8_t filename)
{
    LockGuard lock(displayLock); // The display renders from the state changed here
    silence();
    if (loadPerformance(filename))
    {
        //*** THIS IS HARDCODED TO THE FIRST PATCH AS MULTITIMBRALITY ISN'T FUNCTIONING ***
        if (loadPatch(currentPerformance.patches[0].bankIndex, currentPerformance.patches[0].UID))
        {
            midiChannel = currentPerformance.patches[0].midiCh;
            midiOutCh = currentPerformance.patches[0].midiChOut;
            MIDIThru = currentPerformance.patches[0].midiThru;
            changeMIDIThruMode();

            if (DEBUG)
                Serial.println(midiChannel);
            // TODO Set min and max note range but this is only useful for split and layer(multitimbral functionality)
            setCurrentPatchData();
            currentBankIndex = currentPerformance.patches[0].bankIndex;
            loadPatchNamesFromBank(currentBankIndex);
            currentPatchIndex = findPatchIndex(currentPerformance.patches[0].UID);
        }
        else
        {
            // Patch missing
            if (DEBUG)
                Serial.print(F("Patch Missing:"));
            if (DEBUG)
                Serial.println(currentPerformance.patches[0].UID);
            currentPerformance.patches[0].UID = 0;
        }
    }
    else
    {
        // Performance missing
        if (DEBUG)
            Serial.print(F("Performance Missing:"));
        if (DEBUG)
            Serial.println(filename);
    }
}

FLASHMEM void recallSequence(uint8_t filename)
{
    LockGuard lock(displayLock); // The display renders from the state changed here
    silence();
    if (!loadSequence(filename))
    {
        // Sequence missing
        if (DEBUG)
            Serial.print(F("Sequence Missing:"));
        if (DEBUG)
            Serial.println(filename);
        // Initialise sequence
        SequenceStruct ss;
        currentSequence = ss;
    }
    storeLastSequenceToEEPROM(currentSequenceIndex);
}

FLASHMEM void setCurrentPatchData()
{
    updatePatchnameAndIndex(currentPatch.PatchName, currentPatchIndex + 1, currentPatch.UID);
    updateDisplay = false; // Don't show each parameter on the display as they are all updated.
    VoiceGroup *vg = groupvec[activeGroupIndex];
    {
        AudioUpdateLock lock; // Dexed renders from the voice data
        if (currentPatch.VoiceValid)
            vg->setVoiceData(currentPatch.Voice);
        else
            vg->loadInitVoice();
        vg->setOpMask(currentPatch.OpEnabled);
    }
    vg->setModWheelController(currentPatch.ModWheelRange, currentPatch.ModWheelTarget);
    vg->setAftertouchController(currentPatch.AftertouchRange, currentPatch.AftertouchTarget);
    myControlChange(midiChannel, pitchbendrange, currentPatch.PitchBend);
    myControlChange(midiChannel, CCglide, (currentPatch.PortamentoTime * 127 + 98) / 99);
    myControlChange(midiChannel, CCfilterfreq, currentPatch.FilterFreq);
    myControlChange(midiChannel, CCfilterres, currentPatch.FilterRes);
    myControlChange(midiChannel, CCensemblefxamt, currentPatch.EnsembleEffectAmt);
    myControlChange(midiChannel, CCensemblefxmix, currentPatch.EnsembleEffectMix);
    myControlChange(midiChannel, CCreverbfxtime, currentPatch.ReverbEffectTime);
    myControlChange(midiChannel, CCreverbfxmix, currentPatch.ReverbEffectMix);
    myControlChange(midiChannel, CCensemblefxdepth, currentPatch.EnsembleEffectDepth);
    myControlChange(midiChannel, CCensemblefxdelay, currentPatch.EnsembleEffectDelay);
    myControlChange(midiChannel, CCreverbfxdamping, currentPatch.ReverbEffectDamping);
    myControlChange(midiChannel, CCreverbfxbasscut, currentPatch.ReverbEffectBassCut);
    vg->setMonophonic(currentPatch.MonophonicMode);
    updateDisplay = true;

    if (DEBUG)
        Serial.printf("Set Patch: %s UID: %u\n", currentPatch.PatchName, (unsigned)vg->getUID());
}

// Copies the engine's current FM voice into currentPatch, ready to save
FLASHMEM void storeVoiceInCurrentPatch()
{
    groupvec[activeGroupIndex]->getVoiceData(currentPatch.Voice);
    currentPatch.VoiceValid = true;
    currentPatch.OpEnabled = groupvec[activeGroupIndex]->getOpMask();
}

// Scales the encoder inc/decrement depending on the range to traverse
FLASHMEM int8_t encScaling(EncoderMappingStruct *enc, int8_t delta)
{
    int8_t countLimit = 0;
    switch (enc->Range)
    {
    case 64 ... 127:
        countLimit = 3;
        break;
    case 32 ... 63:
        countLimit = 4;
        break;
    case 16 ... 31:
        countLimit = 6;
        break;
    case 8 ... 15:
        countLimit = 9;
        // Less acceleration
        if (delta > 3)
            delta = 2;
        if (delta < -3)
            delta = -2;
        break;
    case 4 ... 7:
        countLimit = 12;
        // Less acceleration
        if (delta > 1)
            delta = 1;
        if (delta < -1)
            delta = -1;
        break;
    case 1 ... 3:
        countLimit = 15;
        // Less acceleration
        if (delta > 1)
            delta = 1;
        if (delta < -1)
            delta = -1;
        break;
    default: // over 127
        countLimit = 2;
        break;
    }

    enc->Counter += delta;
    if (enc->Counter < (countLimit * -1) || enc->Counter > countLimit)
    {
        enc->Counter = 0;
        if ((enc->Value + delta > -1) && (enc->Value + delta < enc->Range + 1))
        {
            enc->Value += delta;
        }
    }
    else
    {
        delta = 0;
    }

    return delta;
}

FLASHMEM void encoderCallback(unsigned enc_idx, int value, int delta)
{
    LockGuard lock(displayLock); // The display renders from the state changed here
    // --- Value isn't used because it's absolute values that don't change with changes to the encoder assignment---
    if (DEBUG)
        Serial.printf("enc[%u]: v=%d, d=%d\n", enc_idx, value, delta);
    //  Subtract 4 from encoder index due to numbering on shift registers
    enc_idx -= 4;
    if (encMap[enc_idx].active)
    {
        int8_t newDelta = encScaling(&encMap[enc_idx], delta);
        if (fmEncoderTurn(&encMap[enc_idx], newDelta))
            return; // FM parameter or operator/stage selector
        if (syxEncoderTurn(&encMap[enc_idx], newDelta))
            return; // .syx page
        switch (encMap[enc_idx].Parameter)
        {
        case kbdOct:
            if (newDelta == 0 || keyboardOct + newDelta < 0 || keyboardOct + newDelta > encMap[enc_idx].Range)
                break;
            groupvec[activeGroupIndex]->allNotesOff();
            keyboardOct += newDelta;
            storeKeyboardBasisToEEPROM(keyboardOct);
            setEncValue(kbdOct, encMap[enc_idx].Value, KEYBOARD_OCT_STR[keyboardOct]);
            break;
        case kbdScale:
            if (newDelta == 0 || keyboardScale + newDelta < 0 || keyboardScale + newDelta > encMap[enc_idx].Range)
                break;
            groupvec[activeGroupIndex]->allNotesOff();
            keyboardScale += newDelta;
            majMin = keyboardScale % 2;
            storeKeyboardScaleToEEPROM(keyboardScale);
            setEncValue(kbdScale, encMap[enc_idx].Value, SCALE_STR[keyboardScale]);
            break;
        case patchselect:
        case namepatch:
        case deletepatch:
            if (!cardStatus)
                return;
            if (newDelta == 0)
                break;
            newDelta > 0 ? incCurrentPatchIndex() : decCurrentPatchIndex();
            if (state == State::MAIN || state == State::DELETEPATCH)
            {
                selectPatchForRecall(false);
            }
            else if (state == State::PATCHSAVING)
            {
                currentPatchName = patches[currentPatchIndex].patchName;
            }
            else if (state == State::PERFORMANCEPATCHEDIT)
            {
                selectPatchForRecall(true);
            }
            break;
        case CCbankselectLSB:
            if (!cardStatus)
                return;
            if (newDelta == 0)
                break;
            loadPatchNamesFromBank(newDelta > 0 ? incTempBankIndex() : decTempBankIndex());
            if (state == State::PATCHLIST || state == State::EDITBANK || state == State::DELETEPATCH)
            {
                currentBankIndex = tempBankIndex;
                currentPatchIndex = 0;
            }
            else if (state == State::PERFORMANCEPATCHEDIT)
            {
                currentBankIndex = tempBankIndex;
                currentPatchIndex = patches.size() - 1;
                selectPatchForRecall(false);
            }
            else
            {
                while (patches[0].patchUID == 0)
                {
                    loadPatchNamesFromBank(newDelta > 0 ? incTempBankIndex() : decTempBankIndex());
                }
                currentBankIndex = tempBankIndex;
                currentPatchIndex = 0;
                selectPatchForRecall(false);
            }
            break;
        case savebankselect:
        case editbank:
        case deletebank:
            if (!cardStatus)
                return;
            if (newDelta == 0)
                break;
            loadPatchNamesFromBank(newDelta > 0 ? incTempBankIndex() : decTempBankIndex());
            currentBankIndex = tempBankIndex;
            if (patches[0].patchUID != 0)
                patches.push_back(PatchUIDAndName{0, EMPTYNAME});
            currentPatchIndex = patches.size() - 1;
            break;
        case bankeditselect:
        case renamebank:
            if (!cardStatus)
                return;
            if (newDelta == 0)
                break;
            loadPatchNamesFromBank(newDelta > 0 ? incTempBankIndex() : decTempBankIndex());
            break;
        case choosecharacterPatch:
        case choosecharacterBank:
        case choosecharacterPerformance:
        case choosecharacterSequence:
            charCursor += newDelta;
            if (charCursor >= TOTALCHARS)
                charCursor = 0;
            if (charCursor <= -1)
                charCursor = TOTALCHARS - 1;
            break;
        case deleteCharacterBank:
            if (newDelta > 0 && ++nameCursor > bankNames[currentBankIndex].length() - 1)
            {
                nameCursor = bankNames[currentBankIndex].length() - 1;
            }
            else if (newDelta < 0 && --nameCursor < 0)
            {
                nameCursor = 0;
            }
            break;
        case deleteCharacterPatch:
            if (newDelta > 0 && ++nameCursor > currentPatchName.length() - 1)
            {
                nameCursor = currentPatchName.length() - 1;
            }
            else if (newDelta < 0 && --nameCursor < 0)
            {
                nameCursor = 0;
            }
            break;
        case deleteCharacterPerformance:
            if (newDelta > 0 && ++nameCursor > currentPerformance.performanceName.length() - 1)
            {
                nameCursor = currentPerformance.performanceName.length() - 1;
            }
            else if (newDelta < 0 && --nameCursor < 0)
            {
                nameCursor = 0;
            }
            break;
        case deleteCharacterSequence:
            if (newDelta > 0 && ++nameCursor > currentSequence.SequenceName.length() - 1)
            {
                nameCursor = currentSequence.SequenceName.length() - 1;
            }
            else if (newDelta < 0 && --nameCursor < 0)
            {
                nameCursor = 0;
            }
            break;
        case PerfSelect:
        case PerfEdit:
        case PerfDelete:
            if (!cardStatus)
                return;
            if (newDelta == 0)
                break;
            newDelta > 0 ? incPerformanceIndex() : decPerformanceIndex();
            break;
        case SeqSelect:
        case SeqEdit:
        case SeqDelete:
            if (!cardStatus)
                return;
            if (newDelta == 0)
                break;
            newDelta > 0 ? incSequenceIndex() : decSequenceIndex();
            break;
        case chooseEncoderTL:
        case chooseEncoderTR:
        case chooseEncoderBL:
        case chooseEncoderBR:
            if (newDelta == 0 || encMap[enc_idx].Value < 0 || encMap[enc_idx].Value > encMap[enc_idx].Range)
                break;
            setEncValue(encMap[enc_idx].Parameter, encMap[enc_idx].Value, ParameterStrMap[ParametersForPerformanceEncoders[encMap[enc_idx].Value]]);
            currentPerformance.TL = ParametersForPerformanceEncoders[encMap[ENC_TL].Value];
            currentPerformance.TR = ParametersForPerformanceEncoders[encMap[ENC_TR].Value];
            currentPerformance.BL = ParametersForPerformanceEncoders[encMap[ENC_BL].Value];
            currentPerformance.BR = ParametersForPerformanceEncoders[encMap[ENC_BR].Value];
            break;
        case chooseMIDIChIn:
        case choosePerfMIDIChIn:
        {
            String sIn = F("All");
            if (encMap[enc_idx].Value > MIDI_CHANNEL_OMNI)
                sIn = String(encMap[enc_idx].Value);
            if (state == State::PERFORMANCEMIDIEDIT)
            {
                currentPerformance.patches[0].midiCh = encMap[enc_idx].Value;
                setEncValue(choosePerfMIDIChIn, encMap[enc_idx].Value, sIn);
            }
            else if (!setEncValue(chooseMIDIChIn, encMap[enc_idx].Value, sIn))
            {
                showCurrentParameterOverlay(ParameterStrMap[chooseMIDIChIn], sIn);
            }
        }
        break;
        case chooseMIDIChOut:
        case choosePerfMIDIChOut:
        {
            String sOut = F("Off");
            if (encMap[enc_idx].Value > 0)
                sOut = String(encMap[enc_idx].Value);
            if (state == State::PERFORMANCEMIDIEDIT)
            {
                currentPerformance.patches[0].midiChOut = encMap[enc_idx].Value;
                setEncValue(choosePerfMIDIChOut, encMap[enc_idx].Value, sOut);
            }
            else if (!setEncValue(chooseMIDIChOut, encMap[enc_idx].Value, sOut))
            {
                showCurrentParameterOverlay(ParameterStrMap[chooseMIDIChOut], sOut);
            }
        }
        break;
        case chooseMIDIThruMode:
        case choosePerfMIDIThruMode:
        {
            if (state == State::PERFORMANCEMIDIEDIT)
            {
                currentPerformance.patches[0].midiThru = encMap[enc_idx].Value;
                setEncValue(choosePerfMIDIThruMode, encMap[enc_idx].Value, MIDIThruStr[encMap[enc_idx].Value]);
            }
            else if (!setEncValue(chooseMIDIThruMode, encMap[enc_idx].Value, MIDIThruStr[encMap[enc_idx].Value]))
            {
                showCurrentParameterOverlay(ParameterStrMap[chooseMIDIThruMode], MIDIThruStr[encMap[enc_idx].Value]);
            }
        }
        break;
        case settingoption:
            if (newDelta == 0)
                break;
            newDelta > 0 ? settings::increment_setting() : settings::decrement_setting();
            setEncValue(settingoption, value, settings::current_setting());
            setEncValue(settingvalue, (int)settings::index(), settings::current_setting_size() - 1, settings::current_setting_value());
            settings::save_current_value();
            break;
        case settingvalue:
            if (newDelta == 0)
                break;
            newDelta > 0 ? settings::increment_setting_value() : settings::decrement_setting_value();
            setEncValue(settingvalue, value, settings::current_setting_value());
            settings::save_current_value();
            break;
        case SeqTempo:
            if ((seqMidiSync && !syncToMIDIClk) || newDelta == 0 || currentSequence.bpm + (newDelta / 10.0f) < 20.0f || currentSequence.bpm + (newDelta / 10.0f) > 300.0)
                break;
            currentSequence.bpm += (newDelta / 10.0f);
            setSeqTimerPeriod(currentSequence.bpm);
            setEncValue(SeqTempo, bpmToEncValue(currentSequence.bpm), String(currentSequence.bpm, 1));
            break;
        case SeqLength:
            if (newDelta == 0 || currentSequence.length + newDelta < 1 || currentSequence.length + newDelta > 64)
                break;
            currentSequence.length += newDelta;
            if (currentSeqPosition > currentSequence.length - 1)
            {
                currentSeqPosition = currentSequence.length - 1;
            }
            if (currentSequence.running && currentSequence.step > currentSequence.length - 1)
            {
                sequencerStart();
            }
            break;
        case SeqPosition:
            if (newDelta == 0 || currentSeqPosition + newDelta < 0 || currentSeqPosition + newDelta > currentSequence.length - 1)
                break;
            currentSeqPosition += newDelta;
            break;
        case SeqNote:
            if (newDelta == 0 || currentSeqNote + newDelta < 12 || currentSeqNote + newDelta > 119)
                break;
            currentSeqNote += newDelta;
            if (currentSeqNote < 108)
                seqCurrentOctPos = nearbyint((currentSeqNote + 4) / 12) - 2;
            break;
        case ArpCycle:
            if (newDelta == 0 || arpCycles + newDelta < ARP_INF || arpCycles + newDelta > ARP_HOLD)
                break;
            arpCycles += newDelta;
            if (arpCycles != ARP_HOLD)
            {
                resetNotes();
                arpHold = false;
            }
            else
                arpHold = true;
            storeArpCyclesToEEPROM(arpCycles);
            setEncValue(ArpCycle, arpCycles, ARP_CYCLES[arpCycles]);
            break;
        case ArpStyle:
            if (newDelta == 0 || arpStyle + newDelta < 0 || arpStyle + newDelta > 5)
                break;
            arpStyle += newDelta;
            resetArp();
            storeArpStyleToEEPROM(arpStyle);
            setEncValue(ArpStyle, arpStyle, ARP_STYLES[arpStyle]);
            break;
        case ArpDivision:
            if (newDelta == 0 || arpDivision + newDelta < 0 || arpDivision + newDelta > 9)
                break;
            arpDivision += newDelta;
            setSeqTimerPeriod(currentSequence.bpm);
            storeArpDivisionToEEPROM(arpDivision);
            setEncValue(ArpDivision, arpDivision, ARP_DIVISION_STR[arpDivision]);
            break;
        case ArpRange:
            if (newDelta == 0 || arpRange + newDelta < 0 || arpRange + newDelta > 6)
                break;
            arpRange += newDelta;
            storeArpRangeToEEPROM(arpRange);
            setEncValue(ArpRange, arpRange, ARP_RANGE_STR[arpRange]);
            break;
        case ArpBasis:
            if (newDelta == 0 || arpBasis + newDelta < 0 || arpBasis + newDelta > 4)
                break;
            arpBasis += newDelta;
            storeArpBasisToEEPROM(arpBasis);
            setEncValue(ArpBasis, arpBasis, ARP_BASIS_STR[arpBasis]);
            break;
        default:
            // if(DEBUG) Serial.printf("enc[%u]: v=%d, d=%d\n", enc_idx, encMap[enc_idx].Value, newDelta);
            myControlChange(midiChannel, encMap[enc_idx].Parameter, encMap[enc_idx].Value);
            midiCCOut(encMap[enc_idx].Parameter, encMap[enc_idx].Value);
            break;
        }
    }
}

FLASHMEM void encoderButtonCallback(unsigned enc_idx, int buttonState)
{
    LockGuard lock(displayLock); // The display renders from the state changed here
    // Subtract 4 from encoder index due to numbering on shift registers
    enc_idx -= 4;

    // Ignore encoder button held repeatedly
    if (buttonState == HELD_REPEAT)
        return;

    if (encMap[enc_idx].Push && encMap[enc_idx].active)
    {
        if (fmEncoderPush(&encMap[enc_idx]) || // FM parameter: back to the init voice value
            syxEncoderPush(&encMap[enc_idx]))  // .syx page: import, export, open
        {
            state = encMap[enc_idx].PushAction;
            setEncodersState(state);
            return;
        }
        switch (encMap[enc_idx].Parameter)
        {
        case kbdOct:
        case kbdScale:
            keyboardActive = !keyboardActive;
            break;
        case firmwareupdate:
            // Run from loop(): the update can't hold the display lock, or no progress is shown
            firmwareUpdateRequested = true;
            break;
        case firmwarecancel:
            renameFirmwareFile(fwFileName, FW_FILE_CANCELLED_EXT); // Not offered again until it's renamed back
            break;
        case patchselect:
        case CCbankselectLSB:
            if (cardStatus)
            {
                // If current patch is empty in this bank, go to previous bank with valid patch
                while (patches[0].patchUID == 0)
                {
                    tempBankIndex = decTempBankIndex();
                    loadPatchNamesFromBank(tempBankIndex);
                    currentBankIndex = tempBankIndex;
                    currentPatchIndex = 0;
                }
                recallPatch(currentBankIndex, patches[currentPatchIndex].patchUID);
                if (state == State::PERFORMANCEPATCHEDIT)
                {
                    currentPerformance.patches[0].bankIndex = currentBankIndex;
                    currentPerformance.patches[0].UID = patches[currentPatchIndex].patchUID;
                    savePerformance();
                    state = State::PERFORMANCEENCEDIT;
                    singleLED(ledColour::GREEN, 1);
                    setEncodersState(state);
                    return;
                }
            }
            else
            {
                return;
            }
            ledsOff();
            break;
        case namepatch:
            if (patches[currentPatchIndex].patchUID == 0)
                currentPatchName = "";
            nameCursor = currentPatchName.length() - 1;
            break;
        case editbank:
            nameCursor = bankNames[tempBankIndex].length() - 1;
            break;
        case cancel:
            currentPatchIndex = previousPatchIndex;
            currentBankIndex = previousBankIndex;
            tempBankIndex = currentBankIndex;
            loadPatchNamesFromBank(tempBankIndex);
            if (state != State::PATCHSAVING &&
                state != State::PATCHLIST &&
                state != State::PERFORMANCERECALL &&
                state != State::SEQUENCERECALL)
            {
                loadBankNames(); // If in the middle of bank renaming
                recallPatch(currentBankIndex, patches[currentPatchIndex].patchUID);
            }
            ledsOff();
            break;
        case goback:
            if (state != SEQUENCEPAGE)
                ledsOff();
            break;
        case choosecharacterPatch:
            if (currentPatchName.length() < PATCHNAMEMAXLEN)
            {
                if (currentPatchName.length() > 0)
                {
                    currentPatchName = currentPatchName.substring(0, nameCursor + 1) +
                                       CHARACTERS[charCursor] + currentPatchName.substring(nameCursor + 1, currentPatchName.length());
                }
                else
                {
                    currentPatchName = CHARACTERS[charCursor];
                }
                nameCursor++;
            }
            break;
        case choosecharacterBank:
            if (bankNames[tempBankIndex].length() < BANKNAMEMAXLEN)
            {
                if (bankNames[tempBankIndex].length() > 0)
                {
                    bankNames[tempBankIndex] = bankNames[tempBankIndex].substring(0, nameCursor + 1) +
                                               CHARACTERS[charCursor] + bankNames[tempBankIndex].substring(nameCursor + 1, bankNames[tempBankIndex].length());
                }
                else
                {
                    bankNames[tempBankIndex] = CHARACTERS[charCursor];
                }
                nameCursor++;
            }
            break;
        case choosecharacterPerformance:
            if (currentPerformance.performanceName.length() < PERFORMANCENAMEMAXLEN)
            {
                if (bankNames[tempBankIndex].length() > 0)
                {
                    currentPerformance.performanceName = currentPerformance.performanceName.substring(0, nameCursor + 1) +
                                                         CHARACTERS[charCursor] + currentPerformance.performanceName.substring(nameCursor + 1, currentPerformance.performanceName.length());
                }
                else
                {
                    currentPerformance.performanceName = CHARACTERS[charCursor];
                }
                nameCursor++;
            }
            break;
        case choosecharacterSequence:
            if (currentSequence.SequenceName.length() < SEQUENCENAMEMAXLEN)
            {
                if (currentSequence.SequenceName.length() > 0)
                {
                    currentSequence.SequenceName = currentSequence.SequenceName.substring(0, nameCursor + 1) +
                                                   CHARACTERS[charCursor] + currentSequence.SequenceName.substring(nameCursor + 1, currentSequence.SequenceName.length());
                }
                else
                {
                    currentSequence.SequenceName = CHARACTERS[charCursor];
                }
                nameCursor++;
            }
            break;
        case deleteCharacterPatch:
            if (buttonState == HELD)
            {
                currentPatchName = "";
                nameCursor = 0;
                break;
            }
            if (currentPatchName.length() == 0)
                break;
            currentPatchName = currentPatchName.substring(0, nameCursor) + currentPatchName.substring(nameCursor + 1, currentPatchName.length());
            nameCursor--;
            break;
        case deleteCharacterBank:
            if (buttonState == HELD)
            {
                bankNames[tempBankIndex] = "";
                nameCursor = 0;
                break;
            }
            if (bankNames[tempBankIndex].length() == 0)
                break;
            nameCursor--;
            bankNames[tempBankIndex] = String(bankNames[tempBankIndex]).substring(0, bankNames[tempBankIndex].length() - 1);
            break;
        case deleteCharacterSequence:
            if (buttonState == HELD)
            {
                currentSequence.SequenceName = "";
                nameCursor = 0;
                break;
            }
            if (currentSequence.SequenceName.length() == 0)
                break;
            currentSequence.SequenceName = currentSequence.SequenceName.substring(0, nameCursor) +
                                           currentSequence.SequenceName.substring(nameCursor + 1, currentSequence.SequenceName.length());
            nameCursor--;
            break;
        case deleteCharacterPerformance:
            if (buttonState == HELD)
            {
                currentPerformance.performanceName = "";
                nameCursor = 0;
                break;
            }
            if (currentPerformance.performanceName.length() == 0)
                break;
            currentPerformance.performanceName = currentPerformance.performanceName.substring(0, nameCursor) +
                                                 currentPerformance.performanceName.substring(nameCursor + 1, currentPerformance.performanceName.length());
            nameCursor--;
            break;
        case savepatch:
            state = State::SAVE;
            strlcpy(currentPatch.PatchName, currentPatchName.c_str(), sizeof(currentPatch.PatchName)); // Always terminated
            savePatch(currentBankIndex, currentPatchIndex);
            updatePatchnameAndIndex(currentPatch.PatchName, currentPatchIndex + 1, currentPatch.UID);
            recallPatch(currentBankIndex, patches[currentPatchIndex].patchUID);
            ledsOff();
            break;
        case deletepatch:
            if (patches[0].patchUID != 0)
            {
                state = State::DELETEMSG;
                if (findPatchIndex(patches[currentPatchIndex].patchUID, currentPatchIndex) == -1)
                    deletePatch(currentBankIndex, currentPatchIndex);
                // Remove from array if patch is in current bank
                patches.remove(currentPatchIndex);
                recreatePatchIndexFile(currentBankIndex);
            }
            currentPatchIndex = 0;
            // First Patch is empty in this bank, go to previous bank with valid patch
            while (patches[0].patchUID == 0)
            {
                tempBankIndex = decTempBankIndex();
                loadPatchNamesFromBank(tempBankIndex);
                currentBankIndex = tempBankIndex;
                currentPatchIndex = 0;
            }
            recallPatch(currentBankIndex, patches[currentPatchIndex].patchUID);
            ledsOff();
            break;
        case savebank:
            state = State::MAIN;
            saveBankName(bankNames[tempBankIndex]);
            while (patches[0].patchUID == 0)
            {
                tempBankIndex = decTempBankIndex();
                loadPatchNamesFromBank(tempBankIndex);
                currentBankIndex = tempBankIndex;
                currentPatchIndex = 0;
            }
            recallPatch(currentBankIndex, patches[currentPatchIndex].patchUID);
            ledsOff();
            break;
        case deletebank:
            state = State::DELETEBANKMSG;
            deleteBank(tempBankIndex);
            currentPatchIndex = 0;
            // First Patch is empty in this bank, go to previous bank with valid patch
            while (patches[0].patchUID == 0)
            {
                tempBankIndex = decTempBankIndex();
                loadPatchNamesFromBank(tempBankIndex);
                currentBankIndex = tempBankIndex;
                currentPatchIndex = 0;
            }
            recallPatch(currentBankIndex, patches[currentPatchIndex].patchUID);
            ledsOff();
            break;
        case PerfSelect:
        case PerfEdit:
            recallPerformance(currentPerformanceIndex + 1);
            singleLED(ledColour::GREEN, 1); // Led 1 stays green
            break;
        case PerfDelete:
            deletePerformance(currentPerformanceIndex + 1);
            break;
        case SeqSelect:
        case SeqEdit:
            recallSequence(currentSequenceIndex + 1);
            singleLED(ledColour::GREEN, 3); // Led 3 stays green
            break;
        case SeqDelete:
            deleteSequence(currentSequenceIndex + 1);
            break;
        case SeqStartStop:
            if (currentSequence.running)
            {
                sequencerStop();
            }
            else
            {
                sequencerStart();
            }
            singleLED(ledColour::GREEN, 3); // Led 3 stays green
            break;
        case SeqTempo:
            currentSequence.bpm = 120.0f;
            setSeqTimerPeriod(currentSequence.bpm);
            break;
        case SeqLength:
            if (buttonState == HELD)
            {
                // Toggle record sequence
                currentSequence.recording = !currentSequence.recording;
                break;
            }
            switch (currentSequence.length)
            {
            case 64:
                currentSequence.length = 8;
                break;
            case 48:
                currentSequence.length = SEQ_PATTERN_LEN;
                break;
            case 32:
                currentSequence.length = 48;
                break;
            case 16:
                currentSequence.length = 32;
                break;
            case 8:
                currentSequence.length = 16;
                break;
            default:
                currentSequence.length = SEQ_PATTERN_LEN;
                break;
            }
            currentSeqPosition = 0;
            if (currentSequence.running && currentSequence.step > currentSequence.length - 1)
            {
                sequencerStart();
            }
            break;
        case SeqPosition:
            if (buttonState == HELD)
            {
                currentSeqPosition = 0;
                break;
            }
            if (currentSeqPosition < currentSequence.length - 1)
                currentSeqPosition++;
            break;
        case SeqNote:
            if (buttonState == HELD)
            {
                // Delete all notes
                uint8_t n = 0;
                while (n < currentSequence.length)
                {
                    currentSequence.Notes[n] = 0;
                    n++;
                }
                saveSequence();
                break;
            }
            if (currentSequence.Notes[currentSeqPosition] == 0 || currentSequence.Notes[currentSeqPosition] != currentSeqNote)
            {
                currentSequence.Notes[currentSeqPosition] = currentSeqNote;
                currentSequence.Velocities[currentSeqPosition] = 64; // Default velocity
                playNote(currentSeqNote);
            }
            else
            {
                currentSequence.Notes[currentSeqPosition] = 0;
                currentSequence.Velocities[currentSeqPosition] = 0;
            }
            saveSequence();
            break;
        case ArpCycle:
            arpHold = !arpHold;
            if (!arpHold)
            {
                resetNotes();
                arpCycles = ARP_INF;
            }
            else
                arpCycles = ARP_HOLD;
            storeArpCyclesToEEPROM(arpCycles);
            break;

        case ArpStyle:
            arpStyle = encMap[enc_idx].DefaultValue;
            resetArp();
            storeArpStyleToEEPROM(arpStyle);
            setEncValue(ArpStyle, arpStyle, ARP_STYLES[arpStyle]);
            break;
        case ArpDivision:
            arpDivision = encMap[enc_idx].DefaultValue;
            setSeqTimerPeriod(currentSequence.bpm);
            storeArpDivisionToEEPROM(arpDivision);
            setEncValue(ArpDivision, arpDivision, ARP_DIVISION_STR[arpDivision]);
            break;
        case ArpRange:
            arpRange = encMap[enc_idx].DefaultValue;
            storeArpRangeToEEPROM(arpRange);
            setEncValue(ArpRange, arpRange, ARP_RANGE_STR[arpRange]);
            break;
        case ArpBasis:
            arpBasis = encMap[enc_idx].DefaultValue;
            storeArpBasisToEEPROM(arpBasis);
            setEncValue(ArpBasis, arpBasis, ARP_BASIS_STR[arpBasis]);
            break;
        default:
            setDefaultValue(&encMap[enc_idx]);
            myControlChange(midiChannel, encMap[enc_idx].Parameter, encMap[enc_idx].Value);
            midiCCOut(encMap[enc_idx].Parameter, encMap[enc_idx].Value);
            break;
        }
        state = encMap[enc_idx].PushAction;
        setEncodersState(state);
    }
}

// Reinitialise to panel: state shown for this long, then restored (see buttonCallback)
#define REINIT_STATE_MS 250
State reinitRestoreState = State::MAIN;
bool reinitRestorePending = false;
elapsedMillis reinitRestoreTimer;

// Read and drop everything waiting in the MIDI inputs, so a backlog from a flood of MIDI
// doesn't keep playing after Panic. Bounded in time in case the sender keeps going.
#define MIDI_FLUSH_MAX_MS 50
FLASHMEM void flushMidiInput()
{
    midiDiscard = true;
    elapsedMillis t;
    bool more = true;
    while (more && t < MIDI_FLUSH_MAX_MS)
    {
        more = false;
        myusb.Task();
        if (usbHostMIDI.read()) // USB host
            more = true;
        if (usbMIDI.read()) // USB device
            more = true;
    }
    // 5 pin DIN: drop the unread bytes without parsing them, so they aren't sent to MIDI thru
    Serial1.clear();
    midiDiscard = false;
}

// Panic: drop any MIDI waiting to be read, then silence all voices at once
FLASHMEM void midiPanic()
{
    flushMidiInput();
    AudioNoInterrupts();
    groupvec[activeGroupIndex]->panic();
    AudioInterrupts();
    showCurrentParameterOverlay(F("Panic"), F("All notes off"));
}

FLASHMEM void buttonCallback(unsigned button_idx, int buttonStatus)
{
    LockGuard lock(displayLock); // The display renders from the state changed here
    /*
      Button1 SR1 D4  10
      Button2 SR2 D4  9
      Button3 SR3 D4  11
      Button4 SR1 D5  7
      Button5 SR2 D5  6
      Button6 SR3 D5  8
      Button7 SR1 D6  4
      Button8 SR2 D6  3
      ButtonUp SR1 D7 1
      ButtonDown SR2 D7 0
    */
    switch (button_idx)
    {
    case 22: // VOL_UP & VOL_DOWN Held
        keyboardActive = !keyboardActive;
        break;
    case 33: // Arp & Seq Held
        if (currentSequence.running)
        {
            sequencerStop();
            arpRunning = false;
            if (state == State::ARPPAGE1 || state == State::ARPPAGE2)
            {
                state = State::MAIN;
                ledsOff();
            }
            if (state == State::SEQUENCEEDIT || state == State::SEQUENCEPAGE || state == State::SEQUENCERECALL)
                singleLED(ledColour::GREEN, 3);
        }
        else if (!currentSequence.running && (state == State::SEQUENCEEDIT || state == State::SEQUENCEPAGE))
        {
            sequencerStart();
        }
        break;
    case VOL_UP:
        if (buttonStatus == HIGH || buttonStatus == HELD || buttonStatus == HELD_REPEAT)
        {
            myControlChange(midiChannel, CCvolume, clampToRange<int>(currentVolume, 1, 0, 127));
        }
        break;
    case VOL_DOWN:
        if (buttonStatus == HIGH || buttonStatus == HELD || buttonStatus == HELD_REPEAT)
        {
            myControlChange(midiChannel, CCvolume, clampToRange<int>(currentVolume, -1, 0, 127));
        }
        break;
        // Osc / Performance
    case BUTTON_1:
        if (keyboardActive && buttonStatus == LOW)
        {
            myNoteOn(midiChannel, SCALE_STEPS[majMin][0] + ((keyboardOct + 2) * 12) + ((keyboardScale - majMin) / 2), 127);
        }

        if (keyboardActive && buttonStatus == HIGH_AFTER_HELD)
        {
            myNoteOff(midiChannel, SCALE_STEPS[majMin][0] + ((keyboardOct + 2) * 12) + ((keyboardScale - majMin) / 2), 0);
        }
        if (buttonStatus == HIGH)
        {
            if (keyboardActive)
            {
                myNoteOff(midiChannel, SCALE_STEPS[majMin][0] + ((keyboardOct + 2) * 12) + ((keyboardScale - majMin) / 2), 0);
                return;
            }

            switch (state)
            {
            case State::OPPAGE1:
                state = State::OPPAGE2;
                singleLED(ledColour::RED, 1);
                break;
            case State::OPPAGE2:
                state = State::OPPAGE3;
                singleLED(ledColour::RED, 1);
                break;
            case State::OPPAGE3:
                state = State::OPPAGE4;
                singleLED(ledColour::RED, 1);
                break;
            case State::OPPAGE4:
                state = State::OPPAGE5;
                singleLED(ledColour::RED, 1);
                break;
            case State::OPPAGE5:
                state = State::OPPAGE6;
                singleLED(ledColour::RED, 1);
                break;
            case State::OPPAGE6:
                state = State::OPPAGE1;
                singleLED(ledColour::RED, 1);
                break;
            case State::PERFORMANCEPAGE:
                state = State::PERFORMANCERECALL;
                singleLED(ledColour::GREEN, 1);
                break;
            case State::PERFORMANCERECALL:
                state = State::MAIN;
                singleLED(ledColour::OFF, 1);
                break;
            case State::PERFORMANCEPATCHEDIT:
                if (currentPerformance.patches[0].UID == 0) // Patch is missing
                {
                    recallPatch(currentBankIndex, patches[currentPatchIndex].patchUID);
                    currentPerformance.patches[0].bankIndex = currentBankIndex;
                    currentPerformance.patches[0].UID = patches[currentPatchIndex].patchUID;
                    currentPerformance.patches[0].patchName = currentPatchName;
                }
                savePerformance();
                state = State::PERFORMANCEENCEDIT;
                singleLED(ledColour::GREEN, 1);
                break;
            case State::PERFORMANCEENCEDIT:
                savePerformance();
                state = State::PERFORMANCEMIDIEDIT;
                singleLED(ledColour::GREEN, 1);
                break;
            case State::PERFORMANCEMIDIEDIT:
                savePerformance();
                if (currentPerformance.performanceName.equals(EMPTYNAME))
                    currentPerformance.performanceName = "";
                nameCursor = currentPerformance.performanceName.length() - 1;
                state = State::RENAMEPERFORMANCE;
                singleLED(ledColour::GREEN, 1);
                break;
            case State::RENAMEPERFORMANCE:
            case State::CHOOSECHARPERFORMANCE:
            case State::DELETECHARPERFORMANCE:
                savePerformance();
                loadPerformanceNames();
                state = State::PERFORMANCERECALL;
                singleLED(ledColour::GREEN, 1);
                break;
            default:
                state = State::OPPAGE1; // show osc parameters
                singleLED(ledColour::RED, 1);
                break;
            }
        }
        if (buttonStatus == HELD)
        {
            if (keyboardActive)
                return;
            if (cardStatus && state != State::PERFORMANCERECALL && state != State::PERFORMANCEPAGE &&
                !inPages(state, State::OPPAGE1, State::OPPAGE6))
            {
                state = State::PERFORMANCERECALL;
                // Get performances from SD card
                loadPerformanceNames();
                singleLED(ledColour::GREEN, 1);
            }
            else
            {
                state = State::MAIN;
                singleLED(ledColour::OFF, 1);
            }
        }
        break;

        // Osc Mod / Arp
    case BUTTON_2:
        if (keyboardActive && buttonStatus == LOW)
        {
            myNoteOn(midiChannel, SCALE_STEPS[majMin][1] + ((keyboardOct + 2) * 12) + ((keyboardScale - majMin) / 2), 127);
        }

        if (keyboardActive && buttonStatus == HIGH_AFTER_HELD)
        {
            myNoteOff(midiChannel, SCALE_STEPS[majMin][1] + ((keyboardOct + 2) * 12) + ((keyboardScale - majMin) / 2), 0);
        }
        if (buttonStatus == HIGH)
        {
            if (keyboardActive)
            {
                myNoteOff(midiChannel, SCALE_STEPS[majMin][1] + ((keyboardOct + 2) * 12) + ((keyboardScale - majMin) / 2), 0);
                return;
            }
            switch (state)
            {
            case State::ALGOPAGE1:
                state = State::ALGOPAGE2;
                singleLED(ledColour::RED, 2);
                break;
            case State::ALGOPAGE2:
                state = State::ALGOPAGE1;
                singleLED(ledColour::RED, 2);
                break;
            case State::ARPPAGE1:
                state = State::ARPPAGE2;
                singleLED(ledColour::GREEN, 2);
                break;
            case State::ARPPAGE2:
                state = State::ARPPAGE1;
                singleLED(ledColour::GREEN, 2);
                break;
            case State::PERFORMANCEPAGE:
                state = State::MAIN;
                singleLED(ledColour::OFF, 1);
                break;
            default:
                state = State::ALGOPAGE1; // show osc mod parameters
                singleLED(ledColour::RED, 2);
                break;
            }
        }
        if (buttonStatus == HELD)
        {
            if (keyboardActive)
                return;
            if (state != State::ARPPAGE1 && state != State::ARPPAGE2 &&
                !inPages(state, State::ALGOPAGE1, State::ALGOPAGE2))
            {
                if (!arpRunning)
                {
                    if (currentSequence.running)
                        sequencerStop();
                    arpRunning = true;
                    setSeqTimerPeriod(currentSequence.bpm);
                    resetNotes();
                    sequencerStart();
                }
                state = State::ARPPAGE1;
                ledsOff(); // Led 1 stays on after sequencerStart()
                singleLED(ledColour::GREEN, 2);
            }
            else if (state == State::ARPPAGE1 || state == State::ARPPAGE2)
            {
                if (arpRunning)
                {
                    arpRunning = false;
                    sequencerStop();
                    state = State::MAIN;
                    singleLED(ledColour::OFF, 2);
                }
            }
            else
            {
                state = State::MAIN;
                singleLED(ledColour::OFF, 2);
            }
        }
        break;

        // Filter / Seq
    case BUTTON_3:
        if (keyboardActive && buttonStatus == LOW)
        {
            myNoteOn(midiChannel, SCALE_STEPS[majMin][2] + ((keyboardOct + 2) * 12) + ((keyboardScale - majMin) / 2), 127);
        }

        if (keyboardActive && buttonStatus == HIGH_AFTER_HELD)
        {
            myNoteOff(midiChannel, SCALE_STEPS[majMin][2] + ((keyboardOct + 2) * 12) + ((keyboardScale - majMin) / 2), 0);
        }
        if (buttonStatus == HIGH)
        {
            if (keyboardActive)
            {
                myNoteOff(midiChannel, SCALE_STEPS[majMin][2] + ((keyboardOct + 2) * 12) + ((keyboardScale - majMin) / 2), 0);
                return;
            }
            switch (state)
            {
            case State::LFOPAGE1:
                state = State::LFOPAGE2;
                singleLED(ledColour::RED, 3);
                break;
            case State::LFOPAGE2:
                state = State::LFOPAGE1;
                singleLED(ledColour::RED, 3);
                break;
            case State::SEQUENCEPAGE:
            case State::SEQUENCERECALL:
                state = State::MAIN;
                singleLED(ledColour::OFF, 3);
                break;
            case State::SEQUENCEEDIT:
                saveSequence();
                currentSequence.recording = false;
                if (currentSequence.SequenceName.equals(EMPTYNAME))
                    currentSequence.SequenceName = "";
                nameCursor = currentSequence.SequenceName.length() - 1;
                state = State::RENAMESEQUENCE;
                singleLED(ledColour::GREEN, 3);
                break;
            case State::RENAMESEQUENCE:
            case State::CHOOSECHARSEQUENCE:
            case State::DELETECHARSEQUENCE:
                saveSequence();
                loadSequenceNames();
                state = State::SEQUENCERECALL;
                singleLED(ledColour::GREEN, 3);
                break;
            default:
                state = State::LFOPAGE1; // show filter parameters
                singleLED(ledColour::RED, 3);
                break;
            }
        }
        if (buttonStatus == HELD)
        {
            if (keyboardActive)
                return;
            if (arpRunning)
            {
                sequencerStop();
                arpRunning = false;
            }

            if (cardStatus && state != State::SEQUENCERECALL && state != State::SEQUENCEEDIT && !inPages(state, State::LFOPAGE1, State::LFOPAGE2) && !currentSequence.running)
            {
                state = State::SEQUENCERECALL;
                loadSequenceNames();
                currentSequence.recording = true;
                singleLED(ledColour::GREEN, 3);
            }
            else if (currentSequence.running && state != State::SEQUENCEPAGE)
            {
                state = State::SEQUENCEPAGE;
                singleLED(ledColour::GREEN, 3);
            }
            else if (currentSequence.running && state == State::SEQUENCEPAGE)
            {
                state = State::SEQUENCEEDIT;
                singleLED(ledColour::GREEN, 3);
            }
            else if (state == State::SEQUENCEEDIT)
            {
                saveSequence();
                state = State::SEQUENCERECALL;
                singleLED(ledColour::GREEN, 3);
            }
            else
            {
                state = State::MAIN;
                singleLED(ledColour::OFF, 3);
            }
        }
        break;

        // Filter Mod / MIDI
    case BUTTON_4:
        if (keyboardActive && buttonStatus == LOW)
        {
            myNoteOn(midiChannel, SCALE_STEPS[majMin][3] + ((keyboardOct + 2) * 12) + ((keyboardScale - majMin) / 2), 127);
        }

        if (keyboardActive && buttonStatus == HIGH_AFTER_HELD)
        {
            myNoteOff(midiChannel, SCALE_STEPS[majMin][3] + ((keyboardOct + 2) * 12) + ((keyboardScale - majMin) / 2), 0);
        }
        if (buttonStatus == HIGH)
        {
            if (keyboardActive)
            {
                myNoteOff(midiChannel, SCALE_STEPS[majMin][3] + ((keyboardOct + 2) * 12) + ((keyboardScale - majMin) / 2), 0);
                return;
            }
            switch (state)
            {
            case State::CTRLPAGE1:
                state = State::CTRLPAGE2;
                singleLED(ledColour::RED, 4);
                break;
            case State::CTRLPAGE2:
                state = State::CTRLPAGE1;
                singleLED(ledColour::RED, 4);
                break;
            case State::MIDIPAGE:
                state = State::MAIN;
                singleLED(ledColour::OFF, 4);
                break;
            case State::PERFORMANCEPAGE:
                state = State::MAIN;
                singleLED(ledColour::OFF, 1);
                break;
            default:
                state = State::CTRLPAGE1; // show filter mod parameters
                singleLED(ledColour::RED, 4);
                break;
            }
        }
        if (buttonStatus == HELD)
        {
            if (keyboardActive)
                return;
            if (state != State::MIDIPAGE && !inPages(state, State::CTRLPAGE1, State::CTRLPAGE2))
            {
                state = State::MIDIPAGE;
                singleLED(ledColour::GREEN, 4);
            }
            else
            {
                state = State::MAIN;
                singleLED(ledColour::OFF, 4);
            }
        }
        break;
        // Amp / Settings
    case BUTTON_5:
        if (keyboardActive && buttonStatus == LOW)
        {
            myNoteOn(midiChannel, SCALE_STEPS[majMin][4] + ((keyboardOct + 2) * 12) + ((keyboardScale - majMin) / 2), 127);
        }

        if (keyboardActive && buttonStatus == HIGH_AFTER_HELD)
        {
            myNoteOff(midiChannel, SCALE_STEPS[majMin][4] + ((keyboardOct + 2) * 12) + ((keyboardScale - majMin) / 2), 0);
        }
        if (buttonStatus == HIGH)
        {
            if (keyboardActive)
            {
                myNoteOff(midiChannel, SCALE_STEPS[majMin][4] + ((keyboardOct + 2) * 12) + ((keyboardScale - majMin) / 2), 0);
                return;
            }
            switch (state)
            {
            case State::TONEPAGE1:
                state = State::TONEPAGE1;
                singleLED(ledColour::RED, 5);
                break;
            case State::SETTINGS:
                state = State::MAIN;
                singleLED(ledColour::OFF, 5);
                break;
            case State::PERFORMANCEPAGE:
                state = State::MAIN;
                singleLED(ledColour::OFF, 1);
                break;
            default:
                state = State::TONEPAGE1; // show amplifier parameters
                singleLED(ledColour::RED, 5);
                break;
            }
        }
        if (buttonStatus == HELD)
        {
            if (keyboardActive)
                return;
            if (state != State::SETTINGS && !inPages(state, State::TONEPAGE1, State::TONEPAGE1))
            {
                switch (state)
                {
                case State::MAIN:
                    state = SETTINGS;
                    break;
                case SETTINGS:
                    state = State::MAIN;
                    break;
                default:
                    state = SETTINGS;
                    break;
                }
                singleLED(ledColour::GREEN, 5);
            }
            else
            {
                state = State::MAIN;
                singleLED(ledColour::OFF, 5);
            }
        }
        break;
        // FX / Panic
    case BUTTON_6:
        if (keyboardActive && buttonStatus == LOW)
        {
            myNoteOn(midiChannel, SCALE_STEPS[majMin][5] + ((keyboardOct + 2) * 12) + ((keyboardScale - majMin) / 2), 127);
        }

        if (keyboardActive && buttonStatus == HIGH_AFTER_HELD)
        {
            myNoteOff(midiChannel, SCALE_STEPS[majMin][5] + ((keyboardOct + 2) * 12) + ((keyboardScale - majMin) / 2), 0);
        }
        if (buttonStatus == HIGH)
        {
            if (keyboardActive)
            {
                myNoteOff(midiChannel, SCALE_STEPS[majMin][5] + ((keyboardOct + 2) * 12) + ((keyboardScale - majMin) / 2), 0);
                return;
            }

            switch (state)
            {
            case State::FXPAGE1:
                state = State::FXPAGE2; // show FX parameters
                break;
            case State::FXPAGE2:
                state = State::FXPAGE1; // show FX parameters
                break;
            default:
                state = State::FXPAGE1; // show FX parameters
                break;
            }

            singleLED(ledColour::RED, 6);
        }
        if (buttonStatus == HELD)
        {
            if (keyboardActive)
                return;
            if (state != State::FXPAGE1 && state != State::FXPAGE2)
            {
                // If Back button held, Panic: drop any MIDI waiting to be read, then all notes off
                midiPanic();
                flashLED(ledColour::GREEN, 6, 250);
            }
            else
            {
                state = State::MAIN;
                singleLED(ledColour::OFF, 6);
            }
        }
        break;

        // Save / Del
    case BUTTON_7:
        if (keyboardActive && buttonStatus == LOW)
        {
            myNoteOn(midiChannel, SCALE_STEPS[majMin][6] + ((keyboardOct + 2) * 12) + ((keyboardScale - majMin) / 2), 127);
        }

        if (keyboardActive && buttonStatus == HIGH_AFTER_HELD)
        {
            myNoteOff(midiChannel, SCALE_STEPS[majMin][6] + ((keyboardOct + 2) * 12) + ((keyboardScale - majMin) / 2), 0);
        }
        if (buttonStatus == HIGH)
        {
            if (keyboardActive)
            {
                myNoteOff(midiChannel, SCALE_STEPS[majMin][6] + ((keyboardOct + 2) * 12) + ((keyboardScale - majMin) / 2), 0);
                return;
            }
            // Cancel out of Delete
            if (state == State::DELETEPATCH)
            {
                state = State::MAIN;
                singleLED(ledColour::OFF, 7);
            }
            else if (state == State::PERFORMANCEPAGE)
            {
                state = State::MAIN;
                singleLED(ledColour::OFF, 1);
            }
            else if (state == State::RENAMEBANK)
            {
                // Save bank name
                state = State::MAIN;
                saveBankName(bankNames[tempBankIndex]);
                while (patches[0].patchUID == 0)
                {
                    tempBankIndex = decTempBankIndex();
                    loadPatchNamesFromBank(tempBankIndex);
                    currentBankIndex = tempBankIndex;
                    currentPatchIndex = 0;
                }
                recallPatch(currentBankIndex, patches[currentPatchIndex].patchUID);
                ledsOff();
            }
            // SAVE
            else if (cardStatus && state != State::PATCHSAVING && state != State::CHOOSECHARPATCH && state != State::DELETECHARPATCH)
            {
                singleLED(ledColour::RED, 7);
                loadBankNames(); // If in the middle of bank renaming and cancelling
                tempBankIndex = currentBankIndex;
                if (patches.size() < PATCHES_LIMIT)
                {
                    // Prompt for patch name
                    currentPatchName = currentPatch.PatchName;
                    currentPatchIndex++;
                    state = State::PATCHSAVING;
                    if (patches[0].patchUID != 0 && patches.back().patchUID != 0)
                        patches.push_back(PatchUIDAndName{0, EMPTYNAME});
                    currentPatchIndex = patches.size() - 1;
                    if (DEBUG)
                        Serial.printf(F("currentPatchIndex: %i\n"), currentPatchIndex);
                }
                else
                {
                    if (DEBUG)
                        Serial.println(F("Over patch limit on this bank"));
                }
            }
            else if (state == State::PATCHSAVING)
            {
                // Save without patch naming
                state = State::SAVE;
                savePatch(currentBankIndex, currentPatchIndex);
                updatePatchnameAndIndex(currentPatchName, currentPatchIndex + 1, patches[currentPatchIndex].patchUID);
                recallPatch(currentBankIndex, patches[currentPatchIndex].patchUID);
                state = State::MAIN;
                singleLED(ledColour::OFF, 7);
            }
            else if (state == State::CHOOSECHARPATCH || state == State::DELETECHARPATCH)
            {
                // Save after patch naming
                state = State::SAVE;
                strlcpy(currentPatch.PatchName, currentPatchName.c_str(), sizeof(currentPatch.PatchName)); // Always terminated
                savePatch(currentBankIndex, currentPatchIndex);
                updatePatchnameAndIndex(currentPatch.PatchName, currentPatchIndex + 1, currentPatch.UID);
                recallPatch(currentBankIndex, patches[currentPatchIndex].patchUID);
                state = State::MAIN;
                singleLED(ledColour::OFF, 7);
                break;
            }
            else
            {
                state = State::MAIN;
                singleLED(ledColour::OFF, 7);
            }
        }
        if (buttonStatus == HELD)
        {
            if (keyboardActive)
                return;
            if (cardStatus && state != State::PATCHSAVING && state != State::DELETEPATCH && state != State::SAVE && state != State::EDITBANK && state != State::RENAMEBANK)
            {
                // DELETE
                state = State::DELETEPATCH;
                loadBankNames(); // If in the middle of bank renaming and cancelling
                tempBankIndex = currentBankIndex;
                singleLED(ledColour::GREEN, 7);
            }
            else
            {
                state = State::MAIN;
                singleLED(ledColour::OFF, 7);
            }
        }
        break;

        // Recall / Init
    case BUTTON_8:
        if (keyboardActive && buttonStatus == LOW)
        {
            myNoteOn(midiChannel, SCALE_STEPS[majMin][7] + ((keyboardOct + 2) * 12) + ((keyboardScale - majMin) / 2), 127);
        }

        if (keyboardActive && buttonStatus == HIGH_AFTER_HELD)
        {
            myNoteOff(midiChannel, SCALE_STEPS[majMin][7] + ((keyboardOct + 2) * 12) + ((keyboardScale - majMin) / 2), 0);
        }
        if (buttonStatus == HIGH)
        {
            if (keyboardActive)
            {
                myNoteOff(midiChannel, SCALE_STEPS[majMin][7] + ((keyboardOct + 2) * 12) + ((keyboardScale - majMin) / 2), 0);
                return;
            }
            // RECALL
            if (!cardStatus)
            {
                state = State::MAIN;
                ledsOff();
                return;
            }
            singleLED(ledColour::RED, 8);
            switch (state)
            {
            case State::PATCHLIST:
                // If current patch is empty in this bank, go to previous bank with valid patch
                while (patches[0].patchUID == 0)
                {
                    tempBankIndex = decTempBankIndex();
                    loadPatchNamesFromBank(tempBankIndex);
                    currentBankIndex = tempBankIndex;
                    currentPatchIndex = 0;
                }
                recallPatch(currentBankIndex, patches[currentPatchIndex].patchUID);
                state = State::MAIN;
                singleLED(ledColour::OFF, 8);
                break;
            default:
                currentPatchIndex = previousPatchIndex; // when coming from cancelled save on last Empty patch
                currentBankIndex = previousBankIndex;
                tempBankIndex = currentBankIndex;
                loadPatchNamesFromBank(tempBankIndex);
                state = State::PATCHLIST; // show patch list
                break;
            }
        }
        if (buttonStatus == HELD)
        {
            if (keyboardActive)
                return;
            if (state != State::PATCHLIST)
            {
                // Reinitialise to default patch
                reinitRestoreState = state;
                state = State::REINITIALISE;
                reinitialisePatch();
                setCurrentPatchData();
                flashLED(ledColour::GREEN, 8, REINIT_STATE_MS);
                // The previous state comes back after REINIT_STATE_MS (from loop()), so the
                // display shows the reinitialise message, without stopping the main loop
                reinitRestorePending = true;
                reinitRestoreTimer = 0;
            }
            else
            {
                state = State::MAIN;
                singleLED(ledColour::OFF, 8);
            }
        }
        break;
    }
    if (state == State::MAIN)
    {
        // Reset MIDI if changed by performance
        midiChannel = getMIDIChannel();
        midiOutCh = getMIDIOutCh();
        MIDIThru = getMidiThru();
    }
    setEncodersState(state);
}

void midiCCOut(byte cc, byte value)
{
    if (midiOutCh > 0 && cc < 129) // Misusing CC for other parameters above 127/128
    {
        if (cc == filterfreq256) // This is CC 128
        {
            // Turn cutoff freq back to 7 bit for MIDI
            cc = CCfilterfreq;
            value = value >> 1;
        }
        usbMIDI.sendControlChange(cc, value, midiOutCh);
        usbHostMIDI.sendControlChange(cc, value, midiOutCh);
        if (MIDIThru == midi::Thru::Off)
            MIDI.sendControlChange(cc, value, midiOutCh); // MIDI DIN is set to Out
    }
}

FLASHMEM void sdCardDetect()
{
    LockGuard lock(displayLock); // The display renders from the state changed here
    if (sdCardInterrupt)
    {
        silence();
        delayMicroseconds(100'000); // Switch bounce
        if (!digitalReadFast(pinCD))
        {
            cardStatus = SD.begin(BUILTIN_SDCARD); // Reinitialise when card inserted
            // ++++ For older sd cards, retry three times
            if (!cardStatus)
                cardStatus = SD.begin(BUILTIN_SDCARD);
            if (!cardStatus)
                cardStatus = SD.begin(BUILTIN_SDCARD);
            if (!cardStatus)
                cardStatus = SD.begin(BUILTIN_SDCARD);
            //++++
            checkSDCardStructure(); // May be a new card inserted
            if (cardStatus)
            {
                loadBankNames();
                loadPatchNamesFromBank(currentBankIndex);
            }
        }
        else
        {
            cardStatus = false;
            state = State::MAIN;
            // arpRunning = false;
            // sequencerStop();
            setEncodersState(state);
            ledsOff();
        }
        sdCardInterrupt = false;
        attachInterrupt(digitalPinToInterrupt(pinCD), sdCardInteruptRoutine, CHANGE);
    }
}

// Ignores CC messages for four seconds after USB Host is plugged in
// Some MIDI controllers send data when connected that changes synth settings
FLASHMEM void usbHostControlChange(byte channel, byte control, byte value)
{
    if (usbHostPluggedIn && usb_host_wait_timer > 4000) // wait 4s
    {
        // When waiting time has expired, all CC messages go through to the normal handler
        myControlChange(channel, control, value);
    }
}

FLASHMEM void checkUSBHostStatus()
{
    if (usbHostMIDI && !usbHostPluggedIn)
    {
        usb_host_wait_timer = 0;
        usbHostPluggedIn = true;
    }
    else if (!usbHostMIDI && usbHostPluggedIn)
    {
        usbHostPluggedIn = false;
    }
}

FLASHMEM void sequencerLEDs()
{
    if (currentSequence.running && currentSequence.step != seq_last_step)
    {
        seq_last_step = currentSequence.step;
        if (currentSequence.step % 16 < 8)
        {
            if ((currentSequence.step % 8 + 1) == 3)
                ledsOff();
            seqLED(ledColour::RED, currentSequence.step % 8 + 1);
        }
        else
        {
            seqLED(ledColour::GREEN, currentSequence.step % 8 + 1);
        }
    }
}

// Audio CPU use by group of audio objects: current / max over the last second, in % of the
// time available
struct CpuGroup
{
    float now = 0, max = 0;
    void add(AudioStream &s)
    {
        now += s.processorUsage();
        max += s.processorUsageMax();
        s.processorUsageMaxReset();
    }
    void print(const __FlashStringHelper *name)
    {
        Serial.print(F("  "));
        Serial.print(name);
        Serial.print(F(" "));
        Serial.print(now, 1);
        Serial.print(F("/"));
        Serial.print(max, 1);
    }
};

FLASHMEM void CPUMonitor()
{
    // Once a second: printing every loop floods USB serial and can stall the main loop
    static elapsedMillis sincePrint;
    if (sincePrint < 1000)
        return;
    sincePrint = 0;

    CpuGroup cg_fm, cg_mix, cg_ens, cg_rev, cg_out;
    cg_fm.add(global.dexed);
    cg_mix.add(global.volumeMixer);
    cg_mix.add(global.ensembleEffectMixerL);
    cg_mix.add(global.ensembleEffectMixerR);
    cg_ens.add(global.ensemble);
    cg_rev.add(global.reverb);
    cg_out.add(global.i2s);
    cg_out.add(global.usbAudio);
    cg_out.add(global.peak);
    cg_out.add(global.scope);
    cg_out.add(global.cpuGuard);

    // Memory is in audio blocks, out of the AudioMemory() allocation in setup().
    // Stack is the most the display thread has used, out of DISPLAY_THREAD_STACK.
    Serial.printf("CPU %.1f/%.1f%%  MEM %u/%u  Display stack %u/%u |", AudioProcessorUsage(), AudioProcessorUsageMax(),
                  (unsigned)AudioMemoryUsage(), (unsigned)AudioMemoryUsageMax(), (unsigned)displayStackUsed(), (unsigned)DISPLAY_THREAD_STACK);
    cg_fm.print(F("dexed"));
    cg_mix.print(F("mix"));
    cg_ens.print(F("ensemble"));
    cg_rev.print(F("reverb"));
    cg_out.print(F("out"));
    Serial.printf(" notes %u xrun %u render max %uus", (unsigned)global.dexed.getNumNotesPlaying(), (unsigned)global.dexed.getXRun(), (unsigned)global.dexed.getRenderTimeMax());
    global.dexed.resetRenderTimeMax();
    Serial.println();
    AudioProcessorUsageMaxReset();
}

// Prints the details of the last crash (if any) once the Serial Monitor is connected
FLASHMEM void crashReportCheck()
{
    static bool checked = false;
    if (checked || !Serial)
        return;
    checked = true;
    if (CrashReport)
        Serial.print(CrashReport);
}

void loop()
{
    // Improves USB digital audio - Checking USB client MIDI frequently causes drop-outs...
    if (loopCount++ > 15)
    {
        checkUSBHostStatus();
        // Drain pending MIDI messages (bounded) so notes aren't queued behind clock/CC traffic.
        // read() returns false straight away when nothing is waiting.
        // USB Host MIDI Class Compliant
        for (uint8_t i = 0; i < MIDI_READS_PER_LOOP && usbHostMIDI.read(midiChannel); i++)
            ;
        // USB Client MIDI
        for (uint8_t i = 0; i < MIDI_READS_PER_LOOP && usbMIDI.read(midiChannel); i++)
            ;
        // MIDI 5 Pin DIN
        for (uint8_t i = 0; i < MIDI_READS_PER_LOOP && MIDI.read(midiChannel); i++)
            ;
        sdCardDetect();
        fmServiceSysex();    // DX7 voice or bank received
        fmServiceSyxFiles(); // .syx import/export chosen on the .syx page
        sequencerLEDs();
        // MIDI clock lost without a Stop (cable pulled, DAW closed): stop following it,
        // otherwise the sequencer waits for it for ever
        if (MIDIClkSignal && sinceMidiClock > MIDI_CLOCK_TIMEOUT_MS)
        {
            if (seqMidiSync)
                midiClockStopped();
            else
                MIDIClkSignal = false;
        }
        crashReportCheck();
        eepromFlushDeferred(); // Settings that were changing, once they've been still
        serviceLEDFlash();
        if (reinitRestorePending && reinitRestoreTimer >= REINIT_STATE_MS)
        {
            reinitRestorePending = false;
            if (state == State::REINITIALISE)
                state = reinitRestoreState;
        }
        // CPU (total and by group), audio memory, display stack and Dexed figures to USB serial, once a second
        CPUMonitor();
        loopCount = 0;
    }
    encoders.read();
    buttons.read();
    // Start note-ons that were waiting for a stolen voice to fade out, and suspend the
    // oscillators of voices that have stopped sounding
    groupvec[activeGroupIndex]->serviceVoices();
    checkPendingPatchRecall();
    serviceFirmwareUpdate();
    static uint32_t overloadsReported = 0;
    if (global.cpuGuard.overloads != overloadsReported)
    {
        overloadsReported = global.cpuGuard.overloads;
        Serial.println(F("Audio CPU overload: a voice was dropped"));
    }
}
