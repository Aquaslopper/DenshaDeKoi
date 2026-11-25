import { useState } from "react";
import { ExternalLink } from "lucide-react";
import { CARDS, cardsOfMonth, GALLERY_URL, MONTHS, TYPE_INFO, TYPE_ORDER, type CardType } from "@/lib/cards";
import { CardThumb } from "@/components/CardModal";
import { Callout, H2, PageHeader, Table, TypeBadge, TYPE_BADGE_STYLES } from "@/components/ui";

const CHIP_ACTIVE: Record<CardType, string> = {
  hikari: "border-amber-400 bg-amber-100 text-amber-800",
  tane: "border-emerald-400 bg-emerald-100 text-emerald-800",
  tanzaku: "border-rose-400 bg-rose-100 text-rose-800",
  kasu: "border-slate-400 bg-slate-200 text-slate-800",
};

export default function CardsPage() {
  const [filter, setFilter] = useState<CardType | null>(null);

  const jump = (month: number) => {
    document.getElementById(`monat-${month}`)?.scrollIntoView({ behavior: "smooth", block: "start" });
  };

  return (
    <div>
      <PageHeader
        group="Playing"
        level="Beginner"
        title="All 48 Cards"
        lead="The complete railway-themed deck, month by month. Click any card for its full details – the filter dims what doesn't match."
      />

      <Callout tone="info" title="The four types at a glance">
        <div className="flex flex-wrap gap-2 pt-1">
          {TYPE_ORDER.map((t) => (
            <TypeBadge key={t} type={t} />
          ))}
        </div>
        <p className="pt-2">
          Amber = brights, green = animals, rose = ribbons, grey = plains. Every card keeps its traditional
          identity – the trains are the artwork's twist.
        </p>
      </Callout>

      {/* Sticky filter bar */}
      <div className="sticky top-[3.75rem] z-20 -mx-1 mb-8 border-b border-slate-100 bg-white/95 px-1 py-3 backdrop-blur-md">
        <div className="flex flex-wrap items-center gap-1.5">
          <button
            onClick={() => setFilter(null)}
            className={`rounded-full border px-3 py-1 text-xs font-semibold transition ${
              filter === null ? "border-navy-700 bg-navy-800 text-white" : "border-slate-200 text-slate-500 hover:border-navy-300"
            }`}
          >
            All · {CARDS.length}
          </button>
          {TYPE_ORDER.map((t) => (
            <button
              key={t}
              onClick={() => setFilter((f) => (f === t ? null : t))}
              className={`rounded-full border px-3 py-1 text-xs font-semibold transition ${
                filter === t ? CHIP_ACTIVE[t] : `${TYPE_BADGE_STYLES[t]} opacity-80 hover:opacity-100`
              }`}
            >
              {TYPE_INFO[t].label} · {TYPE_INFO[t].count}
            </button>
          ))}
        </div>
        <div className="mt-2 flex flex-wrap items-center gap-1">
          <span className="mr-1 text-[10px] font-bold tracking-wider text-slate-400 uppercase">Jump to month</span>
          {MONTHS.map((m) => (
            <button
              key={m.n}
              onClick={() => jump(m.n)}
              title={`${m.name} · ${m.plant}`}
              className="rounded-md border border-slate-200 px-2 py-0.5 font-display text-[13px] font-bold text-navy-700 transition hover:border-koi-300 hover:bg-koi-50 hover:text-koi-600"
            >
              {m.kanji}
            </button>
          ))}
        </div>
      </div>

      {/* Month blocks */}
      {MONTHS.map((m) => (
        <section key={m.n} id={`monat-${m.n}`} className="mb-10 scroll-mt-40">
          <div className="mb-4 flex flex-wrap items-baseline gap-x-3 gap-y-1">
            <span className="flex h-8 w-8 items-center justify-center rounded-lg bg-navy-800 font-display text-sm font-bold text-white">
              {m.n}
            </span>
            <h3 className="font-display text-xl font-bold text-navy-900">{m.name}</h3>
            <span className="font-display text-lg text-koi-500">{m.kanji}</span>
            <span className="text-sm text-slate-400">{m.romaji}</span>
            <span className="text-sm text-slate-500">· {m.plant}</span>
          </div>
          <div className="grid grid-cols-2 gap-4 sm:grid-cols-4">
            {cardsOfMonth(m.n).map((c) => {
              const dimmed = filter !== null && c.type !== filter;
              return (
                <div key={c.stableId} className={`transition-opacity duration-300 ${dimmed ? "opacity-25" : ""}`}>
                  <CardThumb id={c.stableId} dimmed={dimmed} />
                  <p className="mt-2 font-mono text-xs font-semibold text-navy-700">{c.stableId}</p>
                  <p className="mt-0.5 line-clamp-2 min-h-8 text-xs leading-4 text-slate-500">{c.name}</p>
                  <div className="mt-1.5">
                    <TypeBadge type={c.type} compact />
                  </div>
                </div>
              );
            })}
          </div>
        </section>
      ))}

      <H2 id="ids">What the card IDs mean</H2>
      <p>
        Every card has a <strong>stable ID</strong>: the two-digit month plus a type suffix. These IDs are frozen
        – they're used in the code, in this wiki and in the card gallery.
      </p>
      <Table head={["Suffix", "Meaning", "Example"]}>
        <tr>
          <td><code>B</code></td>
          <td>Bright (Hikari)</td>
          <td><code>01-B</code> – Pine and Crane</td>
        </tr>
        <tr>
          <td><code>A</code></td>
          <td>Animal / object (Tane)</td>
          <td><code>06-A</code> – Peony and Butterflies</td>
        </tr>
        <tr>
          <td><code>RP</code></td>
          <td>Red poetry ribbon</td>
          <td><code>03-RP</code> – Cherry Poetry Ribbon</td>
        </tr>
        <tr>
          <td><code>R</code></td>
          <td>Plain red ribbon</td>
          <td><code>04-R</code> – Wisteria Red Ribbon</td>
        </tr>
        <tr>
          <td><code>RB</code></td>
          <td>Blue ribbon</td>
          <td><code>06-RB</code> – Peony Blue Ribbon</td>
        </tr>
        <tr>
          <td><code>P1</code> / <code>P2</code> / <code>P3</code></td>
          <td>Plain (Kasu) 1–3</td>
          <td><code>12-P3</code> – Paulownia Plain 3</td>
        </tr>
      </Table>
      <p>
        Internally each card also has a <strong>numeric ID</strong>, month-major:{" "}
        <code>(month − 1) × 4 + index</code>, where the index is the card's row (0–3). Example: the sake cup{" "}
        <code>09-A</code> is <code>(9 − 1) × 4 + 0 = 32</code>.
      </p>

      <H2 id="special">Special cards worth knowing</H2>
      <ul>
        <li>
          <strong>09-A · Chrysanthemum and Sake Cup</strong> – an animal that also counts as one extra plain, and
          the missing half of both viewing yaku. The most flexible card in the deck.
        </li>
        <li>
          <strong>11-B · Willow and Rainman</strong> – a bright with attitude: he voids Sanko and downgrades
          Shiko to Ame-Shiko. See <a href="#/yaku/yaku-ame-shiko">Ame-Shiko</a>.
        </li>
        <li>
          <strong>11-P1 · Willow and Lightning</strong> – despite the dramatic name, November's “lightning” card
          is a plain. Only the Rainman is a bright.
        </li>
        <li>
          <strong>December</strong> – the Phoenix plus three plains: no animal, no ribbon. The fastest month to
          sweep if you love Kasu.
        </li>
        <li>
          <strong>August</strong> – Moon and Geese share the pampas grass; together with the sake cup the Moon
          scores Tsukimi-zake.
        </li>
      </ul>

      <div className="mt-8 flex flex-wrap items-center gap-3 rounded-2xl border border-slate-200 bg-slate-50/60 p-4 text-sm text-slate-500">
        <span>
          All artwork and metadata come from the repository's{" "}
          <a href={GALLERY_URL} target="_blank" rel="noreferrer" className="inline-flex items-center gap-1">
            card gallery <ExternalLink className="h-3 w-3" />
          </a>{" "}
          – including the train models and motif descriptions shown in each card's detail window.
        </span>
      </div>
    </div>
  );
}
