/// <reference lib="webworker" />
// The duel runs here, off the page's thread, on the same core EDOPro uses
// (ocgcore) with the Project Ignis card database and scripts.

import createCore, { OcgDuelMode, OcgLocation, OcgMessageType, OcgPosition, OcgProcessResult, type OcgCardData, type OcgCoreSync } from "ocgcore-wasm";
import coreWasmUrl from "ocgcore-wasm/lib/ocgcore.sync.wasm?url";
import initSqlJs from "sql.js";
import sqlWasmUrl from "sql.js/dist/sql-wasm.wasm?url";
import type { FromWorker, ToWorker } from "./protocol";

// Folder holding cards.cdb and the release / pre-release databases, and a listing of that folder.
const CDB_URL = import.meta.env.VITE_CDB_URL ?? "https://raw.githubusercontent.com/ProjectIgnis/BabelCDB/master";
const CDB_LIST_URL = import.meta.env.VITE_CDB_LIST_URL ?? "https://api.github.com/repos/ProjectIgnis/BabelCDB/contents/";
const SCRIPTS_URL = import.meta.env.VITE_SCRIPTS_URL ?? "https://raw.githubusercontent.com/ProjectIgnis/CardScripts/master";

const send = (message: FromWorker) => postMessage(message);

let core: OcgCoreSync;
const cards = new Map<number, OcgCardData>();
const names = new Map<number, string>();
const scripts = new Map<string, string | null>();

// cards.cdb first, then every newer set: release-*.cdb and prerelease-*.cdb (no Rush, Skill or unofficial cards).
async function databaseFiles(): Promise<string[]> {
  const files = ["cards.cdb"];
  try {
    const listing = (await (await fetch(CDB_LIST_URL)).json()) as { name: string }[];
    for (const { name } of listing) {
      if (/^(release|prerelease)-.*\.cdb$/.test(name) && !/rush|skill|unofficial/.test(name)) files.push(name);
    }
  } catch {
    // offline or rate limited: the main database alone still covers almost every deck
  }
  return files;
}

async function loadCards() {
  send({ type: "progress", text: "Loading card database" });
  const SQL = await initSqlJs({ locateFile: () => sqlWasmUrl });
  const files = await databaseFiles();
  const buffers = await Promise.all(files.map(async (file) => {
    const response = await fetch(`${CDB_URL}/${file}`);
    return response.ok ? new Uint8Array(await response.arrayBuffer()) : null;
  }));
  for (const buffer of buffers) {
    if (!buffer) continue;
    const db = new SQL.Database(buffer);
    // sql.js returns numbers; setcode and race need all 64 bits, so read them as text
    const rows = db.exec(
      "select d.id,d.alias,cast(d.setcode as text),d.type,d.level,d.attribute,cast(d.race as text),d.atk,d.def,t.name from datas d join texts t on t.id=d.id",
    );
    for (const row of rows[0]?.values ?? []) {
      const [id, alias, setcode, type, level, attribute, race, atk, def, name] = row as [number, number, string, number, number, number, string, number, number, string];
      if (cards.has(id)) continue;
      const set = BigInt.asUintN(64, BigInt(setcode));
      const setcodes: number[] = [];
      for (let i = 0n; i < 4n; i++) {
        const code = Number((set >> (16n * i)) & 0xffffn);
        if (code) setcodes.push(code);
      }
      const isLink = (type & 0x4000000) !== 0;
      cards.set(id, {
        code: id,
        alias,
        setcodes,
        type,
        level: level & 0xff,
        attribute,
        race: BigInt.asUintN(64, BigInt(race)),
        attack: atk,
        defense: isLink ? 0 : def,
        lscale: (level >> 24) & 0xff,
        rscale: (level >> 16) & 0xff,
        link_marker: isLink ? def : 0,
      });
      names.set(id, name);
    }
    db.close();
  }
}

// Other simulators (YGO Omega) number alternate artworks themselves, a few above the real passcode.
// A code the database does not know is read as the closest real card just below it.
function knownCode(code: number): number {
  if (cards.has(code)) return code;
  for (let offset = 1; offset < 10; offset++) {
    const card = cards.get(code - offset);
    if (card) return card.alias && cards.has(card.alias) ? card.alias : code - offset;
  }
  return code;
}

// The core asks for scripts synchronously, which a worker can do with a blocking request.
function fetchText(url: string): string | null {
  const request = new XMLHttpRequest();
  request.open("GET", url, false);
  try {
    request.send();
  } catch {
    return null;
  }
  return request.status === 200 ? request.responseText : null;
}

function readScript(path: string): string | null {
  const name = path.split("/").pop()!;
  if (scripts.has(name)) return scripts.get(name)!;
  const folders = /^c\d+\.lua$/.test(name) ? ["official", "pre-release", "unofficial"] : [""];
  let source: string | null = null;
  for (const folder of folders) {
    source = fetchText(`${SCRIPTS_URL}/${folder ? folder + "/" : ""}${name}`);
    if (source !== null) break;
  }
  // alternate artworks share the script of the card they are an artwork of
  if (source === null && folders.length > 1) {
    const alias = cards.get(Number(name.slice(1, -4)))?.alias;
    if (alias) source = readScript(`c${alias}.lua`);
  }
  scripts.set(name, source);
  return source;
}

const isPrompt = (type: number) => (type >= 10 && type <= 26 && type !== 17) || type === 132 || (type >= 140 && type <= 143);

function start(main: number[], extra: number[], hand: number, seed: number) {
  const duel = core.createDuel({
    flags: OcgDuelMode.MODE_MR5,
    seed: [BigInt(seed), 2n, 3n, 4n],
    team1: { drawCountPerTurn: 1, startingDrawCount: hand, startingLP: 8000 },
    team2: { drawCountPerTurn: 1, startingDrawCount: 0, startingLP: 8000 },
    cardReader: (code) => cards.get(code) ?? null,
    scriptReader: (name) => readScript(name) ?? "",
    errorHandler: (_type, text) => {
      if (!/not found/.test(text)) send({ type: "error", text });
    },
  });
  if (!duel) throw new Error("The duel could not be created");
  for (const file of ["constant.lua", "utility.lua"]) core.loadScript(duel, file, readScript(file) ?? "");
  const add = (code: number, location: OcgLocation) =>
    core.duelNewCard(duel, { team: 0, duelist: 0, code, controller: 0, location, sequence: 0, position: OcgPosition.FACEDOWN_DEFENSE });
  for (const code of main) add(knownCode(code), OcgLocation.DECK);
  for (const code of extra) add(knownCode(code), OcgLocation.EXTRA);
  core.startDuel(duel);

  const inHand: number[] = [];
  let prompt: string | null = null;
  for (let guard = 0; guard < 1000 && !prompt; guard++) {
    const status = core.duelProcess(duel);
    for (const message of core.duelGetMessage(duel)) {
      if (message.type === OcgMessageType.DRAW && message.player === 0) inHand.push(...message.drawn.map((card) => card.code));
      if (isPrompt(message.type)) prompt = OcgMessageType[message.type];
    }
    if (status === OcgProcessResult.END) break;
  }
  send({ type: "state", hand: inHand.map((code) => ({ code, name: names.get(code) ?? `#${code}` })), prompt });
}

onmessage = async (event: MessageEvent<ToWorker>) => {
  const message = event.data;
  try {
    if (message.type === "init") {
      await loadCards();
      send({ type: "progress", text: "Starting duel engine" });
      const wasmBinary = await (await fetch(coreWasmUrl)).arrayBuffer();
      core = await createCore({ sync: true, wasmBinary });
      send({ type: "ready", cards: cards.size });
    } else if (message.type === "start") {
      start(message.main, message.extra, message.hand, message.seed);
    }
  } catch (error) {
    send({ type: "error", text: error instanceof Error ? error.message : String(error) });
  }
};
