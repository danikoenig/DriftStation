## Drift Station v0.3.2

- **Opens at 100 %**: the window always opens at its normal size, like One Shooter, Quad Shooter and Repetitor. You can still drag the bottom right corner (70 % to 200 %).

### v0.3.1

- **Uninstaller**: `DriftStation-Uninstaller.pkg` removes Drift Station with a double-click (VST3, Audio Unit, Standalone app). Your projects stay untouched.

### v0.3.0

- **Steady pitch in the delays**: Drift now moves every delay line only on its own side of the stereo field. Before, copies of the same tone could cross and briefly waver in pitch (up to about 40 cents in 4 Delays with high Drift). Now it stays below 1 cent, so drones are rock solid for playing over them.
- **Stereo sway**: the memory output slowly drifts between left and right in the background, like the hardware. Your live input stays in the centre.
- **Resizable window**: drag the bottom right corner (70 % to 200 %).
- **Mix** moved to the far right and highlighted, Limit renamed to **Limiter**

From v0.2.0: input auto gain, Transpose (-24 to +12 st), Limiter with gain-reduction light, "by Dani Koenig" header.

**Install:** download `DriftStation-Installer.pkg` and follow `INSTALL.txt` (Apple Silicon, macOS 11+).
