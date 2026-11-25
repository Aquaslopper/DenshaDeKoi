import { useEffect, useMemo, useRef, useState, type KeyboardEvent } from "react";
import { BookOpen, Layers, Library, Search as SearchIcon, Trophy, X } from "lucide-react";
import { PAGES } from "@/lib/pages";
import { YAKU } from "@/lib/yaku";
import { GLOSSARY } from "@/lib/glossary";
import { CARDS, cardPreview, monthOf, TYPE_INFO } from "@/lib/cards";
import { norm } from "@/lib/slug";
import { useRouter } from "@/lib/router";
import { useCardModal } from "./CardModal";
import { Kbd } from "./ui";

type Kind = "Page" | "Yaku" | "Glossary" | "Card";

interface Entry {
  kind: Kind;
  title: string;
  subtitle: string;
  haystack: string;
  to?: string;
  cardId?: string;
}

const KIND_ICON: Record<Kind, typeof BookOpen> = {
  Page: BookOpen,
  Yaku: Trophy,
  Glossary: Library,
  Card: Layers,
};

const KIND_STYLE: Record<Kind, string> = {
  Page: "bg-navy-50 text-navy-600",
  Yaku: "bg-amber-50 text-amber-700",
  Glossary: "bg-emerald-50 text-emerald-700",
  Card: "bg-koi-50 text-koi-600",
};

function buildIndex(): Entry[] {
  const entries: Entry[] = [];
  for (const p of PAGES) {
    entries.push({
      kind: "Page",
      title: p.nav,
      subtitle: `${p.group} · ${p.level}`,
      haystack: norm(`${p.nav} ${p.group} ${p.level} ${p.keywords.join(" ")} ${p.lead}`),
      to: p.id,
    });
  }
  for (const y of YAKU) {
    entries.push({
      kind: "Yaku",
      title: y.name,
      subtitle: `${y.kanji} · ${y.counting ? `${y.points}+` : y.points} pts`,
      haystack: norm(`${y.name} ${y.kanji} ${y.description} ${y.group} yaku`),
      to: `yaku/yaku-${y.id}`,
    });
  }
  for (const g of GLOSSARY) {
    entries.push({
      kind: "Glossary",
      title: g.term,
      subtitle: `${g.tag}${g.kanji ? ` · ${g.kanji}` : ""}`,
      haystack: norm(`${g.term} ${g.kanji ?? ""} ${g.definition} ${g.tag}`),
      to: `glossar/term-${g.slug}`,
    });
  }
  for (const c of CARDS) {
    const m = monthOf(c.month);
    entries.push({
      kind: "Card",
      title: `${c.stableId} · ${c.name}`,
      subtitle: `Month ${c.month} ${m.name} · ${TYPE_INFO[c.type].label}`,
      haystack: norm(`${c.stableId} ${c.name} ${m.name} ${m.romaji} ${m.plant} ${TYPE_INFO[c.type].label} ${TYPE_INFO[c.type].english} ${c.train}`),
      cardId: c.stableId,
    });
  }
  return entries;
}

const INDEX = buildIndex();
const QUICK = INDEX.filter((e) => e.kind === "Page" && ["grundlagen", "yaku", "karten", "glossar"].includes(e.to!));

export function SearchDialog({ onClose }: { onClose: () => void }) {
  const [query, setQuery] = useState("");
  const [active, setActive] = useState(0);
  const inputRef = useRef<HTMLInputElement>(null);
  const listRef = useRef<HTMLDivElement>(null);
  const { navigate } = useRouter();
  const { open } = useCardModal();

  useEffect(() => {
    inputRef.current?.focus();
  }, []);

  const results = useMemo(() => {
    const q = norm(query);
    if (!q) return QUICK;
    const tokens = q.split(" ").filter(Boolean);
    const scored: { e: Entry; score: number }[] = [];
    for (const e of INDEX) {
      if (!tokens.every((t) => e.haystack.includes(t))) continue;
      const title = norm(e.title);
      const score = title.startsWith(q) ? 0 : title.includes(q) ? 1 : 2;
      scored.push({ e, score });
    }
    scored.sort((a, b) => a.score - b.score);
    return scored.slice(0, 12).map((s) => s.e);
  }, [query]);

  useEffect(() => setActive(0), [query]);

  const run = (e: Entry) => {
    onClose();
    if (e.kind === "Card" && e.cardId) open(e.cardId);
    else if (e.to) navigate(e.to);
  };

  const onKeyDown = (ev: KeyboardEvent) => {
    if (ev.key === "ArrowDown") {
      ev.preventDefault();
      setActive((a) => Math.min(a + 1, results.length - 1));
    } else if (ev.key === "ArrowUp") {
      ev.preventDefault();
      setActive((a) => Math.max(a - 1, 0));
    } else if (ev.key === "Enter") {
      ev.preventDefault();
      if (results[active]) run(results[active]);
    } else if (ev.key === "Escape") {
      ev.preventDefault();
      onClose();
    }
  };

  useEffect(() => {
    const el = listRef.current?.querySelector<HTMLElement>(`[data-idx="${active}"]`);
    el?.scrollIntoView({ block: "nearest" });
  }, [active]);

  return (
    <div className="fixed inset-0 z-[60] flex items-start justify-center p-4 pt-[12vh]" role="dialog" aria-modal="true" aria-label="Search the wiki">
      <button aria-label="Close search" className="absolute inset-0 bg-navy-900/45 backdrop-blur-sm" onClick={onClose} />
      <div className="pop-in relative w-full max-w-xl overflow-hidden rounded-2xl bg-white shadow-2xl">
        <div className="flex items-center gap-3 border-b border-slate-100 px-4">
          <SearchIcon className="h-4 w-4 flex-none text-slate-400" />
          <input
            ref={inputRef}
            value={query}
            onChange={(e) => setQuery(e.target.value)}
            onKeyDown={onKeyDown}
            placeholder="Search pages, yaku, glossary and all 48 cards …"
            className="h-13 w-full bg-transparent py-4 text-[15px] text-navy-900 outline-none placeholder:text-slate-400"
            aria-label="Search query"
          />
          <button onClick={onClose} aria-label="Close" className="flex h-8 w-8 flex-none items-center justify-center rounded-lg text-slate-400 hover:bg-slate-100 hover:text-navy-900">
            <X className="h-4 w-4" />
          </button>
        </div>

        <div ref={listRef} className="scrollbar-thin max-h-[50vh] overflow-y-auto p-2">
          {results.length === 0 && (
            <p className="px-3 py-6 text-center text-sm text-slate-500">
              Nothing found for “{query}”. Try a card id like <code className="rounded bg-slate-100 px-1 font-mono text-xs">09-A</code> or a term like “sanko”.
            </p>
          )}
          {results.map((e, i) => {
            const Icon = KIND_ICON[e.kind];
            return (
              <button
                key={`${e.kind}-${e.title}`}
                data-idx={i}
                onClick={() => run(e)}
                onMouseEnter={() => setActive(i)}
                className={`flex w-full items-center gap-3 rounded-xl px-3 py-2.5 text-left transition ${
                  i === active ? "bg-navy-50" : ""
                }`}
              >
                {e.kind === "Card" && e.cardId ? (
                  <img src={cardPreview(e.cardId)} alt="" className="h-11 w-7 flex-none rounded border border-slate-200 object-cover" />
                ) : (
                  <span className={`flex h-9 w-9 flex-none items-center justify-center rounded-lg ${KIND_STYLE[e.kind]}`}>
                    <Icon className="h-4 w-4" />
                  </span>
                )}
                <span className="min-w-0 flex-1">
                  <span className="block truncate text-sm font-semibold text-navy-900">{e.title}</span>
                  <span className="block truncate text-xs text-slate-400">{e.subtitle}</span>
                </span>
                <span className={`flex-none rounded-full px-2 py-0.5 text-[10px] font-bold uppercase tracking-wide ${KIND_STYLE[e.kind]}`}>
                  {e.kind}
                </span>
              </button>
            );
          })}
        </div>

        <div className="flex items-center gap-3 border-t border-slate-100 bg-slate-50/70 px-4 py-2.5 text-[11px] text-slate-400">
          <span className="flex items-center gap-1"><Kbd>↑</Kbd><Kbd>↓</Kbd> navigate</span>
          <span className="flex items-center gap-1"><Kbd>Enter</Kbd> open</span>
          <span className="flex items-center gap-1"><Kbd>Esc</Kbd> close</span>
        </div>
      </div>
    </div>
  );
}
