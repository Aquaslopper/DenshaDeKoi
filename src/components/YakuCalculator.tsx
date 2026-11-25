import { useMemo, useState } from "react";
import { Eraser, Minus, Plus, Wand2 } from "lucide-react";
import { CARDS, cardsOfMonth, MONTHS, TYPE_INFO, TYPE_ORDER, cardPreview } from "@/lib/cards";
import { PROFILE, YAKU, scoreYaku, settle, typeCounts } from "@/lib/yaku";
import { Pill, TypeBadge } from "./ui";

const EXAMPLES: { label: string; cards: string[] }[] = [
  { label: "Sanko", cards: ["01-B", "03-B", "08-B"] },
  { label: "Hanami-zake", cards: ["03-B", "09-A"] },
  { label: "Ino-Shika-Chō + Tane", cards: ["06-A", "07-A", "10-A", "02-A", "04-A"] },
  { label: "Akatan + Aotan", cards: ["01-RP", "02-RP", "03-RP", "06-RB", "09-RB", "10-RB"] },
  { label: "Goko", cards: ["01-B", "03-B", "08-B", "11-B", "12-B"] },
];

const CHIP_STYLES = {
  hikari: "border-amber-200 bg-amber-50 text-amber-700",
  tane: "border-emerald-200 bg-emerald-50 text-emerald-700",
  tanzaku: "border-rose-200 bg-rose-50 text-rose-700",
  kasu: "border-slate-200 bg-slate-100 text-slate-600",
} as const;

export function YakuCalculator() {
  const [selected, setSelected] = useState<Set<string>>(new Set());
  const [calls, setCalls] = useState(0);
  const [oppCalled, setOppCalled] = useState(false);

  const ids = useMemo(() => [...selected], [selected]);
  const score = useMemo(() => scoreYaku(ids), [ids]);
  const counts = useMemo(() => typeCounts(ids), [ids]);
  const result = settle(score.base, oppCalled, calls);
  const formed = YAKU.filter((y) => score.values[y.id] > 0);
  const anySelected = selected.size > 0;

  const toggle = (id: string) => {
    setSelected((prev) => {
      const next = new Set(prev);
      if (next.has(id)) next.delete(id);
      else next.add(id);
      return next;
    });
  };

  const callerFactor = 1 + Math.min(calls, PROFILE.maxCallerFactor - 1);

  return (
    <div className="my-6 rounded-3xl border border-slate-200 bg-white p-4 shadow-[0_1px_3px_rgba(16,28,64,0.05)] sm:p-6">
      {/* Examples */}
      <div className="mb-5 flex flex-wrap items-center gap-2">
        <span className="mr-1 flex items-center gap-1.5 text-xs font-bold tracking-wider text-slate-400 uppercase">
          <Wand2 className="h-3.5 w-3.5" /> Try
        </span>
        {EXAMPLES.map((ex) => (
          <button
            key={ex.label}
            onClick={() => setSelected(new Set(ex.cards))}
            className="rounded-full border border-navy-200 bg-navy-50 px-3 py-1 text-xs font-semibold text-navy-700 transition hover:border-koi-300 hover:bg-koi-50 hover:text-koi-600"
          >
            {ex.label}
          </button>
        ))}
        <button
          onClick={() => setSelected(new Set())}
          className="flex items-center gap-1 rounded-full border border-slate-200 px-3 py-1 text-xs font-semibold text-slate-500 transition hover:border-rose-200 hover:bg-rose-50 hover:text-rose-600"
        >
          <Eraser className="h-3 w-3" /> Clear
        </button>
      </div>

      {/* Month grid */}
      <div className="grid grid-cols-4 gap-2 sm:grid-cols-6 lg:grid-cols-12">
        {MONTHS.map((m) => (
          <div key={m.n} className="flex flex-col gap-1.5">
            <div className="text-center leading-none">
              <span className="block font-display text-lg font-bold text-navy-800">{m.kanji}</span>
              <span className="mt-0.5 block text-[9px] font-semibold tracking-wide text-slate-400 uppercase">
                {m.romaji}
              </span>
            </div>
            {cardsOfMonth(m.n).map((c) => {
              const isSel = selected.has(c.stableId);
              return (
                <button
                  key={c.stableId}
                  onClick={() => toggle(c.stableId)}
                  aria-pressed={isSel}
                  aria-label={`${c.name} (${c.stableId})`}
                  title={`${c.name} (${c.stableId})`}
                  className={`overflow-hidden rounded-md border-2 transition ${
                    isSel
                      ? "border-koi-500 shadow-[0_0_0_3px_#ffe0ee]"
                      : `border-slate-200 ${anySelected ? "opacity-45 hover:opacity-80" : "hover:border-navy-300"}`
                  }`}
                >
                  <img src={cardPreview(c.stableId)} alt="" loading="lazy" className="block h-auto w-full" />
                </button>
              );
            })}
          </div>
        ))}
      </div>

      {/* Counter chips */}
      <div className="mt-5 flex flex-wrap gap-2">
        {TYPE_ORDER.map((t) => (
          <span key={t} className={`rounded-full border px-3 py-1 text-xs font-semibold ${CHIP_STYLES[t]}`}>
            {TYPE_INFO[t].label} {counts[t]}/{TYPE_INFO[t].count}
            {t === "kasu" && score.cup ? " (+1 cup)" : ""}
          </span>
        ))}
        <span className="rounded-full border border-slate-200 bg-white px-3 py-1 text-xs font-semibold text-slate-500">
          {selected.size}/{CARDS.length} selected
        </span>
      </div>

      {/* Detected yaku */}
      <div className="mt-5 rounded-2xl border border-slate-200 bg-slate-50/60 p-4">
        <p className="mb-3 text-xs font-bold tracking-wider text-slate-400 uppercase">Detected yaku</p>
        {!anySelected && <p className="text-sm text-slate-500">Select cards above – or load one of the examples.</p>}
        {anySelected && formed.length === 0 && (
          <p className="text-sm text-slate-500">
            No yaku yet – keep collecting.
            {score.brights === 3 && score.rain && (
              <span className="mt-1 block text-amber-700">
                Three brights including the Rainman (11-B) do <strong>not</strong> form Sanko – he only counts toward
                four- and five-bright yaku.
              </span>
            )}
          </p>
        )}
        {formed.length > 0 && (
          <ul className="space-y-2">
            {formed.map((y) => (
              <li key={y.id} className="flex flex-wrap items-center gap-2 text-sm">
                <span className="font-display font-bold text-navy-900">{y.name}</span>
                <span className="text-xs text-slate-400">{y.kanji}</span>
                <Pill tone={y.group === "brights" ? "amber" : y.group === "ribbons" ? "rose" : y.group === "counting" ? "emerald" : "navy"}>
                  {score.values[y.id]} pts
                </Pill>
                <span className="text-xs text-slate-500">
                  {y.id === "tane" && `${score.animals} animals → ${score.animals} − 4 = ${score.values.tane}`}
                  {y.id === "tanzaku" && `${score.ribbons} ribbons → ${score.ribbons} − 3 = ${score.values.tanzaku}`}
                  {y.id === "kasu" &&
                    `${score.kasuUnits} plain units${score.cup ? " (incl. the cup)" : ""} → ${score.kasuUnits} − 9 = ${score.values.kasu}`}
                  {y.id === "akatan-aotan" && "replaces Akatan and Aotan; the 6 ribbons also add Tanzaku +3"}
                  {y.id === "ame-shiko" && "four brights with the Rainman"}
                  {y.id === "shiko" && "four brights without the Rainman"}
                  {y.id === "sanko" && "three brights without the Rainman"}
                  {y.id === "goko" && "all five brights"}
                  {y.id === "hanami" && "Curtain + Sake Cup"}
                  {y.id === "tsukimi" && "Moon + Sake Cup"}
                  {y.id === "ino-shika-cho" && "Boar + Deer + Butterflies (stacks with Tane)"}
                  {y.id === "akatan" && "three red poetry ribbons"}
                  {y.id === "aotan" && "three blue ribbons"}
                </span>
              </li>
            ))}
          </ul>
        )}
        <div className="mt-3 flex items-center justify-between border-t border-slate-200 pt-3">
          <span className="text-sm font-semibold text-slate-500">Base points</span>
          <span className="font-display text-2xl font-black text-navy-900">{score.base}</span>
        </div>
      </div>

      {/* Multipliers */}
      <div className="mt-5 grid gap-3 sm:grid-cols-2">
        <div className="rounded-2xl border border-slate-200 p-4">
          <p className="mb-2.5 text-sm font-semibold text-navy-900">Your Koi-Koi calls</p>
          <div className="flex items-center gap-3">
            <button
              onClick={() => setCalls((c) => Math.max(0, c - 1))}
              disabled={calls === 0}
              aria-label="Fewer Koi-Koi calls"
              className="flex h-9 w-9 items-center justify-center rounded-xl border border-slate-200 text-navy-700 transition hover:bg-navy-50 disabled:opacity-35"
            >
              <Minus className="h-4 w-4" />
            </button>
            <span className="w-8 text-center font-display text-xl font-black text-navy-900">{calls}</span>
            <button
              onClick={() => setCalls((c) => Math.min(PROFILE.maxCallerFactor - 1, c + 1))}
              disabled={calls >= PROFILE.maxCallerFactor - 1}
              aria-label="More Koi-Koi calls"
              className="flex h-9 w-9 items-center justify-center rounded-xl border border-slate-200 text-navy-700 transition hover:bg-navy-50 disabled:opacity-35"
            >
              <Plus className="h-4 w-4" />
            </button>
            <Pill tone="koi">factor ×{callerFactor}</Pill>
          </div>
          <p className="mt-2 text-xs text-slate-400">Each own call adds ×1, up to ×6.</p>
        </div>

        <div className="rounded-2xl border border-slate-200 p-4">
          <p className="mb-2.5 text-sm font-semibold text-navy-900">Opponent called Koi-Koi</p>
          <button
            role="switch"
            aria-checked={oppCalled}
            onClick={() => setOppCalled((v) => !v)}
            className="flex items-center gap-3"
          >
            <span
              className={`relative h-7 w-12 rounded-full transition ${oppCalled ? "bg-koi-500" : "bg-slate-200"}`}
            >
              <span
                className={`absolute top-0.5 h-6 w-6 rounded-full bg-white shadow transition-all ${oppCalled ? "left-[1.4rem]" : "left-0.5"}`}
              />
            </span>
            <span className="text-sm font-semibold text-slate-600">{oppCalled ? "Yes" : "No"}</span>
            <Pill tone={oppCalled ? "koi" : "slate"}>×{oppCalled ? 2 : 1}</Pill>
          </button>
          <p className="mt-2 text-xs text-slate-400">
            Winning after the opponent's call doubles your award.
          </p>
        </div>
      </div>

      <div className="mt-3 flex flex-wrap items-center gap-2 text-xs">
        <span className="font-semibold text-slate-400 uppercase tracking-wider">7+ rule (automatic):</span>
        {score.base >= PROFILE.sevenThreshold ? (
          <Pill tone="amber">active – base {score.base} ≥ 7 → ×2</Pill>
        ) : (
          <Pill tone="slate">inactive – doubles from 7 base points</Pill>
        )}
      </div>

      {/* Result */}
      <div className="mt-5 rounded-2xl bg-navy-900 p-4 text-center sm:p-5">
        <p className="font-mono text-[13px] leading-6 text-slate-300 sm:text-sm">
          Base {result.base} × {result.sevenFactor} (7+) × {result.counterFactor} (Opp) × {result.callerFactor} (Koi)
        </p>
        <p className="mt-1 font-display text-3xl font-black text-white">
          = {result.award} <span className="text-lg font-bold text-koi-400">pts</span>
        </p>
      </div>

      <p className="mt-4 flex flex-wrap items-center gap-2 text-xs text-slate-400">
        <TypeBadge type="hikari" compact /> brights ·
        <TypeBadge type="tane" compact /> animals ·
        <TypeBadge type="tanzaku" compact /> ribbons ·
        <TypeBadge type="kasu" compact /> plains — logic mirrors <code className="rounded bg-slate-100 px-1 py-0.5 font-mono text-[11px] text-navy-800">ScoreYaku()</code> and{" "}
        <code className="rounded bg-slate-100 px-1 py-0.5 font-mono text-[11px] text-navy-800">CalculateSettlement()</code> from the game source.
      </p>
    </div>
  );
}
