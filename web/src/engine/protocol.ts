// Messages between the page and the duel worker. The worker turns everything
// the duel core says into these plain shapes, so the page knows nothing about the core.

export interface CardRef {
  controller: number;
  location: number;
  sequence: number;
}

export interface CardView extends CardRef {
  code: number; // 0 when the card is hidden
  name: string;
  type: number;
  attack: number;
  defense: number;
  level: number;
  position: number;
  materials: number;
}

export interface PlayerView {
  lp: number;
  deck: number;
  hand: CardView[];
  monsters: (CardView | null)[]; // 0-4 main zones, 5-6 extra monster zones
  spells: (CardView | null)[]; // 0-4 spell/trap zones, 5 field zone
  grave: CardView[];
  banished: CardView[];
  extra: CardView[];
}

export interface Choice {
  label: string;
  card?: CardRef & { code: number; name: string };
  zone?: CardRef;
  // idle and battle commands that belong to the phase bar, not to a card
  phase?: boolean;
}

export type Prompt =
  // pick exactly one (index into choices), or null when cancel is allowed
  | { kind: "choice"; idle: boolean; title: string; choices: Choice[]; cancel: string | null }
  // pick between min and max
  | { kind: "multi"; title: string; choices: Choice[]; min: number; max: number; cancel: boolean };

export interface DuelView {
  turn: number;
  turnPlayer: number;
  phase: string;
  players: [PlayerView, PlayerView];
  prompt: Prompt | null;
  ended: boolean;
}

export interface CardText {
  code: number;
  name: string;
  line: string;
  text: string;
}

export interface StartOptions {
  main: number[];
  extra: number[];
  hand: number;
  seed: number;
}

export type ToWorker =
  | { type: "init" }
  | ({ type: "start" } & StartOptions)
  | { type: "answer"; picks: number[] | null }
  | { type: "text"; code: number };

export type FromWorker =
  | { type: "progress"; text: string }
  | { type: "ready"; cards: number }
  | { type: "view"; view: DuelView }
  | { type: "text"; card: CardText }
  | { type: "error"; text: string };

export const LOCATION = { DECK: 1, HAND: 2, MZONE: 4, SZONE: 8, GRAVE: 16, REMOVED: 32, EXTRA: 64 } as const;
export const POSITION = { FACEUP_ATTACK: 1, FACEDOWN_ATTACK: 2, FACEUP_DEFENSE: 4, FACEDOWN_DEFENSE: 8 } as const;
export const TYPE = { MONSTER: 1, SPELL: 2, TRAP: 4, FUSION: 0x40, SYNCHRO: 0x2000, XYZ: 0x800000, LINK: 0x4000000 } as const;
