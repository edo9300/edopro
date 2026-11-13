// Messages between the page and the duel worker.

export interface CardInfo {
  code: number;
  name: string;
}

export type ToWorker =
  | { type: "init" }
  | { type: "start"; main: number[]; extra: number[]; hand: number; seed: number };

export type FromWorker =
  | { type: "progress"; text: string }
  | { type: "ready"; cards: number }
  | { type: "state"; hand: CardInfo[]; prompt: string | null }
  | { type: "error"; text: string };
