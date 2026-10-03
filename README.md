# LEGO Star Wars: The Complete Saga — high refresh rate mod

**Experimental • AI-generated • Linux/Steam Proton + DXVK**

A small `dinput8.dll` proxy for the Steam PC release of LEGO Star Wars: The Complete Saga. It targets the display refresh rate and corrects one source of frame-rate-dependent character jump height.

## AI-generated notice

This mod was developed with **OpenAI Codex and Claude Code**. AI assistants produced the implementation, reverse-engineering analysis, test automation, and documentation. It has not received an independent human code audit. The measured results below cover specific tests, not the whole game. See [AI-NOTICE.md](AI-NOTICE.md).

## What it does

- Reads the current display refresh rate at launch and writes a matching `d3d9.maxFrameRate` setting for DXVK.
- Raises the game's internal 85 FPS limiter so DXVK can handle pacing.
- Adjusts airborne character height during the gravity step to approximate the trajectory at 60 FPS when running above 60 FPS.
- Checks the expected gravity instruction bytes before applying the release patches. The original executable on disk is not modified.

The hook uses addresses from one Steam executable. Other releases and game versions are unverified. This is not a general fix for every frame-rate-dependent system.

## Measured results

Tests used an isolated game copy and a headless gamescope virtual display, at 60 and 170 FPS. Heights are in game-world units.

| Test | Original at 60 FPS | Unfixed at 170 FPS | Mod at 170 FPS |
| --- | ---: | ---: | ---: |
| Standing single jump height | 0.460 | 0.448 | 0.460 |
| Standing double jump height | ~0.766 | ~0.756 | ~0.766 |
| Single jump air time | 0.783 s | 0.776 s | 0.782 s |

Walking top speed and a running single jump's distance were comparable in the tested Cantina scenario.

### Known limitations

- Running double jumps produced inconsistent results at 170 FPS (approximately 0.38 and 0.69 height versus 0.77 at 60 FPS). Input timing was suspected, but the cause was **not confirmed**.
- Vehicles, debris, hovering characters, and level-specific mechanics have not been validated. The reported Speeder Showdown obstacle has not been retested with this mod.
- The release DLL was tested on a virtual display. Real-monitor smoothness and refresh-rate detection remain unverified.
- Creating or changing `dxvk.conf` during DLL loading may require a game restart before DXVK uses the new target.
- An existing `dxvk.conf` without the mod's marker is left alone. Set `d3d9.maxFrameRate` yourself in that case.
- The mod writes `dxvk.conf` relative to the working directory. Launch with the game installation directory as the working directory.
- The proxy and patch installation are experimental. A matching byte signature is not a guarantee that every executable variation is safe.
- Native Windows, other graphics wrappers, and coexistence with other `dinput8.dll` mods are untested.

## Install on Linux / Steam Proton

1. Close the game. Download `dinput8.dll` from [Releases](https://github.com/braxten509/lego-star-wars-tcs-high-refresh/releases), or build it below.
2. Back up any existing `dinput8.dll` and `dxvk.conf` in the game directory. Avoid overwriting another mod's proxy DLL.
3. Copy `dinput8.dll` beside `LEGOStarWarsSaga.exe`.
4. In the game's Steam launch options, set:

   ```sh
   WINEDLLOVERRIDES="dinput8=n,b" %command%
   ```

   If you already use launch options or DLL overrides, merge this setting into them.

On launch, the mod creates a marked `dxvk.conf` if no custom file is present. The file sets DXVK's cap to the detected display refresh rate.

### Optional installer script

The original local installer is included for reproducibility. It assumes Steam's default Linux paths, copies the DLL, and edits `pfx/user.reg` directly. It can overwrite an existing DLL or backup, and its override detection is not section-specific. **Prefer the manual installation above.** If using the script, close all processes using that Proton prefix and back up your files first.

```sh
./install.sh "/path/to/Lego Star Wars Saga" "/path/to/steamapps/compatdata/32440"
```

## Disable / uninstall

With the game closed, move this mod's `dinput8.dll` out of the game directory and restore any previous DLL. Restore your prior DXVK configuration; if this mod created the file, remove or move that marked file. Remove only the `dinput8` launch override added for this mod.

If you used the installer, remove its `dinput8` registry override or restore the saved `user.reg.before-high-refresh-mod` only if doing so will not undo later prefix changes. Keep a backup before editing the registry.

## Build

Requires a 32-bit MinGW-w64 C compiler (`i686-w64-mingw32-gcc`) and Bash:

```sh
./build.sh
```

Outputs:

- `dinput8.dll`: release mod.
- `dinput8-probe.dll`: diagnostic variant that writes physics records to `phys.bin` and accepts `LSW_FPS` / `LSW_NO_FIX` for experiments. Do not use this diagnostic build for normal play.

## Repository contents

This repository contains the mod source and original build/install scripts. The initial release preserves the locally tested DLL. It does **not** include game executables, extracted game assets, save files, Proton prefixes, memory dumps, or decompiled game code.

LEGO and Star Wars names belong to their respective owners. This is an unofficial modification, with no affiliation or endorsement. A separately obtained copy of the game is required.

## References

- [Community high-refresh discussion](https://steamcommunity.com/app/32440/discussions/0/1318836262669318816/)
- [DXVK configuration reference](https://github.com/doitsujin/dxvk/blob/master/dxvk.conf)
- [OpenSaga community decompilation](https://github.com/opensagadev/saga), used as an investigation reference; this repository does not redistribute it.
