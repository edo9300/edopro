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
