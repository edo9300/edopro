# RKAPro web app

Deck-testing client on EDOPro's duel core (ocgcore-wasm), deployed to GitHub Pages by `.github/workflows/web.yml`.
No online play. Card data, scripts and pictures are fetched from Project Ignis at run time.

Develop: `RKA_DATA_DIR=<folder with cdb/, scripts/, dist/> npm run dev` with a `.env.development.local` pointing
`VITE_CDB_URL`, `VITE_CDB_LIST_URL`, `VITE_SCRIPTS_URL`, `VITE_STRINGS_URL` at `/data/...`.

## Done
- Engine in a worker, .ydk loading (Omega alternate-art ids accepted), release and pre-release card sets.
- Playable duel screen: field, hand, piles, prompts as bottom sheets, card text, card pictures.

## Next, in order
1. Duel Links layout for phones: 3 monster + 3 spell columns are NOT wanted (TCG rules, 5 columns stay); copy the feel:
   large hand fan at the bottom, tap-and-hold card detail, phase button on the right edge, LP bars top and bottom,
   action buttons floating over the tapped card instead of a sheet. Desktop layout = EDOPro's.
2. Hand test options: hand size 1-6, chosen opening hand.
3. Opponent: hand traps (easy / smart), then plays its own turn with board breakers
   (Veiler / Imperm on the player's turn, Imperm at the start of its turn, Lightning Storm, Cosmic Cyclone-style cards).
4. Deck Optimiser: card roles, exact odds, routes learned from saved replays, then end-board suggestion from a random
   hand (beam search over the core, scored against learned end boards).
5. Save states (seed + response log), replays, takeover.
6. Missing prompts: SELECT_SUM, SELECT_COUNTER, ANNOUNCE_RACE / ATTRIB / CARD.
7. Omega deck folder link with write-back, Google Drive sync.
8. Replay to mp4 / GIF.

The C++ fork in `gframe/` already implements 2-5 for desktop (see `deck_optimizer.h`, `handtrap_ai.h`, `single_mode.cpp`);
port the logic from there.

## Handoff notes

Decisions made by the owner
- The web app is the product. The Android APK path is parked (it crashed on launch; cause unknown, no crash log).
- No online play. Built from scratch here, not from the older `ualam677/RKAPro` repo; only its ideas are reused
  (beam line search, Omega replay/deck import, Drive sync through a synced folder first).
- Phone layout should feel like Duel Links but with TCG rules (5 monster and 5 spell/trap zones). Desktop layout should
  feel like EDOPro. Colours: blue and black.
- Purpose of the optimiser: every saved replay of a correct route teaches it that deck's end boards, until it can work
  out the end board from a random hand.
- Omega decks: read from Omega's deck folder and write edits back (or a conversion step if that is simpler).
- Work in small batches; the owner tests on a phone and sends screenshots.

State of the repo
- Branch `ccr-2d230037-wg0h5q`. Only `web.yml` runs on push; the desktop and Android workflows are manual.
- GitHub Pages is on (source: GitHub Actions) at https://ualam677.github.io/RKAProV2/ and the `github-pages`
  environment allows this branch.
- `gframe/` holds a working desktop C++ fork with the optimiser, hand-trap opponent (easy / smart), save states,
  replay takeover and route learning. It is reference material for the web port now.

Things that cost time
- `duelQueryLocation` returns misaligned data when asked for TYPE, LEVEL or RANK. Ask only for CODE, POSITION, ATTACK,
  DEFENSE and read the rest from the card database (see `worker.ts`).
- Omega numbers alternate artworks itself; unknown codes are mapped to the real card just below (`knownCode`).
- Card pictures load on the owner's phone (Project Ignis picture server, YGOPRODeck as fallback). Those hosts, and
  dl.google.com, are blocked in the cloud build environment, so pictures and Android builds cannot be checked there.
- Local testing: Playwright with a phone profile against `npm run dev`, with ProjectIgnis `BabelCDB`, `CardScripts`
  and `Distribution` cloned under `RKA_DATA_DIR` as `cdb/`, `scripts/`, `dist/`.

Not yet confirmed by the owner
- The cropping fix for field cards (cards keep card proportions; defense position is the card turned sideways).
