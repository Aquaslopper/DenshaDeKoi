import {
  createContext,
  useCallback,
  useContext,
  useEffect,
  useState,
  type ReactNode,
} from "react";
import { ChevronLeft, ChevronRight, ExternalLink, TrainFront, X } from "lucide-react";
import {
  CARDS,
  cardById,
  cardFullUrl,
  cardPreview,
  cardsOfMonth,
  monthOf,
} from "@/lib/cards";
import { yakuById, yakuForCard } from "@/lib/yaku";
import { Link } from "@/lib/router";
import { Pill, TypeBadge } from "./ui";

interface CardModalValue {
  open: (stableId: string) => void;
}

const CardModalContext = createContext<CardModalValue>({ open: () => {} });

export function useCardModal(): CardModalValue {
  return useContext(CardModalContext);
}

/** Small embedded preview that opens the detail modal on click. */
export function CardThumb({
  id,
  className = "",
  dimmed = false,
}: {
  id: string;
  className?: string;
  dimmed?: boolean;
}) {
  const { open } = useCardModal();
  const card = cardById(id);
  if (!card) return null;
  return (
    <button
      onClick={() => open(id)}
      aria-label={`Open card ${id} – ${card.name}`}
      className={`group block overflow-hidden rounded-lg border border-slate-200 bg-white transition hover:border-koi-300 hover:shadow-md focus-visible:border-koi-400 ${
        dimmed ? "opacity-30 hover:opacity-70" : ""
      } ${className}`}
    >
      <img
        src={cardPreview(id)}
        alt={`${card.name} (${id})`}
        loading="lazy"
        className="block h-auto w-full transition duration-200 group-hover:scale-[1.03]"
      />
    </button>
  );
}

const RIBBON_LABEL: Record<string, string> = {
  RED_POETRY: "Red poetry ribbon",
  BLUE: "Blue ribbon",
  RED_PLAIN: "Red ribbon",
};

function Detail({ id, onClose }: { id: string; onClose: () => void }) {
  const { open } = useCardModal();
  const card = cardById(id)!;
  const month = monthOf(card.month);
  const [loaded, setLoaded] = useState(false);
  const idx = CARDS.findIndex((c) => c.stableId === id);
  const prev = CARDS[(idx - 1 + CARDS.length) % CARDS.length];
  const next = CARDS[(idx + 1) % CARDS.length];
  const yaku = yakuForCard(id);

  const go = useCallback(
    (dir: -1 | 1) => {
      const i = CARDS.findIndex((c) => c.stableId === id);
      open(CARDS[(i + dir + CARDS.length) % CARDS.length].stableId);
    },
    [id, open],
  );

  useEffect(() => {
    const onKey = (e: KeyboardEvent) => {
      if (e.key === "Escape") onClose();
      else if (e.key === "ArrowLeft") go(-1);
      else if (e.key === "ArrowRight") go(1);
    };
    window.addEventListener("keydown", onKey);
    return () => window.removeEventListener("keydown", onKey);
  }, [go, onClose]);

  return (
    <div className="fixed inset-0 z-[70] flex items-center justify-center p-3 sm:p-6" role="dialog" aria-modal="true" aria-label={`Card ${id}`}>
      <button aria-label="Close card details" className="absolute inset-0 bg-navy-900/45 backdrop-blur-sm" onClick={onClose} />
      <div className="pop-in scrollbar-thin relative max-h-[92vh] w-full max-w-2xl overflow-y-auto rounded-3xl bg-white shadow-2xl">
        <button
          onClick={onClose}
          aria-label="Close"
          className="absolute top-3 right-3 z-10 flex h-9 w-9 items-center justify-center rounded-xl bg-white/90 text-slate-500 shadow-sm transition hover:bg-slate-100 hover:text-navy-900"
        >
          <X className="h-5 w-5" />
        </button>

        <div className="grid gap-6 p-5 sm:grid-cols-[230px_1fr] sm:p-7">
          {/* Image: embedded preview first, original fades in once loaded */}
          <div>
            <div className="relative aspect-[602/928] overflow-hidden rounded-xl border border-slate-200 bg-slate-50">
              <img src={cardPreview(id)} alt={card.name} className="absolute inset-0 h-full w-full object-cover" />
              <img
                src={cardFullUrl(id)}
                alt=""
                onLoad={() => setLoaded(true)}
                className={`absolute inset-0 h-full w-full object-cover transition-opacity duration-500 ${loaded ? "opacity-100" : "opacity-0"}`}
              />
            </div>
            <a
              href={cardFullUrl(id)}
              target="_blank"
              rel="noreferrer"
              className="mt-2.5 flex items-center justify-center gap-1.5 text-xs font-medium text-navy-600 underline decoration-navy-200 underline-offset-2 hover:text-koi-600"
            >
              <ExternalLink className="h-3.5 w-3.5" />
              Original image (602 × 928)
            </a>
          </div>

          {/* Details */}
          <div className="min-w-0">
            <p className="text-xs font-bold tracking-[0.16em] text-koi-500 uppercase">
              Month {card.month} · {month.name} · {month.kanji} {month.romaji}
            </p>
            <h3 className="mt-1.5 font-display text-2xl leading-tight font-black text-navy-900">{card.name}</h3>
            <p className="mt-1 text-sm text-slate-500">{month.plant}</p>

            <div className="mt-3 flex flex-wrap items-center gap-1.5">
              <TypeBadge type={card.type} />
              {card.ribbon !== "NONE" && <Pill tone="rose">{RIBBON_LABEL[card.ribbon]}</Pill>}
              {card.sakeCup && <Pill tone="emerald">Sake Cup · counts as +1 plain</Pill>}
            </div>

            <div className="mt-4 rounded-xl border border-slate-200 bg-slate-50/70 p-3 text-sm">
              <div className="flex flex-wrap items-center gap-2">
                <Pill tone="navy">{card.stableId}</Pill>
                <span className="text-slate-500">
                  numeric id <strong className="font-semibold text-navy-800">{card.id}</strong>
                </span>
              </div>
              <p className="mt-1.5 font-mono text-[12px] text-slate-500">
                ({card.month} − 1) × 4 + {card.index} = {card.id}
              </p>
            </div>

            <div className="mt-4">
              <p className="mb-2 text-xs font-bold tracking-wider text-slate-400 uppercase">What this card brings</p>
              <div className="flex flex-wrap gap-1.5">
                {yaku.map((yid) => {
                  const y = yakuById(yid);
                  return (
                    <Link
                      key={yid}
                      to={`yaku/yaku-${yid}`}
                      data-plain
                      onClick={onClose}
                      className="rounded-full border border-navy-200 bg-navy-50 px-2.5 py-1 text-[11px] font-semibold text-navy-700 transition hover:border-koi-300 hover:bg-koi-50 hover:text-koi-600"
                    >
                      {y.name} · {y.counting ? `${y.points}+` : y.points} pts
                    </Link>
                  );
                })}
              </div>
            </div>

            <p className="mt-4 flex items-start gap-2 text-[13px] leading-6 text-slate-500">
              <TrainFront className="mt-0.5 h-4 w-4 flex-none text-navy-400" />
              <span>{card.train}</span>
            </p>
            <p className="mt-2 text-[13px] leading-6 text-slate-500">{card.description}</p>

            <div className="mt-5">
              <p className="mb-2 text-xs font-bold tracking-wider text-slate-400 uppercase">
                Same month · {month.name}
              </p>
              <div className="grid grid-cols-4 gap-2">
                {cardsOfMonth(card.month).map((c) => (
                  <div key={c.stableId} className={c.stableId === id ? "rounded-lg ring-2 ring-koi-400" : ""}>
                    <CardThumb id={c.stableId} />
                  </div>
                ))}
              </div>
            </div>
          </div>
        </div>

        {/* Prev / next */}
        <div className="flex items-center justify-between gap-3 border-t border-slate-100 px-5 py-3.5 sm:px-7">
          <button
            onClick={() => go(-1)}
            className="flex min-w-0 items-center gap-1.5 rounded-lg px-2 py-1.5 text-sm font-semibold text-navy-700 transition hover:bg-navy-50"
          >
            <ChevronLeft className="h-4 w-4 flex-none" />
            <span className="truncate">{prev.stableId}</span>
          </button>
          <span className="hidden text-[11px] text-slate-400 sm:block">← → to browse · Esc to close</span>
          <button
            onClick={() => go(1)}
            className="flex min-w-0 items-center gap-1.5 rounded-lg px-2 py-1.5 text-sm font-semibold text-navy-700 transition hover:bg-navy-50"
          >
            <span className="truncate">{next.stableId}</span>
            <ChevronRight className="h-4 w-4 flex-none" />
          </button>
        </div>
      </div>
    </div>
  );
}

export function CardModalProvider({ children }: { children: ReactNode }) {
  const [openId, setOpenId] = useState<string | null>(null);
  const open = useCallback((stableId: string) => setOpenId(stableId), []);

  useEffect(() => {
    document.body.style.overflow = openId ? "hidden" : "";
    return () => {
      document.body.style.overflow = "";
    };
  }, [openId]);

  return (
    <CardModalContext.Provider value={{ open }}>
      {children}
      {openId && <Detail key={openId} id={openId} onClose={() => setOpenId(null)} />}
    </CardModalContext.Provider>
  );
}
