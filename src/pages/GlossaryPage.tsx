import { useMemo, useState } from "react";
import { Search } from "lucide-react";
import { GLOSSARY, GLOSSARY_TAGS, type GlossaryTag } from "@/lib/glossary";
import { norm } from "@/lib/slug";
import { Link, useRouter } from "@/lib/router";
import { H2, PageHeader, Pill } from "@/components/ui";

const TAG_TONE: Record<GlossaryTag, "navy" | "koi" | "amber" | "emerald"> = {
  Game: "navy",
  Cards: "koi",
  Yaku: "amber",
  Tech: "emerald",
};

export default function GlossaryPage() {
  const [query, setQuery] = useState("");
  const [tag, setTag] = useState<GlossaryTag | null>(null);
  const { route } = useRouter();

  const filtered = useMemo(() => {
    const q = norm(query);
    return GLOSSARY.filter((g) => {
      if (tag && g.tag !== tag) return false;
      if (!q) return true;
      return norm(`${g.term} ${g.kanji ?? ""} ${g.definition}`).includes(q);
    });
  }, [query, tag]);

  const groups = useMemo(() => {
    const map = new Map<string, typeof filtered>();
    for (const g of filtered) {
      const letter = g.term[0].toUpperCase();
      if (!map.has(letter)) map.set(letter, []);
      map.get(letter)!.push(g);
    }
    return [...map.entries()];
  }, [filtered]);

  return (
    <div>
      <PageHeader
        group="Reference"
        level="Beginner"
        title="Glossary"
        lead="Every Japanese and game term on one page – searchable, filterable, and linked into the wiki."
      />

      {/* Sticky search + tags */}
      <div className="sticky top-[3.75rem] z-20 -mx-1 mb-8 border-b border-slate-100 bg-white/95 px-1 py-3 backdrop-blur-md">
        <div className="flex items-center gap-2.5 rounded-xl border border-slate-200 bg-slate-50/70 px-3.5 focus-within:border-navy-300 focus-within:bg-white">
          <Search className="h-4 w-4 flex-none text-slate-400" />
          <input
            value={query}
            onChange={(e) => setQuery(e.target.value)}
            placeholder="Filter terms … (e.g. sake, dealer, bright)"
            aria-label="Filter glossary"
            className="h-11 w-full bg-transparent text-sm text-navy-900 outline-none placeholder:text-slate-400"
          />
        </div>
        <div className="mt-2.5 flex flex-wrap gap-1.5">
          <button
            onClick={() => setTag(null)}
            className={`rounded-full border px-3 py-1 text-xs font-semibold transition ${
              tag === null ? "border-navy-700 bg-navy-800 text-white" : "border-slate-200 text-slate-500 hover:border-navy-300"
            }`}
          >
            All · {GLOSSARY.length}
          </button>
          {GLOSSARY_TAGS.map((t) => (
            <button
              key={t}
              onClick={() => setTag((cur) => (cur === t ? null : t))}
              className={`rounded-full border px-3 py-1 text-xs font-semibold transition ${
                tag === t ? "border-koi-400 bg-koi-100 text-koi-700" : "border-slate-200 text-slate-500 hover:border-koi-300"
              }`}
            >
              {t} · {GLOSSARY.filter((g) => g.tag === t).length}
            </button>
          ))}
        </div>
      </div>

      {groups.length === 0 && (
        <p className="py-8 text-center text-sm text-slate-500">
          No terms match “{query}”. Try clearing the filter.
        </p>
      )}

      {groups.map(([letter, items]) => (
        <section key={letter} className="mb-8">
          <h3 className="mb-3 flex items-center gap-3 font-display text-lg font-black text-koi-500">
            {letter}
            <span className="h-px flex-1 bg-slate-100" />
          </h3>
          <div className="space-y-3">
            {items.map((g) => {
              const highlighted = route.section === `term-${g.slug}`;
              return (
                <div
                  key={g.slug}
                  id={`term-${g.slug}`}
                  className={`scroll-mt-40 rounded-2xl border p-4 transition sm:p-5 ${
                    highlighted ? "border-koi-300 bg-koi-50/50 ring-2 ring-koi-100" : "border-slate-200 bg-white"
                  }`}
                >
                  <div className="flex flex-wrap items-center gap-x-2.5 gap-y-1">
                    <p className="font-display text-[16px] font-bold text-navy-900">{g.term}</p>
                    {g.kanji && <span className="text-sm text-slate-400">{g.kanji}</span>}
                    <Pill tone={TAG_TONE[g.tag]}>{g.tag}</Pill>
                  </div>
                  <p className="mt-1.5 text-sm leading-6 text-slate-600">{g.definition}</p>
                  {g.link && (
                    <p className="mt-2 text-sm">
                      <Link to={g.link}>{g.linkLabel ?? "Learn more"} →</Link>
                    </p>
                  )}
                </div>
              );
            })}
          </div>
        </section>
      ))}

      <H2 id="missing">Missing a term?</H2>
      <p>
        Use the search in the header (<kbd className="rounded-md border border-slate-200 bg-slate-50 px-1.5 py-0.5 font-mono text-[11px] font-semibold text-slate-600">Ctrl K</kbd>) –
        it also covers pages, yaku and all 48 cards. Or browse the <Link to="yaku">yaku table</Link> and the{" "}
        <Link to="karten">card gallery</Link> directly.
      </p>
    </div>
  );
}
