import { Link } from "@/lib/router";
import { PROFILE } from "@/lib/yaku";
import { Callout, CodeBlock, H2, PageHeader, Pill, Table } from "@/components/ui";

const CONSTANTS = [
  { name: "Id", value: PROFILE.id, meaning: "The profile name – this is not a runtime rules option." },
  { name: "Version", value: PROFILE.version, meaning: "Source version tag of the Xeno-koi rules." },
  { name: "Goko", value: String(PROFILE.goko), meaning: "Five brights." },
  { name: "Shiko", value: String(PROFILE.shiko), meaning: "Four brights without the Rainman." },
  { name: "AmeShiko", value: String(PROFILE.ameShiko), meaning: "Four brights with the Rainman." },
  { name: "Sanko", value: String(PROFILE.sanko), meaning: "Three brights without the Rainman." },
  { name: "FixedSet", value: String(PROFILE.fixedSet), meaning: "Hanami-zake, Tsukimi-zake, Ino-Shika-Chō, Akatan, Aotan." },
  { name: "AnimalThreshold", value: String(PROFILE.animalThreshold), meaning: "Tane scores from 5 animals." },
  { name: "RibbonThreshold", value: String(PROFILE.ribbonThreshold), meaning: "Tanzaku scores from 4 ribbons." },
  { name: "PlainThreshold", value: String(PROFILE.plainThreshold), meaning: "Kasu scores from 10 plain units." },
  { name: "SevenThreshold", value: String(PROFILE.sevenThreshold), meaning: "The ×2 award kicks in from 7 base points." },
  { name: "OpeningPoints", value: String(PROFILE.openingPoints), meaning: "Flat lucky-hand win (not multiplied)." },
  { name: "MaxCallerFactor", value: String(PROFILE.maxCallerFactor), meaning: "Cap for the own-calls factor." },
];

const TURN_TILES = [
  { n: 1, title: "Play a hand card", text: "Capture 0, 1, 2 (choose) or 3 field cards. An uncommitted hand choice can be cancelled." },
  { n: 2, title: "Reveal the stock card", text: "It resolves the same way – but a stock choice is mandatory and cannot be cancelled." },
  { n: 3, title: "Check yaku", text: "The engine snapshots all 13 yaku values and compares them with the player's last call." },
  { n: 4, title: "Decision (maybe)", text: "Only a new or grown individual yaku triggers Koi-Koi / Shobu. Empty hand after scoring → automatic Shobu." },
  { n: 5, title: "Next player / round end", text: "Otherwise the turn passes on. Both hands empty without a bank → 0 : 0 draw." },
];

export default function RulesPage() {
  return (
    <div>
      <PageHeader
        group="Reference"
        level="Advanced"
        title="Rules in Detail"
        lead="The exact KK-ADD-10 ruleset as implemented in the source code – constants, phases and edge cases, for the curious."
      />

      <H2 id="origin">Where these rules come from</H2>
      <p>
        DenshaDeKoi implements the <strong>KK-ADD-10</strong> rules from the sibling project{" "}
        <strong>Xeno-koi</strong>, pinned to commit{" "}
        <code>f422bacf2519cdfd1b7582a647015356478ce66b</code> (profile version <code>xeno-f422bac</code>). None of
        Xeno-koi's other selectable rulesets are included – there is exactly one profile, and it is not a
        runtime option.
      </p>

      <H2 id="constants">All constants at a glance</H2>
      <p>
        Every number below comes straight from <code>src/KoiKoiProfile.hpp</code>:
      </p>
      <Table head={["Constant", "Value", "Meaning"]}>
        {CONSTANTS.map((c) => (
          <tr key={c.name}>
            <td><code>{c.name}</code></td>
            <td><strong>{c.value}</strong></td>
            <td>{c.meaning}</td>
          </tr>
        ))}
      </Table>

      <H2 id="dealer">Choosing the dealer (Oya-gime)</H2>
      <p>
        Two distinct face-down cards are drawn. The comparison, in order:
      </p>
      <ol>
        <li><strong>Earlier month wins</strong> – January beats December.</li>
        <li>
          Same month? <strong>Type order decides</strong>: Hikari &lt; Tane &lt; Tanzaku &lt; Kasu (the register
          order – a bright beats an animal of the same month).
        </li>
        <li>Still tied (same month and type)? <strong>Redraw.</strong></li>
      </ol>
      <p>
        The dealer plays first. In later rounds the previous round's winner deals; a draw retains the dealer.
      </p>

      <H2 id="deal">The deal</H2>
      <p>
        One shuffled permutation of all 48 cards is split in order: <strong>8 to the human</strong>,{" "}
        <strong>8 to the opponent</strong>, <strong>8 to the field</strong>, the remaining <strong>24 to the
        stock</strong>. The stock is popped from the back, so the <em>last</em> card of the permutation is the
        first one revealed. Who deals affects only who plays first – not the deal order.
      </p>

      <H2 id="misdeal">Misdeals</H2>
      <p>
        A field showing <strong>all four cards of one month</strong> or <strong>four pairs</strong> (two cards
        each of four months) is a misdeal: the round is redealt. In the state machine the deal stays in{" "}
        <code>DEAL_CHECK</code> and <code>Advance()</code> reshuffles until the field is legal.
      </p>

      <H2 id="lucky">Lucky hands</H2>
      <ul>
        <li>
          <strong>Teshi</strong> (手四): all four cards of one month in your opening hand.
        </li>
        <li>
          <strong>Kuttsuki</strong> (くっつき): four pairs in your opening hand.
        </li>
      </ul>
      <p>
        Either wins the round immediately for a flat <strong>{PROFILE.openingPoints} points</strong> – opening
        wins are <em>not</em> multiplied. If both players are lucky, the round is a draw.
      </p>

      <H2 id="turn">Anatomy of a turn</H2>
      <div className="my-6 grid gap-3 sm:grid-cols-2 lg:grid-cols-3">
        {TURN_TILES.map((t) => (
          <div key={t.n} className="rounded-2xl border border-slate-200 bg-white p-4">
            <p className="flex items-center gap-2 font-display text-[15px] font-bold text-navy-900">
              <span className="flex h-6 w-6 flex-none items-center justify-center rounded-full bg-navy-800 text-[11px] font-bold text-white">{t.n}</span>
              {t.title}
            </p>
            <p className="mt-2 text-[13px] leading-6 text-slate-500">{t.text}</p>
          </div>
        ))}
      </div>

      <H2 id="yaku-details">Yaku details</H2>
      <p>
        The full yaku table with examples lives in <Link to="yaku">Yaku & Scoring</Link>. The engine specifics
        worth repeating here:
      </p>
      <ul>
        <li>The base is the <strong>sum of all formed yaku</strong> – nothing replaces anything else except the ribbon merge.</li>
        <li><strong>Ino-Shika-Chō stacks with Tane</strong>; its three animals also count toward the animal total.</li>
        <li>The <strong>sake cup</strong> counts as one animal <em>and</em> one phantom plain (<code>kasuUnits = physicalPlains + 1</code>).</li>
        <li><strong>Akatan + Aotan</strong> replaces the two individual ribbon yaku with a single 10-point yaku.</li>
        <li>Counting yaku score <code>max(0, count − (threshold − 1))</code> – one point per card beyond the threshold minus one.</li>
      </ul>

      <H2 id="decision">When the Koi-Koi question appears</H2>
      <p>
        After the mandatory hand and stock stages, a decision appears <strong>only for a new or grown
        individual yaku</strong> since that player's last Koi-Koi call – compared per yaku value, not just the
        total. Two special cases:
      </p>
      <ul>
        <li>
          A new or grown yaku with the <strong>current player's hand empty</strong> is automatically banked as
          Shobu – even when the other player still has a card left.
        </li>
        <li>
          <strong>Both hands empty</strong> without a bank: the round ends <strong>0 : 0</strong> (exhaustive
          draw).
        </li>
      </ul>

      <H2 id="settlement">Settlement</H2>
      <CodeBlock
        label="CalculateSettlement()"
        code={`award = base × (base >= 7 ? 2 : 1) × (opponentCalled ? 2 : 1) × min(6, 1 + ownCalls)`}
      />
      <p>
        The winner deals the next round; a draw retains the dealer. Match totals simply add the round awards –
        there is no match-end bonus in this game.
      </p>

      <H2 id="excluded">What is deliberately not included</H2>
      <div className="my-6 flex flex-wrap gap-2">
        <Pill tone="slate">No speed minigame</Pill>
        <Pill tone="slate">No match-end speed bonus</Pill>
        <Pill tone="slate">No advanced AI</Pill>
        <Pill tone="slate">No persistence / match history</Pill>
        <Pill tone="slate">mt19937, not xorshift32</Pill>
      </div>
      <p>
        Xeno-koi's speed minigame and its match-end minipoint conversion are intentionally excluded. The CPU
        here uses a simple first-available-match strategy and always banks. And the shuffle keeps{" "}
        <code>std::mt19937</code> (seeded from <code>std::random_device</code>) instead of Xeno-koi's xorshift32 –
        so seeds and replay sequences are <strong>not interchangeable</strong> between the two games.
      </p>

      <Callout tone="info">
        <p>
          Want to see the machinery itself – phases, methods, tests? Continue to{" "}
          <Link to="entwicklung">Development</Link>.
        </p>
      </Callout>
    </div>
  );
}
