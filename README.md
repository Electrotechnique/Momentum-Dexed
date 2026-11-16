# Momentum-Dexed

Port of Synth-Dexed to Momentum.

Momentum with a six operator FM engine in place of the subtractive one. The engine is
[Synth_Dexed](https://codeberg.org/dcoredump/Synth_Dexed), the Dexed/DX7 engine used by MicroDexed.
It's the same hardware and the same interface: patches, banks, performances, the arpeggiator,
the sequencer, the ensemble and reverb, MIDI and settings all work as on Momentum.

**Status: in testing.** It runs on a Momentum (Teensy MicroMod): the pages, patches, the voice
browser and the transfers to and from Dexed work. What's still to be checked on the hardware is
in [TESTING.md](TESTING.md).

## Licence

Synth_Dexed is GPLv3, so this firmware is GPLv3 (see LICENSE). It can be built and sold,
including commercially, provided the source is made available under the same licence.

Based on the [Momentum](https://github.com/ElectroTechnique/Momentum) V2.00 firmware (MIT).
The hardware, build guide and 3D models are in that repository.

Credits: Dexed engine by Pascal Gauthier (asb2m10), DX synth engine by Raph Levien and the
msfa team, Synth_Dexed by Holger Wirtz.

## Building

Open this repository in PlatformIO, or from its root:

```
pio run
```

## SD card

Format the card as FAT32 and copy everything in this repository's `SDCard` folder to its root:

- `Bank1`: 32 starter patches, strings and brass
- `Banknames`: the names of the 8 banks, Bank 1 to Bank 8 (rename them on the Edit Banks page)
- `SYX`: DX7 voice banks, from the [Dexed](https://github.com/asb2m10/dexed) project, to play
  with the voice browser or import. Add your own `.syx` banks here

A blank card works too: Momentum makes the bank, Sequences, Performances and SYX folders when
it starts, with the init voice in Bank1. `tools/make_bank.py` makes a bank folder from voices
in `.syx` files, as the starter bank was made.

## Pages

The buttons keep their positions. Press for the next page of an area, hold for the same
secondary functions as Momentum (Perfm, Arp, Seq, MIDI, Settings).

| Button | Pages | Encoders (TL, TR, BL, BR) |
|---|---|---|
| Osc → **Op** | 1 Frequency | Operator, Mode, Coarse, Fine |
| | 2 Output | Operator, On/Off, Detune, Level |
| | 3 Sensitivity | Operator, Velocity, Amp Mod, Rate Scale |
| | 4 Envelope | Operator, Stage, Rate, Level |
| | 5 Key Scaling | Operator, Break Point, Left Depth, Right Depth |
| | 6 Scaling Curves | Operator, -, Left Curve, Right Curve |
| Osc Mod → **Algo** | 1 Algorithm | Algorithm, Feedback, Osc Sync, Transpose |
| | 2 Pitch Envelope | Stage, -, Rate, Level |
| Filter → **LFO** | 1 | Speed, Delay, Pitch Depth, Amp Depth |
| | 2 | Wave, Key Sync, Pitch Sens, - |
| Filter Mod → **Ctrl** | 1 | Bend Range, Mono, Portamento, - |
| | 2 | Wheel Range, Wheel Dest, AT Range, AT Dest |
| Amp → **Envelope** | 1 Envelope | Attack, Decay, Decay 2, Release |
| | 2 Tone | Cutoff, Resonance, Engine, - |
| FX | as Momentum | as Momentum |

On every Op page the top left encoder selects the operator, and the page stays the same, so
operators can be compared. Envelopes are edited a stage at a time. Pushing an encoder sets
the parameter to a default. The Engine setting (Modern, Mark I, OPL, on the Tone page) is saved
with the patch; patches saved before it was are Mark I, Synth_Dexed's default.

The voice grid on the main page shows the 16 voices: blue while a key or the sustain pedal
holds the note, dark blue while it fades out in its release.

## Envelope macros

The Envelope page (Amp button) changes the envelope rates of all six operators at once:
Attack (rate 1), Decay (rate 2), Decay 2 (rate 3, how fast the sound settles to its sustain
level) and Release (rate 4). -50 to +50, + is slower, push for 0. They are saved with the
patch and the voice keeps its own rates (Op page 4 shows and edits those). Exported .syx files
have the macros put into the rates. They can also be assigned to the performance encoders.

## Patches

Patches are saved in the bank folders as on Momentum. A patch holds the DX7 voice (155 bytes,
as hex) and Momentum's settings: effects, controllers, tone filter, engine, operator on/off and
envelope macros. Patches from the subtractive Momentum load as the init voice, with their name
and effects.

A DX7 voice from outside (an imported or browsed `.syx` file, or SysEx from Dexed) has no place
for Momentum's settings: it gets the effects of the patch that was playing, and the defaults for
the rest (controllers, tone filter, envelope macros, all operators on, the Mark I engine).

DX7 `.syx` files on the SD card go in a folder called `SYX`. On the Edit Banks page (from Save),
push **DX7 .syx**:

- **Bank** (bottom left): the bank to import into, export or send
- **Import File** (bottom right): turn to choose a file, push to add its voices to the bank as
  new patches (32 voice banks and single voices). Saving 32 patches takes a few seconds: a box
  shows the progress
- Top left: turn to choose, push to do it:
  - **Export Bank**: save the bank as 32 voice `.syx` files named after the bank (a bank of
    more than 32 patches makes several files; the last one is filled with init voices)
  - **Send Voice**: send the voice playing now over USB MIDI as a single voice dump, with
    any unsaved edits
  - **Send Bank**, **Send 33-64**, **Send 65-96**, **Send 97-128**: send 32 patches of the
    bank over USB MIDI as a 32 voice bank dump (filled with init voices after the last patch)

Sent and exported voices have the envelope macros put into the rates and the patch name as
the voice name. Effects and controller settings stay on Momentum: a DX7 voice has no place
for them.

## Voice browser

The voices in the `SYX` folder can be played without importing them. On the Recall page, push
**DX7 Voices** (top left):

- **File** (bottom left): the .syx file
- **Voice** (bottom right): the voice. It plays once the encoder stops, so play the keyboard
  while turning. Push to keep the voice and go back to the main page (unsaved)
- **Save** (top left), or the Save button: save the voice as a patch, choosing the bank, slot
  and name as usual
- **Cancel** (top right): back to the patch that was playing

The voices play through the effects of the patch that was playing when the browser opened,
with the defaults for the rest, as imported voices.

## Dexed on a computer

Momentum works with the [Dexed](https://asb2m10.github.io/dexed/) plugin or standalone app as a
DX7 would, over USB MIDI. In Dexed's PARM panel, set DX7 In and DX7 Out to Momentum. (Windows
lets only one program open a MIDI port: close any other that has Momentum's.)

DX7 SysEx received over USB MIDI (the USB-C port or the USB host port):

- single voice dump (in Dexed, send the program from the CART window's right-click menu):
  becomes the current voice, keeping the effects (save it to keep it)
- 32 voice bank dump (send the cartridge): the voices are added to the current bank as new
  patches, with a progress box while they are saved
- voice parameter changes: applied live, so the Dexed plugin can be used as an editor
- dump requests: Momentum answers as a DX7 does, with the voice playing now or with patches
  1-32 of the current bank

Sending (Send Voice and Send Bank on the DX7 .syx page, above) goes to the USB-C port (to
Dexed on the computer) and the USB host port.

## Tools

- `tools/screenshot.py COM7 screen.png`: saves what's on the display, over USB serial (close
  the Serial Monitor first)
- `tools/png_to_4bpp.py`: makes a C header from a PNG for the display, as the Dexed logo on
  the boot page (`src/DexedLogo.h`)

- `tools/make_bank.py`: makes a bank folder (patch files and their index) from voices in
  `.syx` banks

The first two need Python with pyserial and Pillow; make_bank.py needs only Python.

## Not done yet

- Load test on hardware: 16 notes with the reverb on (measured so far: 16 notes with the
  reverb off, 21% CPU, no audio dropouts), output level (volume scaling)
- SysEx on the DIN MIDI input (needs a larger SysEx buffer for the MIDI library)
- MIDI clock sync of the LFO (the DX7 LFO has none)
- Panel legends for the new area names (Op, Algo, LFO, Ctrl, Tone)
