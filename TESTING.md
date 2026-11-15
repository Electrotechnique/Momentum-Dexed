# Hardware test checklist

For the first runs on a Momentum. Uncomment `CPUMonitor();` in `loop()` (main.cpp) and keep
the Serial Monitor open: it prints CPU, audio memory, display stack and Dexed figures once a second.

## 1. Basics
- [ ] Boots to the main page; patch name, bank, ALG and the six operator bars show (top right)
- [ ] Notes sound from USB, USB host and TRS MIDI; the voice grid lights
- [ ] Pitch bend, mod wheel (after setting Wheel Range/Dest on Ctrl 2) and sustain pedal work
- [ ] Volume buttons change the level; no clipping at full volume with several notes

## 2. Pages and encoders
- [ ] Op (Osc button): 6 pages; TL changes the operator and the other encoders follow it
- [ ] Op 4: Stage selects 1-4, the selected segment is yellow, Rate/Level change the drawing
- [ ] Op 2: On/Off mutes the operator (its bar goes dark grey)
- [ ] Algo (Osc Mod): diagram changes with Algorithm; selected operator outlined yellow
- [ ] LFO, Ctrl, Tone pages: every encoder changes the sound
- [ ] Envelope page (Amp): Attack/Decay/Decay 2/Release change all operators; drawing follows
- [ ] Envelope macros saved with the patch; Op 4 still shows the voice's own rates
- [ ] Encoder push resets a parameter; holding the buttons still gives Perfm, Arp, Seq, MIDI, Settings

## 3. Patches
- [ ] Save a patch, recall another, recall the saved one: the sound comes back the same
- [ ] Rename and delete a patch
- [ ] A patch from the subtractive Momentum loads as the init voice with its name and effects
- [ ] Program change from MIDI recalls patches

## 4. SysEx (USB)
- [ ] Single voice dump (e.g. from the Dexed plugin): becomes the current voice, name shown
- [ ] 32 voice bank dump: 32 patches added to the current bank, names correct
- [ ] Editing in the Dexed plugin changes the sound live

## 4b. .syx files (SD card, SYX folder)
- [ ] Edit Banks > DX7 .syx lists the files; Bank and Import File scroll
- [ ] Import a 32 voice bank: "32 of 32 added", names correct, sounds right (compare in Dexed)
- [ ] Import a single voice file
- [ ] Import into a nearly full bank: "n of 32 added: bank full"
- [ ] Export a bank, then import the exported file into an empty bank: same sounds
- [ ] Open an exported file in the Dexed plugin: same voices and names

## 5. Arp, sequencer, effects
- [ ] Arpeggiator and sequencer play the FM voice, start/stop as before
- [ ] Ensemble and reverb pages work
- [ ] Performances recall patch and encoder assignments

## 6. Load
- [ ] 16 notes held, ensemble and reverb on: CPU, `xrun 0`, no clicks
- [ ] Display stack (CPUMonitor) stays well below 16384 on every page
- [ ] Changing pages quickly while playing: no clicks, no freeze

Report the CPUMonitor lines and anything that doesn't work.
