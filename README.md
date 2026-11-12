# RKAPro

A fork of [Project Ignis: EDOPro](https://github.com/edo9300/edopro) built for testing decks on a phone.
Everything upstream still works. This fork adds the four things below, with large touch targets and windows that fill the screen.

## Deck Optimiser

Deck editor → **Optimise**.

1. Tap a card, then tap what it does. After each tag the next untagged card is selected, so tagging a deck is one tap per card.
2. Read the odds. They are exact (hypergeometric), not a simulation, for going first (5 cards) and going second (6 cards).
3. Read the suggestions under the list. They name the cards to cut or add and show the odds before and after.

| Role | Meaning |
| --- | --- |
| Starter | Starts your combo by itself. |
| Extender | Keeps the combo going or plays through a negate. Not a starter. |
| Engine requirement | Has to be in the deck for the combo, but you never want to draw it. |
| Hand trap | Stops the opponent from your hand. |
| Board breaker | Clears an established board when going second. |
| Brick | Dead card in the opening hand. |
| Tech / other | Staples, traps, flex spots. |

Common hand traps and board breakers are tagged for you (shown as "guessed"). Tags are saved per deck in `config/deck_roles.json`, and the last tag you gave a card is the default for that card in other decks.

"Playable hand" means at least one starter and at most one card you never want to draw (bricks and engine requirements).

## Hand Test

Deck editor → **Hand Test Settings**.

- **Hand size**: 1 to 6.
- **Opening hand**: random, or pick the cards yourself. Slots you leave empty are filled randomly.
- **Opponent**:
  - *No opponent*.
  - *Copy of my deck*: you make every choice for both sides.
  - *Hand traps - Easy*: the opponent uses each hand trap at the first chance it gets, such as Ash Blossom on your first search.
  - *Hand traps - Smart*: the opponent holds each hand trap for your starters and key cards, using the tags from the Deck Optimiser. With no starters tagged it treats every card as worth stopping.
- **Hand traps held**: how many the opponent starts with (1 to 5), dealt at random from the cards you switch on under **Choose hand traps**.

The built-in opponent only answers your plays. On its own turn it passes.

## Save states

During any single player duel (hand test, puzzle, or a replay you took over):

- **Save state** stores the duel at the current decision.
- **Load state** lists the saved states, newest first. Loading one puts you back at that exact decision.

States are kept in `replay/savestates` and survive restarts. **Saved states** in the Hand Test settings loads one without starting a new test first.

## Replays and takeover

Hand tests are recorded like any other duel. While a replay is playing, **Take over** stops the playback and lets you play on from there.

- In normal playback you continue from the start of the turn being shown.
- With **Old Replay Mode** ticked in the replay list you continue from the exact step.
- You make the choices for both players. If the replay is a hand test against the built-in opponent, that opponent keeps playing its side.

Takeover needs the replay to contain its core recording, which every replay saved by EDOPro or this fork does.

## Building

Build exactly like upstream EDOPro. The new code is in headers and one `.inl` file included from an existing source file, so build setups that list their sources by hand, such as [edopro-android](https://github.com/edo9300/edopro-android), work unchanged when pointed at this repository.

Recordings made with the hand trap opponent store its hand size and difficulty next to the script name. Upstream EDOPro will not replay those correctly.

---

# [Project Ignis](https://github.com/ProjectIgnis): EDOPro

The bleeding-edge automatic duel simulator, a fork of the [YGOPro client](https://github.com/Fluorohydride/ygopro).

All YGOPro forks and known automatic duel simulators are powered by the [YGOPro core (ocgcore)](https://github.com/Fluorohydride/ygopro-core), an automated scripting engine for the Yu-Gi-Oh! Official Card Game. EDOPro is powered by our own [ocgcore fork](https://github.com/edo9300/ygopro-core).

Due to many accumulated changes in this client and its core, it is incompatible with simulators not derived from this fork.

This repository is for the game client only. Related Ignis projects:
- [Canonical card script collection](https://github.com/ProjectIgnis/CardScripts)
- [Canonical card databases collection](https://github.com/ProjectIgnis/BabelCdb)
- [WindBot Ignite](https://github.com/ProjectIgnis/windbot/)

## Contributing

Please keep all usage questions and Windows and macOS bug reports on Discord; do not open an issue or pull request for this purpose.
We are not taking suggestions or feature requests and the issue tracker is not to be used for this purpose either.

Otherwise, pull requests are welcome! It might take some time for them to be evaluated since we are pretty swamped with a lot work to be done.

Check out the [wiki](https://github.com/edo9300/edopro/wiki/) for possibly outdated build instructions and a partial user manual.

## Project Ignis

We are an international, open-source collaboration staffed entirely by volunteers and we welcome support on our projects.
Reach out to us on Discord to learn how to contribute and join!

_Ignis_ is the fire and light of knowledge passed from the gods to humanity in Greco-Roman mythology.
This represents our vision for all of our projects and work and recognizes the contribution of every individual on the team.

[Debut announcement on Reddit](https://www.reddit.com/r/yugioh/comments/fvdn7v/presenting_project_ignis_edopro_the_opensource/).

## License

EDOPro is free/libre and open source software licensed under the GNU Affero General Public License, version 3 or later.
Dependencies and resources may be provided under different licenses.
Please see [LICENSE](https://github.com/edo9300/edopro/blob/master/LICENSE) and [COPYING](https://github.com/edo9300/edopro/blob/master/COPYING) for more details.

Yu-Gi-Oh! is a trademark of Shueisha and Konami. This project is not affiliated with or endorsed by Shueisha or Konami.
