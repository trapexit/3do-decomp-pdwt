# Plumbers Don't Wear Ties (Enhanced) for 3DO

<p align="center">
  <img src="assets/pdwt_banner.png">
</p>

This repository contains a reconstructed, buildable version of the
Plumbers Don't Wear Ties executable for 3DO. Given the relative
simplicity of the game this is not intended as a preservation level
decompilation though it was decompiled and reconstructed using the
original executable and debug symbols file.

The story, decisions, secret paths, video sequences, and original disc
assets remain the foundation of this project. The changes below are
intentional enhancements to that reconstructed baseline rather than
claims about the behavior of the original binary.


## Enhancements

### Reference-calibrated story timing

Every still/CEL story scene now uses a shared `AudioTime` scene clock
instead of letting fixed VBL waits and asset-loading time accumulate
into visible drift.  The current schedules were calibrated against
reference video cuts and the exact lengths of the bundled AIFF tracks.

This provides several practical improvements:

- image changes stay aligned with narration on fast CD emulation and cached
  media;
- CEL loading and drawing overhead shortens the current wait rather than
  shifting every later cut;
- scene audio is allowed to reach its intended endpoint instead of being
  stopped when an undersized visual schedule finishes early;
- pause time is excluded from the scene timeline; and
- special sequences such as the SC02 shake, SC04 rating overlay, and SC25
  bullet-hole animation remain synchronized with their surrounding scenes.

The calibration also corrects source-order issues in several old scene tables,
including SC15, SC24, SC28, and SC29.


### Pause, skip, and step-back controls

Still/CEL story scenes now remain interactive while images, narration,
and transitions are running.

| Control | During a story scene |
| --- | --- |
| **Start** | Pause or resume the scene |
| **X** | Skip the current scene |
| **R** | Step back through the story flow |

Pausing preserves the currently displayed frame, pauses the sound
spooler and scene clock together, and displays a centered `PAUSED`
panel using one of three randomly selected originally unused images
from the title. Resuming restores the scene frame before audio and
calibrated timing continue.

These controls are serviced one field at a time, including during
scene waits and animated transition effects, so they do not depend on
reaching the next slide boundary.


### Responsive decision screens

Decision narration now uses a serviced sound-file player instead of
blocking the decision loop. A highlighted choice can speak while input
and screen updates continue; moving to another choice stops the old
prompt and starts the new one.

| Control | On a decision screen |
| --- | --- |
| **D-pad** | Change the highlighted choice |
| **A** | Confirm the highlighted choice |
| **L** | Show the original control-help screen while held |
| **R** | Return to the previous decision point |

The same audio path is also used for story narration, which makes
pause/resume and scene-clock synchronization possible without
launching a separate music task.


### Presentation corrections

The enhanced source fixes several visible defects discovered after the
initial reconstruction:

- SC25 bullet holes can use the full 240-pixel screen height rather than only
  the upper half;
- calibrated close effects begin at the intended visual or audio boundary;
- known duplicate, skipped, and misordered scene-table entries were corrected;
- screen changes are tracked so pause/resume restores the actual displayed
  frame; and


### Playback and lifecycle hardening

The reconstruction was also hardened for repeated, complete
playthroughs:

- DataStreamer buffer sizes and subscriber chunks are validated before use;
- stream, subscriber, message, audio, graphics, and input resources are tracked
  and released on failure as well as normal completion;
- stream-wrapper, controller, asset-loading, and sound-player errors propagate
  to their callers instead of being silently treated as success;
- decision and story audio cleanly stop, unload, and free partially initialized
  players; and
- the showcase loop detects non-progress and has a bounded transition count
  rather than being able to loop forever on corrupt state.

These changes are mostly invisible when everything succeeds, but they
remove a large class of hangs, stale stream state, leaks, and
misleading follow-on failures.


## Download

As I do not hold the copyright for the game or assets I'm not in a
position to upload a complete ISO. However, I do have
[xdelta3](https://github.com/jmacd/xdelta) patch files available.

Find the files on the [Releases page](releases/)

The md5sum of the source "Plumbers Don't Wear Ties (USA).bin" is `6bb8b63a6d1c1c187d1c2e882f378a4f`.

```
xdelta3 -d -s "Plumbers Don't Wear Ties (USA).bin" pdwt_enhanced_v1.0.xdelta "PDWT Enhanced v1.0.iso"
```

If you want a GUI to apply the patch you can use [Delta
Patcher](https://github.com/marco-calautti/DeltaPatcher).

Keep in mind that the patched output is actually a .iso and not a .bin
so you should rename the file to .iso after patched.


Was created using the following:
```
xdelta3 -9 -S lzma -B1073741824 -e -s "Plumbers Don't Wear Ties (USA).bin" iso/pdwt_enhanced.iso pdwt_enhanced.xdelta
```


## Building

### Requirements

Use the [3do-devkit](https://github.com/trapexit/3do-devkit), including the ARM
SDT compiler/linker and the `3dt`, `3it`, and `modbin` tools. Point
`.devkit-path` at the local devkit checkout, or export `TDO_DEVKIT_PATH` before
activating the environment.

First you need the original assets. Rip your copy of PDWT and use
`3dt` to unpack the contents into `takeme/`
```sh
source ./activate-env
3dt unpack --only-assets "Plumbers Don\'t Wear Ties (USA).bin" -o takeme/
```

The default build compiles the C source, creates
`takeme/LaunchMe`, and packs the complete disc filesystem into the
`iso/` directory:
```sh
make clean
make
```

Useful targets and options:

```sh
make launchme       # rebuild only the LaunchMe executable
make iso            # build or refresh the disc image
make run            # launch iso through the configured run-iso helper
```


## Repository layout

- `src/` — reconstructed source plus the post-reconstruction timing, audio, and
  scene-control systems.
- `takeme/` — the disc filesystem packed into the ISO, including the original
  media and system files.
- `Makefile` and `activate-env` — native 3DO devkit build and environment setup.


## Reconstruction note

This is a semantic source reconstruction, not an original source
release and not a byte-identical rebuild. The original executable,
symbols, disassembly, and SDK behavior were used to recover the
program structure and game flow. The post-reconstruction features
described above intentionally make the current build behaviorally
different in the areas of timing, controls, audio service, and error
handling.


## FAQ

### Why? Why PDWT?

[Because I love the game.](https://gamefaqs.gamespot.com/3do/584456-plumbers-dont-wear-ties/faqs/18976)


### Can you provide a iso to download?

I would love to and maybe it would be safe given the title is so
easily found to download but I don't wish to risk it. If I ever get
permission from the copyright holders to do so, I will.


### Do you have any additional plans for this enhanced version?

Yes. I would like to improve the quality of the images and use a never
used feature of the hardware that allows for higher color still images
to be displayed. Also, clean up the menu audio.

I've actually never played Plumbers Don't Wear Ties: Definitive
Edition but it seems to me that the 3DO assets were used rather than
original assets rescanned or otherwise touched up. So perhaps we can
create the *true* definitive edition.


### Why not a byte-for-byte reproduction?

A number of factors make that complicated. The two major ones being
that the OS, libraries, and compilers changed over the lifetime
of the console and what we have available today. The goal of the 3DO
Decomp Project is not to recreate titles in C89 with a byte-for-byte
identical output to the original but to create a functionally
equivalent version that compiles against the modern [3DO
DevKit](https://github.com/trapexit/3do-devkit).


## References

* https://www.patreon.com/trapexit/posts/announcing-3do-164834756
* https://www.twitch.tv/3dodev
* https://3dodev.com
* https://github.com/trapexit/3do-devkit
* https://gamefaqs.gamespot.com/3do/584456-plumbers-dont-wear-ties/faqs/18976


## Donations / Sponsorship

If you find the work I'm doing valuable please consider supporting its
ongoing development.

https://github.com/trapexit/support

