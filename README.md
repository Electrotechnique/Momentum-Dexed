# Momentum-Dexed

Port of Synth-Dexed to Momentum.

Momentum with a six operator FM engine in place of the subtractive one. The engine is
[Synth_Dexed](https://codeberg.org/dcoredump/Synth_Dexed), the Dexed/DX7 engine used by MicroDexed.
It's the same hardware and the same interface: patches, banks, performances, the arpeggiator,
the sequencer, the ensemble and reverb, MIDI and settings all work as on Momentum.

**Status: prototype.** It compiles, but it hasn't been run on a Momentum yet.

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
the parameter to a default. The Engine setting (Modern, Mark I, OPL) isn't saved in patches yet.

## Envelope macros

The Envelope page (Amp button) changes the envelope rates of all six operators at once:
Attack (rate 1), Decay (rate 2), Decay 2 (rate 3, how fast the sound settles to its sustain
level) and Release (rate 4). -50 to +50, + is slower, push for 0. They are saved with the
patch and the voice keeps its own rates (Op page 4 shows and edits those). Exported .syx files
have the macros put into the rates. They can also be assigned to the performance encoders.

## Patches

Patches are saved in the bank folders as on Momentum. A patch holds the DX7 voice (155 bytes,
as hex) and Momentum's settings: effects, controllers, tone filter, operator on/off.
Patches from the subtractive Momentum load as the init voice, with their name and effects.

DX7 `.syx` files on the SD card go in a folder called `SYX`. On the Edit Banks page (from Save),
push **DX7 .syx**:

- **Bank** (bottom left): the bank to import into, export or send
- **Import File** (bottom right): turn to choose a file, push to add its voices to the bank as
  new patches (32 voice banks and single voices)
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

The voices play through the effects of the patch that was playing when the browser opened.
The controllers, tone filter and envelope macros are the defaults.

DX7 SysEx sent over USB MIDI (the USB-C port or the USB host port):

- single voice dump: becomes the current voice (save it to keep it)
- 32 voice bank dump: the voices are added to the current bank as new patches
- voice parameter changes: applied live, so the Dexed plugin can be used as an editor
- dump requests: Momentum answers as a DX7 does, with the voice playing now or with patches
  1-32 of the current bank

Sending goes to the USB-C port (to Dexed on the computer) and the USB host port.

## Not done yet

- Test on hardware: CPU load with 16 notes plus the effects, output level (volume scaling),
  display timing
- SysEx on the DIN MIDI input (needs a larger SysEx buffer for the MIDI library)
- MIDI clock sync of the LFO (the DX7 LFO has none)
- Panel legends for the new area names (Op, Algo, LFO, Ctrl, Tone)
