Spine Animation Studio 1.0.0 for macOS (arm64-x86_64)

Built from projects/spine_preview by scripts/package-macos.sh.

Running it
----------
1. Unzip and drag SpineAnimationStudio.app into /Applications.
2. First launch: right-click the app and choose Open, then confirm.
   (Or, in Terminal:  xattr -cr /Applications/SpineAnimationStudio.app)

   Why: the app is ad-hoc signed because there is no Apple Developer ID
   certificate behind it. macOS therefore marks a copy that arrived over the
   network or AirDrop as quarantined and refuses a plain double-click. Opening
   it once explicitly is the user's way of vouching for it; after that a normal
   double-click works.

Where it keeps things
---------------------
Window layout: ~/Library/Application Support/Spine Animation Studio/layout.ini
GIF exports:   wherever you choose in the save panel.

Nothing is installed outside the app bundle.
