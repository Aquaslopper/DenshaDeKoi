import { ArrowRight, Calculator, Cpu, Download, Gamepad2, GraduationCap, Layers, MapPin, Radio, Sparkles, TrainFront } from "lucide-react";
import hero from "@/assets/hero.jpg";
import { Link } from "@/lib/router";
import { PAGES } from "@/lib/pages";
import { cardById } from "@/lib/cards";
import { CardThumb } from "@/components/CardModal";
import { Callout, H2, LevelBadge, Pill, TypeBadge } from "@/components/ui";

const STATS = [
  { value: "48", label: "cards in the deck" },
  { value: "3", label: "rounds per standard match" },
  { value: "1", label: "CPU opponent" },
  { value: "C++20", label: "with Raylib 5.5" },
];

const PATH = [
  { n: 1, label: "Install", to: "installation" },
  { n: 2, label: "Learn Koi-Koi", to: "grundlagen" },
  { n: 3, label: "Controls", to: "steuerung" },
  { n: 4, label: "Yaku", to: "yaku" },
  { n: 5, label: "Let's play!", to: "steuerung" },
];

const TERMS = [
  { term: "Koi-Koi", text: "“Come on, come on!” – keep playing instead of banking your points. Greedy, but exciting.", to: "glossar/term-koi-koi" },
  { term: "Shobu", text: "“Settle it!” – bank your current combinations and end the round safely.", to: "glossar/term-shobu" },
  { term: "Yaku", text: "A scoring combination of captured cards – the only way to earn points.", to: "glossar/term-yaku" },
  { term: "Hanafuda", text: "The 48-card Japanese deck with 12 months. No numbers – you match by month.", to: "glossar/term-hanafuda" },
];

const FEATURES = [
  { icon: GraduationCap, title: "Learn from zero", text: "A complete beginner course with a real example turn and a mini quiz.", to: "grundlagen" },
  { icon: Layers, title: "All 48 cards", text: "Every railway-themed card with its month, type and train motif.", to: "karten" },
  { icon: Calculator, title: "Yaku calculator", text: "Click cards together and watch the exact game scoring happen.", to: "yaku/rechner" },
  { icon: Gamepad2, title: "Controls & menus", text: "Every key, every menu, every option – explained step by step.", to: "steuerung" },
  { icon: Radio, title: "Gallery, radio & sound", text: "The 3D card viewer, six J-Pop stations and the sound bank.", to: "extras" },
  { icon: MapPin, title: "Rules deep-dive", text: "The exact KK-ADD-10 ruleset, straight from the source code.", to: "regelwerk" },
];

const EXAMPLE_CARDS = ["01-B", "06-A", "03-RP", "05-P1"];

export default function StartPage() {
  return (
    <div>
      {/* Hero */}
      <div className="mb-8">
        <span className="mb-4 inline-flex items-center gap-1.5 rounded-full border border-koi-200 bg-koi-50 px-3 py-1 text-xs font-bold text-koi-600">
          <Sparkles className="h-3.5 w-3.5" /> A wiki for beginners
        </span>
        <h1 className="font-display text-4xl leading-tight font-black tracking-tight text-navy-900 sm:text-5xl">
          Welcome to <span className="text-koi-500">Densha de Koi</span>
        </h1>
        <p className="mt-4 max-w-2xl text-lg leading-8 text-slate-500">
          The friendly guide to the train-themed Koi-Koi game. No Hanafuda knowledge needed – we start at zero,
          explain every Japanese term, and get you ready for your first match.
        </p>
        <div className="mt-6 flex flex-wrap gap-3">
          <Link
            to="grundlagen"
            data-plain
            className="inline-flex items-center gap-2 rounded-xl bg-navy-900 px-5 py-3 text-sm font-semibold text-white shadow-sm transition hover:bg-navy-700"
          >
            <GraduationCap className="h-4 w-4" /> Learn Koi-Koi
          </Link>
          <Link
            to="installation"
            data-plain
            className="inline-flex items-center gap-2 rounded-xl border border-slate-200 bg-white px-5 py-3 text-sm font-semibold text-navy-800 transition hover:border-navy-300 hover:bg-navy-50"
          >
            <Download className="h-4 w-4" /> Install the game
          </Link>
        </div>
      </div>

      <figure className="mb-10">
        <img
          src={hero}
          alt="Densha de Koi title screen: the logo above a mountain lake, cherry blossoms and a train at Yamazakura station"
          className="w-full rounded-3xl border border-slate-200 shadow-sm"
        />
        <figcaption className="mt-3 text-center text-[13px] leading-6 text-slate-400">
          Densha de Koi (電車で恋) – “Love on the train”. The game's slogan:{" "}
          <span className="font-medium text-slate-500">鉄道で、恋を集めに行こう。</span> – “Let's ride the rails and collect love.”
        </figcaption>
      </figure>

      {/* Stats */}
      <div className="mb-4 grid grid-cols-2 gap-3 sm:grid-cols-4">
        {STATS.map((s) => (
          <div key={s.label} className="rounded-2xl border border-slate-200 bg-slate-50/60 p-4 text-center">
            <p className="font-display text-2xl font-black text-navy-900">{s.value}</p>
            <p className="mt-1 text-xs leading-5 text-slate-500">{s.label}</p>
          </div>
        ))}
      </div>

      <H2 id="about">What is Densha de Koi?</H2>
      <p>
        <strong>Densha de Koi</strong> is a desktop game for Windows, macOS and Linux, written in C++20 with the
        Raylib 5.5 library. It plays <strong>Koi-Koi</strong>, the classic Japanese card game for two players –
        but every one of the 48 Hanafuda cards has been reimagined with a railway motif: cranes share the sky
        with Yamanote trains, and the moon rises over pixel-cloud skylines.
      </p>
      <p>
        You play a match (three rounds by default) against a CPU opponent at a fixed table beside a train
        station. The rules follow the <strong>KK-ADD-10</strong> ruleset from the sibling project Xeno-koi:
        combinations add up, the sake cup is flexible, and big scores get multiplied.
      </p>

      <Callout tone="pink" title="One sentence to take away">
        <p>
          Match cards of the <strong>same month</strong>, collect winning combinations (<strong>yaku</strong>),
          and after each score decide: bank it with <strong>Shobu</strong> – or push your luck with{" "}
          <strong>Koi-Koi</strong>.
        </p>
      </Callout>

      <H2 id="learning-path">Your learning path – five stations</H2>
      <p>Follow the line from left to right. Each station builds on the previous one.</p>
      <div className="relative my-8">
        <span className="absolute top-[18px] right-4 left-4 hidden h-0.5 bg-slate-200 sm:block" aria-hidden />
        <ol className="grid gap-4 sm:grid-cols-5">
          {PATH.map((s) => (
            <li key={s.n} className="relative">
              <Link to={s.to} data-plain className="group flex items-center gap-3 sm:flex-col sm:gap-2 sm:text-center">
                <span className="relative z-10 flex h-9 w-9 flex-none items-center justify-center rounded-full border-2 border-koi-400 bg-white font-display text-sm font-black text-koi-500 transition group-hover:bg-koi-500 group-hover:text-white">
                  {s.n}
                </span>
                <span className="text-sm font-semibold text-navy-800 transition group-hover:text-koi-600">{s.label}</span>
              </Link>
            </li>
          ))}
        </ol>
      </div>

      <H2 id="terms">Four terms you'll meet everywhere</H2>
      <div className="grid gap-3 sm:grid-cols-2">
        {TERMS.map((t) => (
          <Link
            key={t.term}
            to={t.to}
            data-plain
            className="group rounded-2xl border border-slate-200 bg-white p-5 transition hover:border-koi-300 hover:shadow-md"
          >
            <p className="font-display text-lg font-bold text-navy-900 transition group-hover:text-koi-600">{t.term}</p>
            <p className="mt-1.5 text-sm leading-6 text-slate-500">{t.text}</p>
          </Link>
        ))}
      </div>

      <H2 id="example-cards">Meet four of the 48 cards</H2>
      <p>
        A bright, an animal, a ribbon and a plain – the four card types you'll collect. Click any card to open
        its details.
      </p>
      <div className="my-6 grid grid-cols-2 gap-4 sm:grid-cols-4">
        {EXAMPLE_CARDS.map((id) => {
          const c = cardById(id)!;
          return (
            <div key={id}>
              <CardThumb id={id} />
              <p className="mt-2 text-center font-mono text-xs font-semibold text-navy-700">{id}</p>
              <p className="mt-0.5 text-center text-xs leading-5 text-slate-500">{c.name}</p>
              <div className="mt-1.5 flex justify-center">
                <TypeBadge type={c.type} compact />
              </div>
            </div>
          );
        })}
      </div>

      <H2 id="features">What's inside this wiki</H2>
      <div className="grid gap-3 sm:grid-cols-2 lg:grid-cols-3">
        {FEATURES.map((f) => (
          <Link
            key={f.title}
            to={f.to}
            data-plain
            className="group rounded-2xl border border-slate-200 bg-white p-5 transition hover:border-navy-300 hover:shadow-md"
          >
            <span className="mb-3 flex h-10 w-10 items-center justify-center rounded-xl bg-navy-50 text-navy-600 transition group-hover:bg-koi-50 group-hover:text-koi-500">
              <f.icon className="h-5 w-5" />
            </span>
            <p className="font-display text-[15px] font-bold text-navy-900">{f.title}</p>
            <p className="mt-1 text-[13px] leading-6 text-slate-500">{f.text}</p>
          </Link>
        ))}
      </div>

      <Callout tone="warn" title="Good to know">
        <p>
          According to the README, the CPU currently uses a simple first-available-match strategy and always
          banks a yaku – it never calls Koi-Koi. That makes it a forgiving first opponent.{" "}
          <Cpu className="inline h-4 w-4 text-amber-600" />
        </p>
      </Callout>

      <H2 id="all-pages">All pages at a glance</H2>
      <div className="grid gap-3 sm:grid-cols-2">
        {PAGES.map((p) => (
          <Link
            key={p.id}
            to={p.id}
            data-plain
            className="group flex items-center gap-3.5 rounded-2xl border border-slate-200 bg-white p-4 transition hover:border-navy-300 hover:shadow-md"
          >
            <span className="flex h-10 w-10 flex-none items-center justify-center rounded-xl bg-navy-50 text-navy-600 transition group-hover:bg-koi-50 group-hover:text-koi-500">
              <p.icon className="h-5 w-5" />
            </span>
            <span className="min-w-0 flex-1">
              <span className="flex items-center gap-2">
                <span className="truncate font-display text-[15px] font-bold text-navy-900">{p.nav}</span>
                <ArrowRight className="h-3.5 w-3.5 flex-none text-slate-300 transition group-hover:translate-x-0.5 group-hover:text-koi-500" />
              </span>
              <span className="mt-0.5 block text-xs text-slate-400">{p.group}</span>
            </span>
            <LevelBadge level={p.level} />
          </Link>
        ))}
      </div>

      <div className="mt-10 flex items-center justify-center gap-2 rounded-2xl border border-slate-200 bg-slate-50/60 p-4 text-sm text-slate-500">
        <TrainFront className="h-4 w-4 text-navy-400" />
        Ready? Start at <Link to="installation">Installation</Link> – or jump straight into{" "}
        <Link to="grundlagen">Learn Koi-Koi</Link>.
        <Pill tone="koi">次の駅</Pill>
      </div>
    </div>
  );
}
