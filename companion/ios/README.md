# F-91W Health (iOS)

Small SwiftUI app that pairs with the watch and saves its data to Apple Health:

- **Steps**: one sample per day for today and the previous 7 days (from the Steps and History
  characteristics). Each day has a fixed sync identifier, so re-syncing replaces it instead of
  adding duplicates. Health already merges the iPhone's own steps by source priority
  (Health > Steps > Data Sources & Access), so nothing is counted twice.
- **Heart rate**: each successful spot check, timestamped by the watch. The watch only keeps its
  latest reading, so a check is saved if the phone syncs before the next one.

The app keeps a pending connection to the bonded watch, so a sync happens whenever the watch
advertises (hold LIGHT for 1.5 s), even with the app in the background. On connect it also sets
the watch's clock and time zone.

## Install (needs a Mac)

1. Install Xcode from the Mac App Store and open `companion/ios/F91WHealth.xcodeproj`.
2. Xcode > Settings > Accounts: add your Apple ID. Then select the F91WHealth target >
   Signing & Capabilities and pick your team. If the bundle ID is taken, change it to anything unique.
3. On the iPhone, turn on Settings > Privacy & Security > Developer Mode (it appears after the
   phone is plugged into the Mac once), and restart when asked.
4. Plug in the iPhone, choose it as the run destination and press Run. The first time, trust the
   developer under Settings > General > VPN & Device Management.

With a free Apple ID the app stops launching after 7 days; plug in and press Run again to re-sign.
A paid developer account ($99/year) makes that a year.

## First pairing

Open the app, allow Bluetooth and Health, then hold LIGHT on the watch for 1.5 s. iOS asks for a
passkey: type the 6 digits the watch shows (`PA  123456`).

Not yet tested against hardware.
