import { Link } from "@/lib/router";
import { YAKU, type YakuDef, type YakuGroup } from "@/lib/yaku";
import { CardThumb } from "@/components/CardModal";
import { YakuCalculator } from "@/components/YakuCalculator";
import { Callout, CodeBlock, H2, PageHeader, Pill, Table } from "@/components/ui";

const GROUP_TONE: Record<YakuGroup, "amber" | "navy" | "rose" | "emerald"> = {
  brights: "amber",
  fixed: "navy",
  ribbons: "rose",
  counting: "emerald",
};

function YakuCard({ y }: { y: YakuDef }) {
  return (
    <div id={`yaku-${y.id}`} className="scroll-mt-24 rounded-2xl border border-slate-200 bg-white p-5 shadow-[0_1px_2px_rgba(16,28,64,0.04)]">
      <div className="flex flex-wrap items-center gap-x-2.5 gap-y-1">
        <h3 className="font-display text-lg font-bold text-navy-900">{y.name}</h3>
        <span className="text-sm text-slate-400">{y.kanji}</span>
        <Pill tone={GROUP_TONE[y.group]}>{y.counting ? `${y.points}+ pts` : `${y.points} pts`}</Pill>
      </div>
      <p className="mt-2 text-sm leading-6 text-slate-500">{y.description}</p>
      <div className="mt-3.5 flex flex-wrap gap-2">
        {y.cards.map((id) => (
          <div key={id} className="w-12">
            <CardThumb id={id} />
            <p className="mt-1 text-center font-mono text-[9px] text-slate-400">{id}</p>
          </div>
        ))}
      </div>
    </div>
  );
}

function YakuGroupGrid({ group }: { group: YakuGroup }) {
  return (
    <div className="my-6 grid gap-4 sm:grid-cols-2">
      {YAKU.filter((y) => y.group === group).map((y) => (
        <YakuCard key={y.id} y={y} />
      ))}
    </div>
  );
}

export default function YakuPage() {
  return (
    <div>
      <PageHeader
        group="Playing"
        level="Beginner"
        title="Yaku & Scoring"
        lead="Every winning combination and every point in the game – with an interactive calculator at the end."
      />

      <Callout tone="tip" title="Don't panic">
        <p>
          You don't need to memorize any of this to play. The game tells you whenever you score and offers the
          Koi-Koi / Shobu decision automatically. This page is for looking things up – and for the moment you
          wonder “why did that score 32?”.
        </p>
      </Callout>

      <H2 id="brights">Brights (Hikari) – the big four</H2>
      <p>
        The five brights build the strongest yaku. The only twist: the <strong>Rainman</strong> (11-B) is a
        party pooper for small groups – three brights <em>with</em> him score nothing.
      </p>
      <YakuGroupGrid group="brights" />

      <H2 id="fixed-sets">Fixed sets – three famous pairs & a trio</H2>
      <p>These combinations score as soon as you hold the listed cards – no counting needed.</p>
      <YakuGroupGrid group="fixed" />

      <H2 id="ribbons">Ribbons – poetry and blue</H2>
      <p>
        The three red poetry ribbons form <strong>Akatan</strong>, the three blue ribbons form{" "}
        <strong>Aotan</strong>. Hold both sets and they merge into one 10-point yaku.
      </p>
      <YakuGroupGrid group="ribbons" />

      <H2 id="counting">Counting yaku – the long game</H2>
      <p>
        These yaku start at a threshold and grow by <strong>+1 point per extra card</strong>. The thresholds in
        this ruleset: animals from <strong>5</strong>, ribbons from <strong>4</strong>, plains from{" "}
        <strong>10</strong>.
      </p>
      <YakuGroupGrid group="counting" />

      <H2 id="details">Important details</H2>
      <ul>
        <li>
          <strong>Everything adds up.</strong> Your base points are the sum of all formed yaku – brights, sets,
          ribbons and counting yaku together.
        </li>
        <li>
          <strong>Ino-Shika-Chō stacks with Tane.</strong> The boar, deer and butterflies score their 5-point set{" "}
          <em>and</em> count toward the animal total.
        </li>
        <li>
          <strong>The sake cup is double-booked.</strong> It counts as an animal <em>and</em> as one phantom
          plain – ten physical plains plus the cup already score Kasu 1.
        </li>
        <li>
          <strong>The Rainman rule.</strong> Three brights with 11-B: nothing. Four with him: Ame-Shiko (7). All
          five: Goko (10). Without him: Sanko (5) and Shiko (8).
        </li>
        <li>
          <strong>Akatan + Aotan replaces the singles.</strong> Both ribbon sets together are one 10-point yaku,
          not two 5-point yaku – which matters for what counts as “new” after a Koi-Koi call.
        </li>
        <li>
          <strong>After Koi-Koi, only news counts.</strong> The decision returns only when an individual yaku is
          new or has grown since your last call.
        </li>
      </ul>

      <H2 id="formula">The scoring formula</H2>
      <p>When a round is banked, the base points pass through three multipliers:</p>
      <CodeBlock label="Settlement" code={`award = base × 7+ factor × opponent factor × Koi factor`} />
      <Table head={["Factor", "When it applies", "Value"]}>
        <tr>
          <td><strong>7+ factor</strong></td>
          <td>Base points are 7 or more</td>
          <td>×2 (otherwise ×1)</td>
        </tr>
        <tr>
          <td><strong>Opponent factor</strong></td>
          <td>The opponent called Koi-Koi during the round</td>
          <td>×2 (otherwise ×1)</td>
        </tr>
        <tr>
          <td><strong>Koi factor</strong></td>
          <td>Your own Koi-Koi calls</td>
          <td>×1 … ×6 (see below)</td>
        </tr>
      </Table>
      <Table head={["Your Koi-Koi calls", "0", "1", "2", "3", "4", "5 or more"]}>
        <tr>
          <td><strong>Koi factor</strong></td>
          <td>×1</td>
          <td>×2</td>
          <td>×3</td>
          <td>×4</td>
          <td>×5</td>
          <td>×6 (cap)</td>
        </tr>
      </Table>

      <H2 id="examples">Three worked examples</H2>
      <div className="my-6 space-y-3">
        <div className="rounded-2xl border border-slate-200 bg-slate-50/60 p-4">
          <p className="text-sm font-semibold text-navy-900">Sanko, nobody called anything</p>
          <p className="mt-1 font-mono text-[13px] text-slate-600">5 × 1 × 1 × 1 = <strong className="text-navy-900">5 pts</strong></p>
        </div>
        <div className="rounded-2xl border border-slate-200 bg-slate-50/60 p-4">
          <p className="text-sm font-semibold text-navy-900">Shiko (8 base) – the 7+ rule kicks in</p>
          <p className="mt-1 font-mono text-[13px] text-slate-600">8 × 2 × 1 × 1 = <strong className="text-navy-900">16 pts</strong></p>
        </div>
        <div className="rounded-2xl border border-slate-200 bg-slate-50/60 p-4">
          <p className="text-sm font-semibold text-navy-900">A base of 10 (the Akatan + Aotan value) with one own Koi-Koi call</p>
          <p className="mt-1 font-mono text-[13px] text-slate-600">10 × 2 × 1 × 2 = <strong className="text-navy-900">40 pts</strong></p>
          <p className="mt-1.5 text-xs text-slate-400">
            In a real hand the six ribbons also add Tanzaku +3 on top – try it in the calculator below.
          </p>
        </div>
      </div>

      <H2 id="rechner">Yaku calculator</H2>
      <p>
        Click cards to capture them, load an example, add Koi-Koi calls – the result line updates exactly like
        the game's <code>ScoreYaku()</code> and <code>CalculateSettlement()</code>.
      </p>
      <YakuCalculator />

      <p>
        Want the full picture – thresholds, misdeals, lucky hands? Continue to{" "}
        <Link to="regelwerk">Rules in Detail</Link>.
      </p>
    </div>
  );
}
