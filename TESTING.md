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
- [ ] Engine (Tone page): save a patch as Modern or OPL, recall another, recall it: the engine
  comes back. A patch saved before this build loads as Mark I
- [ ] Encoder push resets a parameter; holding the buttons still gives Perfm, Arp, Seq, MIDI, Settings

## 3. Patches
- [ ] Save a patch, recall another, recall the saved one: the sound comes back the same
- [ ] Rename and delete a patch
- [ ] A patch from the subtractive Momentum loads as the init voice with its name and effects
- [ ] Program change from MIDI recalls patches

## 4. SysEx (USB)
- [ ] Single voice dump (e.g. from the Dexed plugin): becomes the current voice, name shown;
  the reverb and ensemble of the patch that was playing stay on
- [ ] 32 voice bank dump: 32 patches added to the current bank, names correct
- [ ] While it saves: "Saving received bank", "n of 32" and a bar filling; then "32 voices added"
- [ ] A bank grown past 54 patches (two bank dumps into one bank): all patches listed, in order,
  after recalling another bank and coming back, and after a restart
- [ ] Editing in the Dexed plugin changes the sound live
- [ ] .syx page, top left turned: Export Bank, Send Voice, Send Bank, Send 33-64 ... shown in turn
- [ ] Send Voice: Dexed gets the voice playing now (with unsaved edits), named after the patch
- [ ] Send Bank: Dexed's cartridge shows the bank's first 32 patches; sounds match Momentum
- [ ] Send 33-64 on a bank of 32 or fewer patches: "No patch 33 in the bank"
- [ ] Notes playing during a send: no clicks or hanging notes
- [x] Dump requests answered (checked with a script: voice 163 bytes and bank 4104 bytes,
  headers and checksums right)

## 4b. .syx files (SD card, SYX folder)
- [ ] Edit Banks > DX7 .syx lists the files; Bank and Import File scroll
- [ ] Import a 32 voice bank: "32 of 32 added", names correct, sounds right (compare in Dexed);
  "Importing .syx bank" with a progress bar while it saves
- [ ] Import a single voice file
- [ ] Import into a nearly full bank: "n of 32 added: bank full"
- [ ] Export a bank, then import the exported file into an empty bank: same sounds
- [ ] Open an exported file in the Dexed plugin: same voices and names

## 4c. Voice browser (Recall > DX7 Voices)
- [ ] Recall page shows DX7 Voices top left; pushing it lists the .syx files and plays voice 1
- [ ] Turning Voice plays each voice (names as in Dexed); fast turns don't freeze or click badly
- [ ] Turning File reads the next file; a single voice file shows 1 voice; a non-DX7 file says so
- [ ] Effects of the previous patch stay on while browsing, and on a .syx import
- [ ] Cancel: the previous patch comes back
- [ ] Voice push: back to the main page with the voice, its name shown
- [ ] Save (TL) and the Save button: naming page with the voice name; the saved patch recalls the same
- [ ] Save into a full bank: "The bank is full", still on the browser
- [ ] No SYX folder / no card: message, no crash
- [ ] Import on the .syx page still works (it shares the file reader)

## 5. Arp, sequencer, effects
- [ ] Arpeggiator and sequencer play the FM voice, start/stop as before
- [ ] Ensemble and reverb pages work
- [ ] Performances recall patch and encoder assignments

## 6. Load
- [ ] 16 notes held, ensemble and reverb on: CPU, `xrun 0`, no clicks
- [ ] Display stack (CPUMonitor) stays well below 16384 on every page
- [ ] Changing pages quickly while playing: no clicks, no freeze

Report the CPUMonitor lines and anything that doesn't work.

Screenshots: `python tools/screenshot.py COM7 screen.png` saves what's on the display (needs
pyserial and Pillow; close the Serial Monitor first, with the bin icon in VS Code: closing the
panel leaves it running and holding the port).
