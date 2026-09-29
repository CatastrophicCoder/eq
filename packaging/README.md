Spectral Fault
==============

A 16-band parametric and dynamic equalizer by Catastrophic Audio.
For Macs with Apple silicon, macOS 12 or later. Open source under the AGPLv3.

User guide:   https://catastrophiccoder.github.io/eq/
Source code:  https://github.com/CatastrophicCoder/eq


Installing
----------

The installer (SpectralFault-<version>.pkg) puts each format where hosts look for it:

  Audio Unit   /Library/Audio/Plug-Ins/Components/Spectral Fault.component
  VST3         /Library/Audio/Plug-Ins/VST3/Spectral Fault.vst3
  Standalone   /Applications/Spectral Fault.app

You can choose the formats on the installer's "Installation Type" page.
This disk image holds only the Standalone app: drag it onto Applications.


First use: allow it once
------------------------

This build is not signed with an Apple Developer ID, so macOS asks before
running it the first time. If a host skips the plug-in, or macOS says it
cannot check it for malicious software:

  1. Open System Settings > Privacy & Security.
  2. Under Security, click "Open Anyway" next to Spectral Fault.
  3. Restart the host and rescan plug-ins.

In Logic Pro, the plug-in appears as "Catastrophic Audio: Spectral Fault".
If it does not show up, quit Logic completely and open it again.


Uninstalling
------------

Delete the files listed under "Installing". User presets are kept in
~/Library/Audio/Presets/Catastrophic Audio/Spectral Fault/.
