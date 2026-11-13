import { StrictMode, useCallback, useEffect, useRef, useState } from "react";
import { createRoot } from "react-dom/client";
import { parseYdk, shuffled, type Deck } from "./deck";
import { Duel } from "./Duel";
import type { CardText, DuelView, FromWorker, ToWorker } from "./engine/protocol";
import "./style.css";

function App() {
  const worker = useRef<Worker | null>(null);
  const [status, setStatus] = useState("Starting");
  const [ready, setReady] = useState(false);
  const [deck, setDeck] = useState<Deck | null>(null);
  const [deckName, setDeckName] = useState("");
  const [view, setView] = useState<DuelView | null>(null);
  const [texts, setTexts] = useState(new Map<number, CardText>());
  const [error, setError] = useState("");

  const post = (message: ToWorker) => worker.current?.postMessage(message);

  useEffect(() => {
    const instance = new Worker(new URL("./engine/worker.ts", import.meta.url), { type: "module" });
    worker.current = instance;
    instance.onmessage = (event: MessageEvent<FromWorker>) => {
      const message = event.data;
      if (message.type === "progress") setStatus(message.text);
      else if (message.type === "ready") {
        setReady(true);
        setStatus(`${message.cards.toLocaleString()} cards loaded`);
      } else if (message.type === "view") setView(message.view);
      else if (message.type === "text") setTexts((known) => new Map(known).set(message.card.code, message.card));
      else if (message.type === "error") setError(message.text);
    };
    instance.postMessage({ type: "init" } satisfies ToWorker);
    return () => instance.terminate();
  }, []);

  const loadDeck = async (file: File | undefined) => {
    if (!file) return;
    setDeck(parseYdk(await file.text()));
    setDeckName(file.name.replace(/\.ydk$/i, ""));
  };

  const start = useCallback(() => {
    if (!deck) return;
    setError("");
    post({ type: "start", main: shuffled(deck.main), extra: deck.extra, hand: 5, seed: Math.floor(Math.random() * 2 ** 31) });
  }, [deck]);

  const requestText = useCallback((code: number) => {
    if (!texts.has(code)) post({ type: "text", code });
  }, [texts]);

  if (view) {
    return (
      <>
        {error && (
          <p className="error toast" onClick={() => setError("")}>
            {error}
          </p>
        )}
        <Duel view={view} texts={texts} requestText={requestText} answer={(picks) => post({ type: "answer", picks })} restart={start} leave={() => setView(null)} />
      </>
    );
  }

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
        <button className="button primary" disabled={!ready || !deck} onClick={start}>
          Test hand
        </button>
      </section>
      {error && <p className="error">{error}</p>}
    </main>
  );
}

createRoot(document.getElementById("root")!).render(
  <StrictMode>
    <App />
  </StrictMode>,
);
