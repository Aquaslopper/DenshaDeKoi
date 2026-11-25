import { useState, type ReactNode } from "react";
import { AlertTriangle, Check, ChevronDown, Copy, Info, Lightbulb, Sparkles } from "lucide-react";
import { TYPE_INFO, type CardType } from "@/lib/cards";
import type { Level } from "@/lib/pages";

/* ---------- Headings ---------- */

export function H2({ id, children }: { id: string; children: ReactNode }) {
  return (
    <h2
      id={id}
      data-toc=""
      className="mt-12 mb-4 scroll-mt-24 font-display text-2xl font-bold tracking-tight text-navy-900 first:mt-0"
    >
      {children}
    </h2>
  );
}

export function H3({ children }: { children: ReactNode }) {
  return <h3 className="mt-8 mb-3 font-display text-lg font-bold text-navy-800">{children}</h3>;
}

/* ---------- Page header ---------- */

export function LevelBadge({ level }: { level: Level }) {
  const styles: Record<Level, string> = {
    Beginner: "border-emerald-200 bg-emerald-50 text-emerald-700",
    Advanced: "border-amber-200 bg-amber-50 text-amber-700",
    Developer: "border-navy-200 bg-navy-50 text-navy-700",
  };
  return (
    <span className={`rounded-full border px-2.5 py-0.5 text-[11px] font-semibold ${styles[level]}`}>
      {level}
    </span>
  );
}

export function PageHeader({
  group,
  level,
  title,
  lead,
}: {
  group: string;
  level: Level;
  title: string;
  lead: string;
}) {
  return (
    <div className="mb-10">
      <div className="mb-3 flex flex-wrap items-center gap-2.5">
        <span className="text-xs font-bold uppercase tracking-[0.18em] text-koi-500">{group}</span>
        <LevelBadge level={level} />
      </div>
      <h1 className="font-display text-4xl leading-tight font-black tracking-tight text-navy-900 sm:text-[2.75rem]">
        {title}
      </h1>
      <p className="mt-4 text-lg leading-8 text-slate-500">{lead}</p>
    </div>
  );
}

/* ---------- Callout ---------- */

type CalloutTone = "info" | "tip" | "warn" | "pink";

const CALLOUT_STYLES: Record<CalloutTone, { box: string; icon: string; Icon: typeof Info }> = {
  info: { box: "border-navy-200 bg-navy-50/70", icon: "text-navy-500", Icon: Info },
  tip: { box: "border-emerald-200 bg-emerald-50/70", icon: "text-emerald-600", Icon: Lightbulb },
  warn: { box: "border-amber-200 bg-amber-50/70", icon: "text-amber-600", Icon: AlertTriangle },
  pink: { box: "border-koi-200 bg-koi-50/70", icon: "text-koi-500", Icon: Sparkles },
};

export function Callout({
  tone = "info",
  title,
  children,
}: {
  tone?: CalloutTone;
  title?: string;
  children: ReactNode;
}) {
  const s = CALLOUT_STYLES[tone];
  return (
    <div className={`my-6 rounded-2xl border p-4 sm:p-5 ${s.box}`}>
      <div className="flex gap-3">
        <s.Icon className={`mt-0.5 h-5 w-5 flex-none ${s.icon}`} />
        <div className="min-w-0 flex-1 text-[15px] leading-7 text-slate-600">
          {title && <p className="mb-1 font-display text-[15px] font-bold text-navy-900">{title}</p>}
          <div className="[&>p]:mb-2 [&>p:last-child]:mb-0 [&_a]:font-medium [&_a]:text-navy-600 [&_a]:underline [&_a]:decoration-navy-200 [&_a]:underline-offset-2 [&_a:hover]:text-koi-600 [&_code]:rounded-md [&_code]:bg-white/70 [&_code]:px-1.5 [&_code]:py-0.5 [&_code]:font-mono [&_code]:text-[0.85em] [&_code]:text-navy-800">
            {children}
          </div>
        </div>
      </div>
    </div>
  );
}

/* ---------- Small bits ---------- */

export function Kbd({ children }: { children: ReactNode }) {
  return (
    <kbd className="rounded-md border border-slate-200 bg-slate-50 px-1.5 py-0.5 font-mono text-[11px] font-semibold text-slate-600 shadow-[0_1px_0_#e2e8f0]">
      {children}
    </kbd>
  );
}

export const TYPE_BADGE_STYLES: Record<CardType, string> = {
  hikari: "border-amber-200 bg-amber-50 text-amber-700",
  tane: "border-emerald-200 bg-emerald-50 text-emerald-700",
  tanzaku: "border-rose-200 bg-rose-50 text-rose-700",
  kasu: "border-slate-200 bg-slate-100 text-slate-600",
};

export function TypeBadge({ type, compact = false }: { type: CardType; compact?: boolean }) {
  const info = TYPE_INFO[type];
  return (
    <span
      className={`inline-flex items-center gap-1 rounded-full border px-2.5 py-0.5 text-[11px] font-semibold whitespace-nowrap ${TYPE_BADGE_STYLES[type]}`}
    >
      {info.label}
      {!compact && <span className="font-normal opacity-75">· {info.english}</span>}
    </span>
  );
}

type PillTone = "navy" | "koi" | "amber" | "emerald" | "rose" | "slate";

const PILL_STYLES: Record<PillTone, string> = {
  navy: "border-navy-200 bg-navy-50 text-navy-700",
  koi: "border-koi-200 bg-koi-50 text-koi-600",
  amber: "border-amber-200 bg-amber-50 text-amber-700",
  emerald: "border-emerald-200 bg-emerald-50 text-emerald-700",
  rose: "border-rose-200 bg-rose-50 text-rose-700",
  slate: "border-slate-200 bg-slate-100 text-slate-600",
};

export function Pill({ children, tone = "slate" }: { children: ReactNode; tone?: PillTone }) {
  return (
    <span className={`inline-flex items-center rounded-full border px-2.5 py-0.5 text-[11px] font-semibold whitespace-nowrap ${PILL_STYLES[tone]}`}>
      {children}
    </span>
  );
}

/* ---------- Code block with copy ---------- */

export function CodeBlock({ code, label }: { code: string; label?: string }) {
  const [copied, setCopied] = useState(false);

  const copy = async () => {
    try {
      await navigator.clipboard.writeText(code);
    } catch {
      const ta = document.createElement("textarea");
      ta.value = code;
      document.body.appendChild(ta);
      ta.select();
      document.execCommand("copy");
      ta.remove();
    }
    setCopied(true);
    window.setTimeout(() => setCopied(false), 1600);
  };

  return (
    <div className="relative my-5 overflow-hidden rounded-xl bg-navy-900 shadow-sm">
      {label && (
        <div className="border-b border-white/10 px-4 py-2 pr-12 text-[11px] font-semibold tracking-wider text-slate-400 uppercase">
          {label}
        </div>
      )}
      <pre className="overflow-x-auto p-4 font-mono text-[13px] leading-6 text-slate-100">
        <code>{code}</code>
      </pre>
      <button
        onClick={copy}
        aria-label={copied ? "Copied" : "Copy to clipboard"}
        className="absolute top-2 right-2 flex h-8 w-8 items-center justify-center rounded-lg bg-white/10 text-slate-300 transition hover:bg-white/20 hover:text-white"
      >
        {copied ? <Check className="h-4 w-4 text-emerald-400" /> : <Copy className="h-4 w-4" />}
      </button>
    </div>
  );
}

/* ---------- Steps ---------- */

export function Steps({ children }: { children: ReactNode }) {
  return <div className="my-6">{children}</div>;
}

export function Step({
  n,
  title,
  last = false,
  children,
}: {
  n: number;
  title: string;
  last?: boolean;
  children: ReactNode;
}) {
  return (
    <div className="flex gap-4">
      <div className="flex flex-col items-center">
        <span className="flex h-8 w-8 flex-none items-center justify-center rounded-full bg-navy-800 font-display text-sm font-bold text-white">
          {n}
        </span>
        {!last && <span className="w-px flex-1 bg-slate-200" />}
      </div>
      <div className={`min-w-0 flex-1 ${last ? "" : "pb-8"}`}>
        <p className="mb-2 font-display text-[16px] font-bold text-navy-900">{title}</p>
        <div className="text-[15px] leading-7 text-slate-600 [&>p]:mb-2">{children}</div>
      </div>
    </div>
  );
}

/* ---------- Table ---------- */

export function Table({ head, children }: { head: ReactNode[]; children: ReactNode }) {
  return (
    <div className="my-6 overflow-x-auto rounded-2xl border border-slate-200">
      <table className="w-full border-collapse text-left text-sm">
        <thead>
          <tr className="bg-navy-50/70">
            {head.map((h, i) => (
              <th key={i} className="px-4 py-3 font-display text-[12px] font-bold tracking-wide text-navy-800 uppercase">
                {h}
              </th>
            ))}
          </tr>
        </thead>
        <tbody className="[&_td]:px-4 [&_td]:py-3 [&_td]:align-top [&_td]:leading-6 [&_td]:text-slate-600 [&_tr]:border-t [&_tr]:border-slate-100">
          {children}
        </tbody>
      </table>
    </div>
  );
}

/* ---------- Accordion ---------- */

export function AccordionItem({
  id,
  question,
  open,
  onToggle,
  children,
}: {
  id: string;
  question: string;
  open: boolean;
  onToggle: () => void;
  children: ReactNode;
}) {
  return (
    <div id={id} className="scroll-mt-24 overflow-hidden rounded-2xl border border-slate-200 bg-white">
      <button
        onClick={onToggle}
        aria-expanded={open}
        className="flex w-full items-center justify-between gap-3 px-5 py-4 text-left transition hover:bg-slate-50/70"
      >
        <span className="font-display text-[15px] font-bold text-navy-900">{question}</span>
        <ChevronDown
          className={`h-4 w-4 flex-none text-slate-400 transition-transform duration-200 ${open ? "rotate-180" : ""}`}
        />
      </button>
      {open && <div className="border-t border-slate-100 px-5 py-4 text-[15px] leading-7 text-slate-600 [&>p]:mb-2 [&>p:last-child]:mb-0 [&_a]:font-medium [&_a]:text-navy-600 [&_a]:underline [&_a]:decoration-navy-200 [&_a]:underline-offset-2 [&_a:hover]:text-koi-600 [&_code]:rounded-md [&_code]:bg-slate-100 [&_code]:px-1.5 [&_code]:py-0.5 [&_code]:font-mono [&_code]:text-[0.85em] [&_code]:text-navy-800">{children}</div>}
    </div>
  );
}

/* ---------- Panel ---------- */

export function Panel({
  title,
  icon,
  children,
  className = "",
}: {
  title?: string;
  icon?: ReactNode;
  children: ReactNode;
  className?: string;
}) {
  return (
    <div className={`rounded-2xl border border-slate-200 bg-white p-5 shadow-[0_1px_2px_rgba(16,28,64,0.04)] ${className}`}>
      {title && (
        <p className="mb-3 flex items-center gap-2 font-display text-[15px] font-bold text-navy-900">
          {icon}
          {title}
        </p>
      )}
      {children}
    </div>
  );
}

/* ---------- Tabs ---------- */

export function Tabs({ tabs }: { tabs: { id: string; label: string; content: ReactNode }[] }) {
  const [active, setActive] = useState(tabs[0]?.id);
  const current = tabs.find((t) => t.id === active) ?? tabs[0];
  return (
    <div className="my-6">
      <div className="flex flex-wrap gap-1 rounded-xl border border-slate-200 bg-slate-50 p-1">
        {tabs.map((t) => (
          <button
            key={t.id}
            onClick={() => setActive(t.id)}
            className={`rounded-lg px-4 py-2 text-sm font-semibold transition ${
              t.id === current?.id
                ? "bg-white text-navy-900 shadow-sm"
                : "text-slate-500 hover:text-navy-700"
            }`}
          >
            {t.label}
          </button>
        ))}
      </div>
      <div className="mt-4 rounded-2xl border border-slate-200 bg-white p-5 text-[15px] leading-7 text-slate-600 [&>p]:mb-3 [&>p:last-child]:mb-0 [&_code]:rounded-md [&_code]:bg-slate-100 [&_code]:px-1.5 [&_code]:py-0.5 [&_code]:font-mono [&_code]:text-[0.85em] [&_code]:text-navy-800 [&_ul]:space-y-1.5 [&_ul]:pl-5 [&_ul]:list-disc [&_ul]:marker:text-navy-300">
        {current?.content}
      </div>
    </div>
  );
}
