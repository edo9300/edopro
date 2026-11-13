// EDOPro deck files (.ydk): one passcode per line under #main, #extra and !side.

export interface Deck {
  main: number[];
  extra: number[];
  side: number[];
}

export function parseYdk(text: string): Deck {
  const deck: Deck = { main: [], extra: [], side: [] };
  let section: keyof Deck = "main";
  for (const raw of text.split(/\r?\n/)) {
    const line = raw.trim();
    if (line === "#main") section = "main";
    else if (line === "#extra") section = "extra";
    else if (line === "!side") section = "side";
    else if (/^\d+$/.test(line)) deck[section].push(Number(line));
  }
  return deck;
}

export function shuffled<T>(items: T[]): T[] {
  const copy = [...items];
  for (let i = copy.length - 1; i > 0; i--) {
    const j = Math.floor(Math.random() * (i + 1));
    [copy[i], copy[j]] = [copy[j], copy[i]];
  }
  return copy;
}
