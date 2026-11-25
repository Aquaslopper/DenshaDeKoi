import { useState } from "react";
import { ArrowRight, Check, Lightbulb, RotateCcw, X } from "lucide-react";
import { Link } from "@/lib/router";
import { cardById } from "@/lib/cards";
import { CardThumb } from "@/components/CardModal";
import { Callout, H2, H3, PageHeader, Panel, Pill, Table, TypeBadge } from "@/components/ui";

const TYPE_PANELS = [
  {
    type: "hikari" as const,
    count: 5,
    minimum: "3 brights (without the Rainman) score Sanko",
    example: "01-B",
    text: "The showpieces: Crane, Curtain, Moon, Rainman and Phoenix. Few, but powerful.",
  },
  {
    type: "tane" as const,
    count: 9,
    minimum: "5 animals score Tane",
    example: "06-A",
    text: "Animals and objects: warbler, cuckoo, bridge, butterflies, boar, geese, sake cup, deer, swallow.",
  },
  {
    type: "tanzaku" as const,
    count: 10,
    minimum: "4 ribbons score Tanzaku",
    example: "03-RP",
    text: "The paper strips: three red poetry ribbons, three blue ribbons, four plain red ribbons.",
  },
  {
    type: "kasu" as const,
    count: 24,
    minimum: "10 plains score Kasu",
    example: "05-P1",
    text: "The plain plant cards – the quiet majority. Collecting them is slow but steady.",
  },
];

const QUIZ = [
  {
    q: "How many cards does a Hanafuda deck have?",
    options: ["36 cards", "48 cards", "52 cards"],
    answer: 1,
    explain: "12 months × 4 cards = 48. No jokers, no numbers.",
  },
  {
    q: "Which field card can you match with your hand card?",
    options: ["Any card with a similar picture", "Any card of the same month", "Any card of the same type"],
    answer: 1,
    explain: "Only the month matters. A March ribbon matches a March plain – the artwork is decoration.",
  },
  {
    q: "You captured three brights, without the Rainman. What do you have?",
    options: ["Sanko – 5 points", "Shiko – 8 points", "Nothing yet"],
    answer: 0,
    explain: "Three brights without the Rainman form Sanko (5 points). With the Rainman it would be nothing.",
  },
  {
    q: "What does calling Koi-Koi do?",
    options: ["It banks your points immediately", "It ends the round as a draw", "It keeps the round going so you can score more"],
    answer: 2,
    explain: "Koi-Koi declines banking and continues play. Each own call raises your award factor – but an opponent win afterwards doubles their award.",
  },
];

function Quiz() {
  const [answers, setAnswers] = useState<(number | null)[]>([null, null, null, null]);
  const answered = answers.filter((a) => a !== null).length;
  const correct = answers.filter((a, i) => a === QUIZ[i].answer).length;
  const done = answered === QUIZ.length;

  return (
    <div className="my-6 space-y-4">
      {QUIZ.map((item, qi) => {
        const picked = answers[qi];
        return (
          <Panel key={qi} className={picked !== null ? (picked === item.answer ? "border-emerald-200" : "border-rose-200") : ""}>
            <p className="mb-3 font-display text-[15px] font-bold text-navy-900">
              <span className="mr-2 text-koi-500">{qi + 1}.</span>
              {item.q}
            </p>
            <div className="grid gap-2 sm:grid-cols-3">
              {item.options.map((opt, oi) => {
                const isAnswer = oi === item.answer;
                const isPicked = picked === oi;
                let cls = "border-slate-200 bg-white text-slate-600 hover:border-navy-300 hover:bg-navy-50";
                if (picked !== null) {
                  if (isAnswer) cls = "border-emerald-300 bg-emerald-50 text-emerald-800";
                  else if (isPicked) cls = "border-rose-300 bg-rose-50 text-rose-700";
                  else cls = "border-slate-200 bg-white text-slate-400";
                }
                return (
                  <button
                    key={oi}
                    disabled={picked !== null}
                    onClick={() => setAnswers((prev) => prev.map((a, i) => (i === qi ? oi : a)))}
                    className={`rounded-xl border px-3 py-2.5 text-left text-sm font-medium transition disabled:cursor-default ${cls}`}
                  >
                    {opt}
                    {picked !== null && isAnswer && <Check className="ml-1.5 inline h-3.5 w-3.5 text-emerald-600" />}
                    {picked !== null && isPicked && !isAnswer && <X className="ml-1.5 inline h-3.5 w-3.5 text-rose-500" />}
                  </button>
                );
              })}
            </div>
            {picked !== null && (
              <p className={`mt-3 text-sm leading-6 ${picked === item.answer ? "text-emerald-700" : "text-slate-500"}`}>
                {picked === item.answer ? "Correct! " : "Not quite. "}
                {item.explain}
              </p>
            )}
          </Panel>
        );
      })}

      {done && (
        <div className="pop-in flex flex-wrap items-center justify-between gap-3 rounded-2xl border border-koi-200 bg-koi-50/70 p-5">
          <p className="font-display text-lg font-bold text-navy-900">
            You got {correct} / {QUIZ.length} right
            {correct === QUIZ.length ? " – ready for the table! 🎉" : " – scroll up and try again."}
          </p>
          <button
            onClick={() => setAnswers([null, null, null, null])}
            className="flex items-center gap-1.5 rounded-xl border border-koi-300 bg-white px-4 py-2 text-sm font-semibold text-koi-600 transition hover:bg-koi-100"
          >
            <RotateCcw className="h-4 w-4" /> Try again
          </button>
        </div>
      )}
    </div>
  );
}

function CardCaption({ id }: { id: string }) {
  const c = cardById(id)!;
  return (
    <div>
      <CardThumb id={id} />
      <p className="mt-1.5 text-center font-mono text-[11px] font-semibold text-navy-700">{id}</p>
      <p className="text-center text-[11px] leading-4 text-slate-500">{c.name}</p>
    </div>
  );
}

export default function BasicsPage() {
  return (
    <div>
      <PageHeader
        group="First steps"
        level="Beginner"
        title="Learn Koi-Koi"
        lead="Never played Hanafuda? Perfect. This page starts at zero: the deck, the goal, a full example turn – and a quiz at the end."
      />

      <H2 id="hanafuda">What is Hanafuda?</H2>
      <p>
        <strong>Hanafuda</strong> (花札, “flower cards”) is a traditional Japanese card deck. It has{" "}
        <strong>48 cards</strong>: twelve months, each with a plant and exactly four cards. There are no numbers
        and no suits in the Western sense – <strong>the month is the suit</strong>. Two cards match if and only
        if they belong to the same month.
      </p>
      <p>
        In <strong>Densha de Koi</strong>, every classic motif gets a railway twist: the crane of January flies
        beside a Yamanote-line train, the moon of August rises over a pixel-cloud skyline. The traditional
        identities stay the same – only the artwork rides the rails.
      </p>

      <H2 id="goal">The goal</H2>
      <p>
        Koi-Koi is a game for two – here, you against the CPU. You take turns playing cards onto the field and{" "}
        <strong>capturing</strong> matching pairs into your own pile. Captured cards form{" "}
        <strong>yaku</strong> (scoring combinations). The first player to bank a yaku with <strong>Shobu</strong>{" "}
        wins the round and its points. A standard match lasts <strong>three rounds</strong>; the highest total
        wins.
      </p>

      <H2 id="types">The four card types</H2>
      <p>Every card belongs to one of four types. The type decides which yaku it can build:</p>
      <div className="my-6 grid gap-4 sm:grid-cols-2">
        {TYPE_PANELS.map((t) => (
          <Panel key={t.type}>
            <div className="flex items-start justify-between gap-3">
              <div>
                <TypeBadge type={t.type} />
                <p className="mt-2.5 text-sm leading-6 text-slate-500">{t.text}</p>
              </div>
              <div className="w-16 flex-none">
                <CardThumb id={t.example} />
              </div>
            </div>
            <div className="mt-3 flex flex-wrap items-center gap-2 border-t border-slate-100 pt-3">
              <Pill tone="navy">{t.count} cards</Pill>
              <span className="text-xs text-slate-500">{t.minimum}</span>
            </div>
          </Panel>
        ))}
      </div>

      <Callout tone="tip" title="Check: 5 + 9 + 10 + 24 = 48">
        <p>
          Five brights, nine animals, ten ribbons and 24 plains make the full deck. One special case: the{" "}
          <strong>Sake Cup</strong> (09-A) is an animal, but it <em>also</em> counts as one extra plain – more on
          that in <Link to="yaku">Yaku & Scoring</Link>.
        </p>
      </Callout>

      <H2 id="months">Months are the suits</H2>
      <p>
        Each month has a plant. March, for example, is the cherry blossom (<em>sakura</em>) – and all four March
        cards match each other, regardless of type:
      </p>
      <div className="my-6 grid max-w-md grid-cols-4 gap-3">
        <CardCaption id="03-B" />
        <CardCaption id="03-RP" />
        <CardCaption id="03-P1" />
        <CardCaption id="03-P2" />
      </div>
      <p>
        The bright, the poetry ribbon and both plains are all “March”. You can find every month with its plant
        and kanji on the <Link to="karten">All 48 Cards</Link> page.
      </p>

      <H2 id="round-flow">How a round flows</H2>
      <H3>1 · Choosing the dealer</H3>
      <p>
        Before the first round, two face-down cards are offered. You pick one; both reveal in sequence and the{" "}
        <strong>earlier month determines the first dealer</strong>. The dealer acts first. In later rounds, the
        previous round's winner deals; a draw keeps the dealer.
      </p>
      <H3>2 · The deal</H3>
      <p>
        Eight cards go to your hand, eight to the CPU, and eight face-up onto the <strong>field</strong>. The
        remaining <strong>24 cards form the face-down stock</strong>. Each round opens with a one-second
        bullet-time beat: the logo spins, then everything is dealt.
      </p>
      <H3>3 · A turn in three steps</H3>
      <ol>
        <li>
          <strong>Play a hand card.</strong> Drag it onto a field card of the same month – or into the field if
          nothing matches.
        </li>
        <li>
          <strong>The stock card flips.</strong> It resolves automatically against the field, exactly like a hand
          card.
        </li>
        <li>
          <strong>Check your yaku.</strong> If a new combination appeared (or grew), you choose{" "}
          <strong>Koi-Koi</strong> or <strong>Shobu</strong>. Otherwise the turn passes on.
        </li>
      </ol>
      <Table head={["Field cards of your month", "What happens", "Cards you capture"]}>
        <tr>
          <td>0</td>
          <td>Your card stays on the field as a new target.</td>
          <td>0</td>
        </tr>
        <tr>
          <td>1</td>
          <td>Automatic match – both cards move to your pile.</td>
          <td>2</td>
        </tr>
        <tr>
          <td>2</td>
          <td>You choose which one to take.</td>
          <td>2</td>
        </tr>
        <tr>
          <td>3</td>
          <td>Jackpot – your card sweeps all three.</td>
          <td>4</td>
        </tr>
      </Table>

      <H2 id="example-turn">A turn, played out</H2>
      <p>
        Let's play one real turn. The field shows these four cards, and you hold the March poetry ribbon{" "}
        <code>03-RP</code>:
      </p>
      <div className="my-6 rounded-2xl border border-slate-200 bg-slate-50/60 p-5">
        <p className="mb-3 text-xs font-bold tracking-wider text-slate-400 uppercase">The field</p>
        <div className="grid max-w-md grid-cols-4 gap-3">
          <CardCaption id="03-P1" />
          <CardCaption id="07-A" />
          <CardCaption id="10-P1" />
          <CardCaption id="12-B" />
        </div>
        <div className="mt-5 flex flex-wrap items-center gap-4">
          <div>
            <p className="mb-2 text-xs font-bold tracking-wider text-slate-400 uppercase">Your hand card</p>
            <div className="w-20">
              <CardCaption id="03-RP" />
            </div>
          </div>
          <ArrowRight className="mt-6 h-6 w-6 flex-none text-koi-500" />
          <div>
            <p className="mb-2 text-xs font-bold tracking-wider text-slate-400 uppercase">You capture</p>
            <div className="flex gap-3">
              <div className="w-20">
                <CardCaption id="03-RP" />
              </div>
              <div className="w-20">
                <CardCaption id="03-P1" />
              </div>
            </div>
          </div>
        </div>
      </div>
      <ol>
        <li>
          You drag <code>03-RP</code> onto <code>03-P1</code> – both are March, so they match and move to your
          capture pile. The other three field cards are untouched.
        </li>
        <li>
          A stock card flips automatically. If it matches a field card, it captures too; with two possible
          matches you click the one you want. If it matches nothing, it joins the field.
        </li>
        <li>
          Two March cards are not a yaku yet – no decision appears, and the CPU takes its turn. (Had you
          captured the sake cup with the cherry curtain, <em>Hanami-zake</em> would have popped up right here.)
        </li>
      </ol>

      <H2 id="decision">Koi-Koi or Shobu?</H2>
      <p>
        Whenever a yaku appears or grows, the game asks the big question. Two buttons, two philosophies:
      </p>
      <div className="my-6 grid gap-4 sm:grid-cols-2">
        <Panel className="border-emerald-200">
          <p className="font-display text-lg font-bold text-emerald-700">Shobu · 勝負</p>
          <p className="mt-1 text-xs font-semibold tracking-wide text-emerald-600 uppercase">“Settle it!”</p>
          <p className="mt-3 text-sm leading-6 text-slate-600">
            Bank your current yaku and end the round. You score the base points, multiplied by the active
            factors. Safe, solid, done.
          </p>
        </Panel>
        <Panel className="border-koi-200">
          <p className="font-display text-lg font-bold text-koi-600">Koi-Koi · こいこい</p>
          <p className="mt-1 text-xs font-semibold tracking-wide text-koi-500 uppercase">“Come on, come on!”</p>
          <p className="mt-3 text-sm leading-6 text-slate-600">
            Keep playing and grow your score. Each of your own calls raises your award factor (up to ×6). The
            risk: if the opponent wins the round after your call, <strong>their award doubles</strong>.
          </p>
        </Panel>
      </div>
      <Callout tone="info">
        <p>
          After a Koi-Koi call, the decision only returns when a yaku is <strong>new or has grown</strong> –
          repeating the same score won't ask again. And if your hand runs empty right after scoring, the game
          banks automatically.
        </p>
      </Callout>

      <H2 id="round-end">How a round ends</H2>
      <ul>
        <li>
          <strong>Shobu:</strong> someone banks – the round is scored and over. The winner deals next.
        </li>
        <li>
          <strong>Both hands empty:</strong> nobody banked – the round is a <strong>0 : 0 draw</strong> and the
          dealer stays.
        </li>
        <li>
          <strong>Lucky hand:</strong> four cards of one month (<em>Teshi</em>) or four pairs (<em>Kuttsuki</em>)
          dealt into your hand win instantly for a flat <strong>6 points</strong>. Both players lucky? Draw.
        </li>
        <li>
          <strong>Misdeal:</strong> if the field shows all four cards of a month or four pairs, everything is
          redealt automatically.
        </li>
      </ul>

      <H2 id="tips">Five tips for your first match</H2>
      <div className="my-6 space-y-3">
        {[
          "Match by month, not by picture. The trains are gorgeous, but only the month decides what fits.",
          "Protect your brights. Crane, Curtain, Moon and Phoenix are your fastest route to 5+ points – three of them (without the Rainman) already score.",
          "Love the sake cup. It counts as an animal and an extra plain, and it completes both viewing yaku. Grab it whenever you can.",
          "Keep Card Hints on. The pause menu option outlines every legal target while you drag – perfect while learning.",
          "Respect the CPU's Shobu. It always banks at its first yaku. If you called Koi-Koi before that, its award doubles – so call greedily only when your hand is strong.",
        ].map((tip, i) => (
          <div key={i} className="flex gap-3 rounded-2xl border border-emerald-200 bg-emerald-50/50 p-4">
            <Lightbulb className="mt-0.5 h-4 w-4 flex-none text-emerald-600" />
            <p className="text-sm leading-6 text-slate-600">
              <strong className="font-semibold text-emerald-800">Tip {i + 1}.</strong> {tip}
            </p>
          </div>
        ))}
      </div>

      <Callout tone="warn" title="About the CPU">
        <p>
          According to the README, the current CPU uses a simple first-available-match strategy and{" "}
          <strong>always banks a yaku</strong> – it never calls Koi-Koi. Xeno-koi's full AI is not part of this
          game (yet).
        </p>
      </Callout>

      <H2 id="quiz">Quick quiz – four questions</H2>
      <p>No pressure: wrong answers come with an explanation, and you can retry as often as you like.</p>
      <Quiz />

      <p>
        Nicely done! Next station: <Link to="steuerung">Controls</Link> – or dive straight into{" "}
        <Link to="yaku">Yaku & Scoring</Link>.
      </p>
    </div>
  );
}
