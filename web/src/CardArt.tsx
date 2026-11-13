import { useState } from "react";

// Card pictures are fetched one card at a time, when the card first shows up, so new cards
// need no update: the first source is the picture server EDOPro itself downloads from.
const SOURCES = [
  (code: number) => `https://pics.projectignis.org:2096/pics/${code}.jpg`,
  (code: number) => `https://images.ygoprodeck.com/images/cards_small/${code}.jpg`,
];

// once a source has failed for one card, later cards start from the next one
let firstWorking = 0;

export function CardArt({ code, onLoad }: { code: number; onLoad?: () => void }) {
  const [source, setSource] = useState(firstWorking);
  if (!code || source >= SOURCES.length) return null;
  return (
    <img
      className="art"
      src={SOURCES[source](code)}
      alt=""
      loading="lazy"
      draggable={false}
      onLoad={onLoad}
      onError={() => {
        if (firstWorking === source && source + 1 < SOURCES.length) firstWorking = source + 1;
        setSource(source + 1);
      }}
    />
  );
}
