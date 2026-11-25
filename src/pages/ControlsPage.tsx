import { MousePointer2 } from "lucide-react";
import { Link } from "@/lib/router";
import { Callout, CodeBlock, H2, Kbd, PageHeader, Step, Steps, Table } from "@/components/ui";

export default function ControlsPage() {
  return (
    <div>
      <PageHeader
        group="Playing"
        level="Beginner"
        title="Controls"
        lead="Every mouse move and key you need – from the start menu to reading the match result like a regular."
      />

      <H2 id="first-move">From the start menu to your first move</H2>
      <Steps>
        <Step n={1} title="Press Play">
          <p>
            On the start menu, choose <strong>Play</strong>. The match settings appear.
          </p>
        </Step>
        <Step n={2} title="Set up the match">
          <p>
            Pick the <strong>Number of Rounds</strong> (1, 3, 6 or 12 – standard is 3) and the route (currently{" "}
            <strong>The City</strong>), then press <strong>Start</strong>. Details in{" "}
            <Link to="optionen">Menus & Options</Link>.
          </p>
        </Step>
        <Step n={3} title="Draw for the dealer">
          <p>
            Two face-down cards appear – choose one. Both reveal in sequence; the <strong>earlier month</strong>{" "}
            determines who deals (and plays) first.
          </p>
        </Step>
        <Step n={4} title="Watch the deal" last>
          <p>
            A one-second bullet-time beat plays: the Koidenscha logo spins above the table, then eight cards go
            to each hand and eight to the field. If you're the dealer, your first move is up.
          </p>
        </Step>
      </Steps>

      <H2 id="drag-drop">Playing a card: drag & drop</H2>
      <p>
        You move your cards <strong>by hand</strong> – literally. Press and hold a hand card, drag it over the
        field, and release:
      </p>
      <ul>
        <li>
          <strong>One matching field card:</strong> drop onto it – both cards fly to your capture pile.
        </li>
        <li>
          <strong>Two matching field cards:</strong> drop onto the one you want to take.
        </li>
        <li>
          <strong>No match:</strong> drop the card anywhere in the field – it stays there as a new target.
        </li>
        <li>
          <strong>Changed your mind?</strong> A far or invalid drop glides the card back to your hand.{" "}
          <strong>Right-click</strong> cancels a drag immediately.
        </li>
      </ul>
      <Callout tone="info" title="How precise do you have to be?">
        <p>
          Matching accepts overlap or an edge gap of up to <strong>10 pixels</strong> (in the 840 × 720
          table-plan coordinate system). And there's protection against mostly overlapping a{" "}
          <em>non-matching</em> card – you won't accidentally “miss” your target. With{" "}
          <strong>Card Hints</strong> on, every legal target is outlined while you drag.
        </p>
      </Callout>

      <H2 id="stock">The stock card</H2>
      <p>
        After your hand card resolves, the top <strong>stock card flips automatically</strong> and plays against
        the field by the same rules. If it has two possible matches, the game waits for you to{" "}
        <strong>click the desired field card</strong>. Unlike a hand choice, a stock choice cannot be cancelled –
        the card must land somewhere.
      </p>

      <H2 id="buttons">The Koi-Koi / Shobu buttons</H2>
      <p>
        When a yaku appears or grows on your side, two buttons appear: <strong>Koi-Koi</strong> (keep going) and{" "}
        <strong>Shobu</strong> (bank and end the round). Click one to decide. What they mean is covered in{" "}
        <Link to="grundlagen">Learn Koi-Koi</Link>; the exact multipliers live in{" "}
        <Link to="yaku">Yaku & Scoring</Link>.
      </p>

      <H2 id="inputs">All inputs at a glance</H2>
      <Table head={["Input", "Where", "What it does"]}>
        <tr>
          <td><MousePointer2 className="mr-1 inline h-3.5 w-3.5 text-navy-400" /> Left mouse, drag</td>
          <td>Table / pause menu</td>
          <td>Play a hand card; drag the volume meters.</td>
        </tr>
        <tr>
          <td>Left click</td>
          <td>Everywhere</td>
          <td>Choose a field card, press buttons, select menu entries and gallery cards.</td>
        </tr>
        <tr>
          <td>Right click</td>
          <td>Table / 3D viewer</td>
          <td>Cancel a drag; close the 3D card viewer.</td>
        </tr>
        <tr>
          <td><Kbd>Esc</Kbd> or <Kbd>P</Kbd></td>
          <td>Match</td>
          <td>Open the pause menu / resume the match.</td>
        </tr>
        <tr>
          <td><Kbd>↑</Kbd> <Kbd>↓</Kbd></td>
          <td>Pause menu / gallery</td>
          <td>Select an option row / change the month.</td>
        </tr>
        <tr>
          <td><Kbd>←</Kbd> <Kbd>→</Kbd></td>
          <td>Pause menu / gallery</td>
          <td>Adjust the selected option / change the card.</td>
        </tr>
        <tr>
          <td><Kbd>Enter</Kbd></td>
          <td>Pause menu / gallery</td>
          <td>Activate the selected row (Resume) / pop the big card out in 3D.</td>
        </tr>
        <tr>
          <td><Kbd>M</Kbd></td>
          <td>Radio panel</td>
          <td>Mute / unmute the radio.</td>
        </tr>
        <tr>
          <td>Mouse wheel</td>
          <td>3D card viewer</td>
          <td>Zoom the card between 0.6× and 3.0×.</td>
        </tr>
        <tr>
          <td>Close window</td>
          <td>Anywhere</td>
          <td>Exit the game (settings are saved on normal exit).</td>
        </tr>
      </Table>

      <H2 id="screen">What you see on screen</H2>
      <p>
        The table is a fixed logical canvas of <strong>1360 × 768</strong>; resizing the window preserves its
        aspect ratio inside a letterbox. The painted station-and-table image is the background, cards lie on a
        horizontal 3D plane under a perspective camera. Your eight-card hand sits above the bottom edge; a
        crowded field switches to a compact layout.
      </p>
      <ul>
        <li><strong>Centre:</strong> the field – the shared face-up cards.</li>
        <li><strong>Bottom:</strong> your hand; <strong>top:</strong> the CPU's face-down cards.</li>
        <li><strong>Sides:</strong> the capture piles and the face-down stock.</li>
        <li>
          <strong>HUD:</strong> the round counter (<code>ROUND n / total</code>), match totals and the action
          buttons. Round starts show a centered <code>MATCH START</code> / <code>ROUND START</code> cue.
        </li>
      </ul>

      <H2 id="turns">Whose turn is it?</H2>
      <p>
        You always perform your own card flight by dragging. CPU and stock cards use a slow{" "}
        <strong>600 ms automatic flight</strong>, followed by the same snappy <strong>150 ms</strong> table drop
        everyone uses. Matches freeze the action for a deliberate <strong>500 ms</strong> capture hit.
      </p>
      <p>
        A hand-card capture waits at its field contact point until the stock card has flipped and resolved –
        then <em>all</em> captures from both parts of the turn move to the pile together in 500 ms. That's why
        your cards sometimes “hover” for a moment before flying home.
      </p>

      <H2 id="result">Reading the round result</H2>
      <p>
        After a Shobu, the result panel breaks the award down. A line like this one tells the whole story:
      </p>
      <CodeBlock label="Result line" code={`Base 8  x2 (7+)  x2 (Opp Koi)  =  32 pts`} />
      <ul>
        <li><strong>Base 8</strong> – the sum of the winner's yaku (here: Shiko, four brights).</li>
        <li><strong>x2 (7+)</strong> – the base reached 7 points, so it doubles.</li>
        <li><strong>x2 (Opp Koi)</strong> – the opponent had called Koi-Koi, doubling the winner's award again.</li>
        <li>
          A <strong>Koi factor</strong> from the winner's own calls would appear as another multiplier (up to
          ×6). Factors of 1 are simply left out.
        </li>
      </ul>
      <p>
        The <strong>Round Ledger</strong> lists every round with its yaku and points for <em>You</em> and{" "}
        <em>CPU</em>. Yaku that were never banked show as <code>Unbanked - 0 pts awarded</code> – a Koi-Koi gamble
        that didn't pay off. Lucky opening wins appear as a flat 6 points.
      </p>
      <p>
        Between rounds, click <strong>Next Round</strong>. After the final round, <strong>Match Over</strong>{" "}
        starts a fresh match. The winner of a round deals the next one; a draw keeps the dealer.
      </p>

      <Callout tone="tip">
        <p>
          Ready for the numbers behind all of this? Continue to <Link to="yaku">Yaku & Scoring</Link> – the
          calculator there lets you replay any result line yourself.
        </p>
      </Callout>
    </div>
  );
}
