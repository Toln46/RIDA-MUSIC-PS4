# RIDA MUSIC — PS4 Homebrew

RIDA MUSIC is an independent PS4 / GoldHEN homebrew music-player project.

## Current build

- Native 1080p interface with a redesigned sidebar, clearer sections and a persistent player bar.
- DualShock 4 navigation: D-pad / left stick, L1/R1, Cross, Circle and Options.
- Honest empty states: no demo tracks are presented as real music and the player does not pretend to stream.
- Spotify catalogue integration is **blocked until an eligible Spotify Developer app can be configured**.
- Audio streaming is not implemented yet.

## Spotify access limitation

Spotify's current Developer Mode rules require the app owner to have an active Spotify Premium subscription. Without that, a new Development Mode integration cannot be used to authenticate to the Web API. Spotify's Web API can return catalogue metadata, but it does not provide unrestricted full-track audio streams for a custom PS4 player.

RIDA MUSIC will not extract Spotify audio, bypass DRM, or expose a client secret inside the PS4 executable. When eligible API credentials and a permitted provider are available, catalogue search and playback can be implemented through a secure service layer.

## Controls

- Up / Down: move focus in the sidebar
- Cross: open the focused section
- Left / Right: previous / next section
- L1 / R1: previous / next section
- Circle: return to Home
- Options: reset to Home

## Build

Push to the default branch or run **Actions → Build RIDA MUSIC PS4 → Run workflow**. The workflow uses the OpenOrbis toolchain and publishes the generated PKG as a downloadable Actions artifact.

Target: PS4 homebrew / GoldHEN  
Toolchain: OpenOrbis PS4 Toolchain  
Title ID: RIDA00001
