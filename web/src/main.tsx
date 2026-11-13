import { StrictMode, useEffect, useRef, useState } from "react";
import { createRoot } from "react-dom/client";
import { parseYdk, shuffled, type Deck } from "./deck";
import type { CardInfo, FromWorker, ToWorker } from "./engine/protocol";
import "./style.css";

function App() {
  const worker = useRef<Worker | null>(null);
  const [status, setStatus] = useState("Starting");
  const [ready, setReady] = useState(false);
  const [deck, setDeck] = useState<Deck | null>(null);
  const [deckName, setDeckName] = useState("");
  const [hand, setHand] = useState<CardInfo[]>([]);
  const [prompt, setPrompt] = useState<string | null>(null);
  const [error, setError] = useState("");

  useEffect(() => {
    const instance = new Worker(new URL("./engine/worker.ts", import.meta.url), { type: "module" });
    worker.current = instance;
    instance.onmessage = (event: MessageEvent<FromWorker>) => {
      const message = event.data;
      if (message.type === "progress") setStatus(message.text);
      else if (message.type === "ready") {
        setReady(true);
        setStatus(`${message.cards.toLocaleString()} cards loaded`);
      } else if (message.type === "state") {
        setHand(message.hand);
        setPrompt(message.prompt);
        setStatus("Your turn");
      } else if (message.type === "error") setError(message.text);
    };
    instance.postMessage({ type: "init" } satisfies ToWorker);
    return () => instance.terminate();
  }, []);

  const loadDeck = async (file: File | undefined) => {
    if (!file) return;
    setDeck(parseYdk(await file.text()));
    setDeckName(file.name.replace(/\.ydk$/i, ""));
    setHand([]);
  };

  const draw = () => {
    if (!deck) return;
    setError("");
    setStatus("Drawing");
    worker.current?.postMessage({
      type: "start",
      main: shuffled(deck.main),
      extra: deck.extra,
      hand: 5,
      seed: Math.floor(Math.random() * 2 ** 31),
    } satisfies ToWorker);
  };

  return (
    <main>
      <header>
        <h1>RKAPro</h1>
        <p className="status">{status}</p>
      </header>
      <section className="panel">
        <label className="button">
          {deck ? `${deckName} (${deck.main.length} cards)` : "Open a deck (.ydk)"}
          <input type="file" accept=".ydk" hidden onChange={(event) => loadDeck(event.target.files?.[0])} />
        </label>
        <button className="button primary" disabled={!ready || !deck} onClick={draw}>
          Test hand
        </button>
      </section>
      {error && <p className="error">{error}</p>}
      {hand.length > 0 && (
        <section className="hand">
          {hand.map((card, index) => (
            <div className="card" key={index}>
              {card.name}
            </div>
          ))}
        </section>
      )}
      {prompt && <p className="status">Engine is waiting for: {prompt}</p>}
    </main>
  );
}

createRoot(document.getElementById("root")!).render(
  <StrictMode>
    <App />
  </StrictMode>,
);
