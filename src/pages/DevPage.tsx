import { FlaskConical, GitBranch, ShieldAlert } from "lucide-react";
import { Link } from "@/lib/router";
import { REPO_URL } from "@/lib/cards";
import { Callout, CodeBlock, H2, PageHeader, Pill, Table } from "@/components/ui";

const PHASES = [
  "DEAL_CHECK",
  "AWAIT_HAND_PLAY",
  "AWAIT_HAND_CHOICE",
  "RESOLVE_HAND",
  "REVEAL_DRAW",
  "AWAIT_DRAW_CHOICE",
  "RESOLVE_DRAW",
  "CHECK_YAKU",
  "AWAIT_DECISION",
  "NEXT_PLAYER",
  "ROUND_END",
];

export default function DevPage() {
  return (
    <div>
      <PageHeader
        group="Reference"
        level="Developer"
        title="Development"
        lead="How the code is organized, how to test it headlessly, and which areas are protected by the product contract."
      />

      <H2 id="targets">Three build targets</H2>
      <Table head={["Target", "What it is"]}>
        <tr>
          <td><code>hanafuda_core</code></td>
          <td>A pure C++20 static library with <strong>no Raylib dependency</strong>: card database, deck, rules engine and match state machine.</td>
        </tr>
        <tr>
          <td><code>denshadekoi</code></td>
          <td>The game itself – Raylib rendering, 3D table, menus, radio and sound.</td>
        </tr>
        <tr>
          <td><code>denshadekoi_inspector</code></td>
          <td>The atlas inspector: months 1–12 as columns, card indices 0–3 as rows, hover identifiers. Accepts an alternative atlas path as its first argument.</td>
        </tr>
      </Table>

      <H2 id="structure">Project structure</H2>
      <Table head={["Path", "Contents"]}>
        <tr><td><code>src/CardData.hpp</code></td><td>Card and ribbon types.</td></tr>
        <tr><td><code>src/DeckRegister.hpp</code></td><td>The single source of all 48 cards (ids, types, names).</td></tr>
        <tr><td><code>src/Deck.*</code></td><td>Reset, shuffle, draw; <code>std::mt19937</code> seeded from <code>std::random_device</code>.</td></tr>
        <tr><td><code>src/KoiKoiRules.*</code></td><td>Dealer comparison, dealing, misdeals, captures, yaku scoring, settlement.</td></tr>
        <tr><td><code>src/KoiKoiStateMachine.*</code></td><td>One round as a phase machine.</td></tr>
        <tr><td><code>src/TableGame.*</code></td><td>Match controller: three-round totals, dealer flow, deal motion.</td></tr>
        <tr><td><code>src/TableScene.*</code>, <code>TableLayout.hpp</code></td><td>3D table, camera calibration, hit testing, fixed 1360 × 768 canvas.</td></tr>
        <tr><td><code>src/ui/</code></td><td>Start, pause, radio, gallery, match-settings and match-result menus.</td></tr>
        <tr><td><code>assets/</code></td><td>Card atlas, fonts, optional SFX, menu textures.</td></tr>
        <tr><td><code>card-gallery/</code></td><td>The 48 original-resolution JPGs plus metadata (CSV / JSON).</td></tr>
        <tr><td><code>tests/</code></td><td>Headless tests with Raylib test doubles.</td></tr>
        <tr><td><code>.github/workflows/</code></td><td>Build CI and the protected-areas guard.</td></tr>
      </Table>

      <H2 id="database">The card database</H2>
      <p>
        <code>DeckRegister.hpp</code> is the canonical month-major register: numeric id{" "}
        <code>(month − 1) × 4 + cardIndex</code>, stable ids like <code>01-B</code> and <code>09-A</code>. The
        September sake cup is a Tane flagged with <code>isSakeCup</code>.{" "}
        <code>Deck::ValidateCardDatabase()</code> is a headless invariant check for tests or a debugger.
      </p>
      <ul>
        <li><code>Reset()</code> restores month/row order <strong>without reseeding</strong>.</li>
        <li><code>Shuffle()</code> rearranges only undrawn cards.</li>
        <li><code>Draw()</code> returns an empty optional when the deck is exhausted.</li>
        <li>A seed can be supplied for repeatable tests.</li>
      </ul>

      <H2 id="state-machine">The state machine</H2>
      <p>
        <code>KoiKoiStateMachine</code> holds exactly one round. Automatic phases advance via{" "}
        <code>Advance()</code>; input phases take explicit calls – wrong-phase or invalid actions throw.
      </p>
      <Table head={["Method", "Purpose"]}>
        <tr><td><code>TryStartDeal(permutation)</code></td><td>Deterministic deal for tests/replays; a rejected field stays in <code>DEAL_CHECK</code> for reshuffling.</td></tr>
        <tr><td><code>Advance()</code></td><td>Advance one automatic phase (throws in decision/input phases).</td></tr>
        <tr><td><code>PlayHandCard(card)</code></td><td>Play a card from the current hand.</td></tr>
        <tr><td><code>ChooseFieldCard(target)</code></td><td>Resolve a two-match choice.</td></tr>
        <tr><td><code>CancelHandChoice()</code></td><td>Cancel an uncommitted hand choice – stock choices are mandatory.</td></tr>
        <tr><td><code>ChooseDecision(KOI_KOI | SHOUBU)</code></td><td>Answer the Koi-Koi / Shobu decision.</td></tr>
        <tr><td><code>StartNextRound()</code></td><td>Reset round state with the result's next dealer.</td></tr>
        <tr><td><code>KoiCalls(player)</code></td><td>The round's call count per player.</td></tr>
        <tr><td><code>ValidateConservation()</code></td><td>Checks all 48 physical cards, including a revealed pending stock card.</td></tr>
      </Table>
      <p className="mt-4">The eleven phases, in order:</p>
      <div className="mt-2 flex flex-wrap gap-1.5">
        {PHASES.map((p) => (
          <Pill key={p} tone="navy">{p}</Pill>
        ))}
      </div>

      <H2 id="tests">Tests</H2>
      <p>
        Rules, deck, table geometry, render commands, pause options, atlas, asset lookup and the inspector are
        all verified <strong>without Raylib, a window system or network access</strong> – the table and
        inspector tests use Raylib test doubles:
      </p>
      <CodeBlock
        label="Headless test run"
        code={`cmake -S . -B build-headless -DBUILD_DESKTOP_APP=OFF -DBUILD_TESTING=ON\ncmake --build build-headless\nctest --test-dir build-headless --output-on-failure`}
      />
      <p>
        When Python 3 is available, CTest also checks the black frame pixels on every card using only the
        Python standard library <FlaskConical className="inline h-4 w-4 text-navy-400" />.
      </p>

      <H2 id="ci">Continuous integration</H2>
      <p>
        <code>.github/workflows/build.yml</code> builds and tests every push to <code>main</code> and every pull
        request on Windows with <strong>both MSVC and MinGW-w64</strong>, and uploads the staged game as the
        artifacts <code>denshadekoi-windows-msvc</code> and <code>denshadekoi-windows-mingw</code>{" "}
        <GitBranch className="inline h-4 w-4 text-navy-400" />.
      </p>

      <H2 id="mitarbeit">Contributing & protected areas</H2>
      <p>
        Contributions outside the protected areas are welcome. The locked product contract (see{" "}
        <code>.github/PROTECTED_AREAS.md</code>) covers:
      </p>
      <ul>
        <li><strong>Game & Koi-Koi rules</strong> – dealing, captures, scoring, turns, decisions.</li>
        <li><strong>Card identities & design</strong> – the register, atlas, artwork, logo and fonts.</li>
        <li><strong>Table layout & camera</strong> – positions, projection, hit testing, presentation.</li>
        <li><strong>Menu designs & shared UI premises</strong> – each menu's visual identity.</li>
        <li><strong>The guardrails themselves</strong> – including <code>CMakeLists.txt</code> with its contract check.</li>
      </ul>
      <p>
        Every build verifies SHA-256 fingerprints of the approved files before compiling; a mismatch stops the
        build with <code>PROTECTED CONTRACT CHANGED</code>. The approval procedure{" "}
        <ShieldAlert className="inline h-4 w-4 text-amber-600" />:
      </p>
      <ol>
        <li>The owner states exactly what protected behavior may change.</li>
        <li>The pull request lists every protected file and the approved reason.</li>
        <li>The owner applies the <code>protected-code-approved</code> label and approves the PR.</li>
        <li>Required tests and protection checks pass before merge.</li>
      </ol>
      <Callout tone="warn" title="For automated agents">
        <p>
          Agents may never infer approval from unrelated requests, apply the label themselves, or weaken the
          guardrails. General requests like “improve the game” are not approval. This wiki, by the way, is
          purely additive – it touches no protected file.
        </p>
      </Callout>

      <H2 id="thanks">Acknowledgments</H2>
      <ul>
        <li><strong>Xeno-koi</strong> – the source of the KK-ADD-10 ruleset (commit <code>f422bac</code>).</li>
        <li><strong>Raylib 5.5</strong> – rendering, audio and input.</li>
        <li><strong>Asia DREAM Radio</strong> – the six J-Pop streams.</li>
        <li><strong>dr_mp3</strong> – the vendored MP3 decoder behind the radio.</li>
        <li><strong>The project author</strong> – game, card artwork and the railway reinterpretation of all 48 motifs.</li>
      </ul>

      <p className="mt-8">
        Everything lives in the public{" "}
        <a href={REPO_URL} target="_blank" rel="noreferrer">GitHub repository</a>. New to the game itself? Start
        at <Link to="grundlagen">Learn Koi-Koi</Link>.
      </p>
    </div>
  );
}
