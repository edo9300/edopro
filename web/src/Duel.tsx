import { useEffect, useMemo, useState } from "react";
import { CardArt } from "./CardArt";
import { LOCATION, POSITION, TYPE, type CardRef, type CardText, type CardView, type Choice, type DuelView, type PlayerView } from "./engine/protocol";

interface Props {
  view: DuelView;
  texts: Map<number, CardText>;
  requestText: (code: number) => void;
  answer: (picks: number[] | null) => void;
  restart: () => void;
  leave: () => void;
}

type Sheet = { kind: "card"; card: CardView } | { kind: "pile"; controller: number; pile: "grave" | "banished" | "extra" } | null;

const same = (a: CardRef, b: CardRef) => a.controller === b.controller && a.location === b.location && a.sequence === b.sequence;
// only cards on the field are ever "set"; the hand and the piles just report a face-down position
const faceDown = (card: CardView) =>
  (card.location === LOCATION.MZONE || card.location === LOCATION.SZONE) && (card.position & (POSITION.FACEDOWN_ATTACK | POSITION.FACEDOWN_DEFENSE)) !== 0;
const sideways = (card: CardView) => card.location === LOCATION.MZONE && (card.position & (POSITION.FACEUP_DEFENSE | POSITION.FACEDOWN_DEFENSE)) !== 0;

function frame(card: CardView): string {
  if (!card.code) return "back";
  if (card.type & TYPE.SPELL) return "spell";
  if (card.type & TYPE.TRAP) return "trap";
  if (card.type & TYPE.LINK) return "link";
  if (card.type & TYPE.XYZ) return "xyz";
  if (card.type & TYPE.SYNCHRO) return "synchro";
  if (card.type & TYPE.FUSION) return "fusion";
  return "monster";
}

function CardTile({ card, glow, picked, onTap }: { card: CardView; glow?: boolean; picked?: boolean; onTap?: () => void }) {
  const hidden = !card.code;
  // the name stays as the tile's content until its picture has arrived
  const [art, setArt] = useState(0);
  const pictured = art === card.code;
  const classes = [pictured ? "pictured" : "", "tile", hidden ? "back" : frame(card), sideways(card) ? "sideways" : "", glow ? "glow" : "", picked ? "picked" : "", faceDown(card) && !hidden ? "set" : ""];
  return (
    <button className={classes.join(" ")} onClick={onTap} aria-label={hidden ? "Face-down card" : card.name}>
      {!hidden && <CardArt key={card.code} code={card.code} onLoad={() => setArt(card.code)} />}
      {!hidden && !pictured && <span className="tile-name">{card.name}</span>}
      {!hidden && (card.type & TYPE.MONSTER) !== 0 && card.location === LOCATION.MZONE && (
        <span className="tile-stats">
          {card.attack}
          {(card.type & TYPE.LINK) === 0 && `/${card.defense}`}
        </span>
      )}
      {card.materials > 0 && <span className="tile-badge">{card.materials}</span>}
    </button>
  );
}

export function Duel({ view, texts, requestText, answer, restart, leave }: Props) {
  const [sheet, setSheet] = useState<Sheet>(null);
  const [picks, setPicks] = useState<number[]>([]);
  const prompt = view.prompt;

  // a new prompt starts with nothing picked and nothing open
  useEffect(() => {
    setPicks([]);
    setSheet(null);
  }, [prompt]);

  const idle = prompt?.kind === "choice" && prompt.idle;
  const zonePrompt = prompt?.kind === "multi" && prompt.choices.every((choice) => choice.zone);
  const choices = prompt?.choices ?? [];

  const actionsFor = (card: CardRef) => choices.map((choice, index) => ({ choice, index })).filter(({ choice }) => choice.card && same(choice.card, card));
  const hasAction = (card: CardRef) => !!prompt && !zonePrompt && choices.some((choice) => choice.card && same(choice.card, card));
  const zoneIndex = (zone: CardRef) => (zonePrompt ? choices.findIndex((choice) => choice.zone && same(choice.zone, zone)) : -1);

  const togglePick = (index: number) => {
    if (prompt?.kind !== "multi") return;
    const next = picks.includes(index) ? picks.filter((pick) => pick !== index) : [...picks, index];
    if (next.length > prompt.max) return;
    // nothing left to decide: send it without a confirm tap
    if (next.length === prompt.max && prompt.min === prompt.max) answer(next);
    else setPicks(next);
  };

  const tapCard = (card: CardView) => {
    if (prompt?.kind === "multi" && !zonePrompt) {
      const index = choices.findIndex((choice) => choice.card && same(choice.card, card));
      if (index >= 0) return togglePick(index);
    }
    if (card.code) requestText(card.code);
    setSheet({ kind: "card", card });
  };

  const zone = (player: PlayerView, controller: number, location: number, sequence: number) => {
    const card = (location === LOCATION.MZONE ? player.monsters : player.spells)[sequence] ?? null;
    const ref = { controller, location, sequence };
    const index = zoneIndex(ref);
    const pickedCard = card !== null && prompt?.kind === "multi" && !zonePrompt && picks.some((pick) => choices[pick].card && same(choices[pick].card!, ref));
    return (
      <div className={`zone ${index >= 0 ? "open" : ""} ${picks.includes(index) ? "picked" : ""}`} key={`${controller}-${location}-${sequence}`} onClick={index >= 0 && !card ? () => togglePick(index) : undefined}>
        {card && <CardTile card={card} glow={hasAction(card)} picked={pickedCard} onTap={() => (index >= 0 ? togglePick(index) : tapCard(card))} />}
      </div>
    );
  };

  const row = (controller: number, location: number) => {
    const player = view.players[controller];
    // the opponent sits across the table, so their zones run right to left
    const order = controller === 0 ? [0, 1, 2, 3, 4] : [4, 3, 2, 1, 0];
    return <div className="row">{order.map((sequence) => zone(player, controller, location, sequence))}</div>;
  };

  const extraZone = (mine: number) => {
    const theirs = mine === 5 ? 6 : 5;
    if (view.players[1].monsters[theirs]) return zone(view.players[1], 1, LOCATION.MZONE, theirs);
    return zone(view.players[0], 0, LOCATION.MZONE, mine);
  };

  const piles = (controller: number) => {
    const player = view.players[controller];
    const pile = (label: string, name: "grave" | "banished" | "extra", cards: CardView[]) => (
      <button className={`pile ${cards.some(hasAction) ? "glow" : ""}`} onClick={() => setSheet({ kind: "pile", controller, pile: name })} disabled={cards.length === 0}>
        {label} <b>{cards.length}</b>
      </button>
    );
    return (
      <div className="piles">
        <span className="pile static">
          Deck <b>{player.deck}</b>
        </span>
        {pile("GY", "grave", player.grave)}
        {pile("Banished", "banished", player.banished)}
        {pile("Extra", "extra", player.extra)}
        {controller === 1 && (
          <span className="pile static">
            Hand <b>{player.hand.length}</b>
          </span>
        )}
      </div>
    );
  };

  const phaseChoices = idle ? choices.map((choice, index) => ({ choice, index })).filter(({ choice }) => choice.phase) : [];
  const pickButton = (choice: Choice, index: number) => (
    <button className="button" key={index} onClick={() => answer([index])}>
      {choice.label}
    </button>
  );

  const sheetBody = useMemo(() => {
    if (sheet?.kind === "card") {
      const text = texts.get(sheet.card.code);
      const actions = idle || prompt?.kind === "choice" ? actionsFor(sheet.card) : [];
      return (
        <>
          <div className="sheet-head">
            <CardArt key={sheet.card.code} code={sheet.card.code} />
            <h2>{sheet.card.code ? sheet.card.name : "Face-down card"}</h2>
          </div>
          {text?.line && <p className="muted">{text.line}</p>}
          {actions.length > 0 && <div className="actions">{actions.map(({ choice, index }) => pickButton(choice, index))}</div>}
          {text?.text && <p className="card-text">{text.text}</p>}
          <button className="button ghost" onClick={() => setSheet(null)}>
            Close
          </button>
        </>
      );
    }
    if (sheet?.kind === "pile") {
      const cards = view.players[sheet.controller][sheet.pile];
      return (
        <>
          <h2>{sheet.pile === "grave" ? "Graveyard" : sheet.pile === "banished" ? "Banished" : "Extra Deck"}</h2>
          <div className="list">
            {cards.map((card, index) => (
              <button className={`list-row ${hasAction(card) ? "glow" : ""}`} key={index} onClick={() => tapCard(card)}>
                {card.code ? card.name : "Face-down card"}
              </button>
            ))}
          </div>
          <button className="button ghost" onClick={() => setSheet(null)}>
            Close
          </button>
        </>
      );
    }
    if (!prompt || idle || zonePrompt) return null;
    if (prompt.kind === "choice") {
      return (
        <>
          <h2>{prompt.title}</h2>
          <div className="actions">{prompt.choices.map((choice, index) => pickButton(choice, index))}</div>
          {prompt.cancel && (
            <button className="button ghost" onClick={() => answer(null)}>
              {prompt.cancel}
            </button>
          )}
        </>
      );
    }
    const ready = picks.length >= prompt.min && picks.length <= prompt.max;
    return (
      <>
        <h2>
          {prompt.title} ({prompt.min === prompt.max ? prompt.min : `${prompt.min}-${prompt.max}`})
        </h2>
        <div className="list">
          {prompt.choices.map((choice, index) => (
            <button className={`list-row ${picks.includes(index) ? "picked" : ""}`} key={index} onClick={() => togglePick(index)}>
              {choice.label}
            </button>
          ))}
        </div>
        <div className="actions">
          <button className="button primary" disabled={!ready} onClick={() => answer(picks)}>
            Confirm
          </button>
          {prompt.cancel && (
            <button className="button ghost" onClick={() => answer(null)}>
              Cancel
            </button>
          )}
        </div>
      </>
    );
  }, [sheet, prompt, picks, texts, view]);

  const me = view.players[0];
  const them = view.players[1];
  return (
    <div className="duel">
      <div className="bar">
        <span className="lp them">{them.lp}</span>
        <span className="turn">
          Turn {view.turn} · {view.turnPlayer === 0 ? "You" : "Opponent"} · {view.phase}
        </span>
        <span className="lp">{me.lp}</span>
      </div>
      {piles(1)}
      <div className="field">
        {row(1, LOCATION.SZONE)}
        {row(1, LOCATION.MZONE)}
        <div className="row">
          {zone(them, 1, LOCATION.SZONE, 5)}
          {extraZone(5)}
          <div className="zone none" />
          {extraZone(6)}
          {zone(me, 0, LOCATION.SZONE, 5)}
        </div>
        {row(0, LOCATION.MZONE)}
        {row(0, LOCATION.SZONE)}
      </div>
      {piles(0)}
      {zonePrompt && <p className="hint">{prompt!.title}: tap a highlighted zone</p>}
      <div className="hand">
        {me.hand.map((card) => (
          <CardTile key={card.sequence} card={card} glow={hasAction(card)} picked={prompt?.kind === "multi" && picks.some((pick) => choices[pick].card && same(choices[pick].card!, card))} onTap={() => tapCard(card)} />
        ))}
      </div>
      <div className="controls">
        {phaseChoices.map(({ choice, index }) => (
          <button className="button primary" key={index} onClick={() => answer([index])}>
            {choice.label}
          </button>
        ))}
        <button className="button" onClick={restart}>
          New hand
        </button>
        <button className="button ghost" onClick={leave}>
          Exit
        </button>
      </div>
      {view.ended && <p className="hint">The duel has ended.</p>}
      {sheetBody && <div className="sheet">{sheetBody}</div>}
    </div>
  );
}
