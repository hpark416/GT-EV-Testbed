#!/usr/bin/env python3
"""One-off generator for engineering logbook entries from video dictation."""
from pathlib import Path

ROOT = Path(__file__).parent

SESSIONS = [
    {
        "file": "2026-05-22.md",
        "title": "2026-05-22 — Stock battery connector and power distribution",
        "summary": "Makeshift bullet-connector adapter from the stock Segway battery to XT60 anti-spark output; bus bar layout planning.",
        "entries": [
            ("52226-1", "Stock battery connector adapter", "Connector fabricated by de-shelling an existing bullet-type connector and adapting it to a 7W2 gold-plated high-current sub-adapter."),
            ("52226-2", "Adapter prep for soldering", "Connector staged for soldering the makeshift adapter to the stock battery connector."),
            ("52226-3", "Power bus bars", "Bus bars that will distribute power from the main battery to subsystems."),
            ("52226-4", "Bus bar tap detail", "Close-up of a bus bar with a connector tapping power for downstream subsystems."),
            ("52226-5", "Soldered stock connector", "Close-up of the soldered makeshift adapter on the stock connector."),
            ("52226-6", "Adapter process note", "Interim adapter when correct parts were unavailable; documents soldering and heat-shrink insulation steps to follow."),
            ("52226-7", "Solder fill on adapter", "Soldering to lock the makeshift connector to the stock connector."),
            ("52226-8", "Layout estimate", "Approximate path: battery adapter → 120 A breaker → distribution (layout not final)."),
            ("52226-9", "XT60 anti-spark output", "Tap terminates in XT60; battery rear labeled +/−."),
        ],
    },
    {
        "file": "2026-05-24.md",
        "title": "2026-05-24 — Motor phase harnesses and first ESC power-on",
        "summary": "Anderson pole motor leads, crimp quality checks, first dual-ESC power-up, hoverboard hub inspection and hall wiring.",
        "entries": [
            ("52426-1", "Connector assortment", "XT60, bus bar lugs, Anderson PP connectors gathered for harness work."),
            ("52426-2", "Motor lead strip length", "Motor wires stripped ~2 mm before Anderson crimping."),
            ("52426-3", "Anderson crimp tool", "Crimp tool for Anderson pole terminals."),
            ("52426-4", "Crimped leads with housings", "Crimped leads and housed assemblies; keep phase color mapping consistent."),
            ("52426-5", "Good crimp reference", "Target crimp quality; verify continuity and low resistance after every crimp."),
            ("52426-6", "Completed motor harnesses", "Both motor phase harnesses with Anderson housings."),
            ("52426-7", "First system power-on", "Green alive LED; LED1/LED2 show both motor channels up."),
            ("52426-8", "Hoverboard hub wheel", "Wheel out of chassis; record diameter and sidewall markings."),
            ("52426-9", "ESC-to-hub connection", "Brown/blue/yellow phases for initial spin test."),
            ("52426-10", "Hall sensor harness", "Black=GND, red=V+, blue/green/yellow = hall A/B/C."),
            ("52426-11", "Wheel interior marking", "Manufacturing ID: `30EFC19C1A0297`."),
            ("52426-12", "Wheel sidewall marking", "Marking `E484425xw60-350` recorded."),
        ],
    },
    {
        "file": "2026-05-29.md",
        "title": "2026-05-29 — Preliminary control with Mega and hall calibration",
        "summary": "Arduino Mega + analog joystick PPM bench rig; hall sensors wired for motor identification.",
        "entries": [
            ("52926-1", "Joystick PPM bench rig", "Mega feeds PPM to FT85BD; battery powers ESC."),
            ("52926-2", "Motor + hall wiring", "Phase and hall connected for calibration."),
            ("52926-3", "Hall connector close-up", "Custom bullet connectors on working hoverboard — primary test platform."),
        ],
    },
    {
        "file": "2026-05-30.md",
        "title": "2026-05-30 — Stock hoverboard teardown vs test rig",
        "summary": "Compare stock assembly to stripped test configuration; RC receiver replaces joystick.",
        "entries": [
            ("53026-1", "Stock vs test assembly", "Stock hoverboard with current-sense sandwich vs stripped test rig."),
            ("53026-2", "Gutted internals", "Stock PCBs removed for space and weight."),
            ("53026-3", "RC receiver on Mega", "Radiolink receiver replaces analog joystick."),
        ],
    },
    {
        "file": "2026-06-03.md",
        "title": "2026-06-03 — Harness faults: crimp and cold solder",
        "summary": "Intermittent calibration traced to bad green-wire crimp and cold solder on motor lead extensions.",
        "entries": [
            ("60326-1", "Bad crimp discovery", "Green wire poor contact caused intermittent tuning — continuity-check all crimps."),
            ("60326-2", "Two hoverboards compared", "One board later found with damaged hall hardware from prior lab use."),
            ("60326-3", "Loaded test rig", "Battery connected for motor runs."),
            ("60326-4", "Multimeter continuity", "Use continuity mode; target near 0 Ω."),
            ("60326-5", "Problem green wire", "Crimp may have grabbed insulation not copper."),
            ("60326-6", "Motor lead extension", "High-temp solder + rosin on thick stranded stock leads."),
            ("60326-7", "Cold solder joint", "Rough unsoldered patches — unreliable connection."),
            ("60326-8", "Rosin paste rework", "Flux before re-flow."),
            ("60326-9", "Re-flowed joint", "Completed joint after rework."),
        ],
    },
    {
        "file": "2026-06-04.md",
        "title": "2026-06-04 — Chassis integration and wire routing",
        "summary": "Wheels mounted under Mecha kit; electronics placement and accessory lighting notes.",
        "entries": [
            ("60426-1", "Wheels mounted under chassis", "Loaded test: route leads and plan electronics mount."),
            ("60426-2", "Lead routing options", "Harness length allows front or rear exit."),
            ("60426-3", "Full electronics stack", "Battery reposition; ESC ambient brake light; headlight/horn untested."),
        ],
    },
    {
        "file": "2026-06-10.md",
        "title": "2026-06-10 — Mecha kit rear carrier layout and prototyping",
        "summary": "M6 auxiliary holes, seat removal, foam-board and MDF/laser-cut carrier concepts, battery orientation, wire clamping.",
        "entries": [
            ("61026-1", "Auxiliary mount holes", "Pop-out holes on Mecha kit back expose M6 bolt mounts for add-on plates."),
            ("61026-2", "Accessing auxiliary holes", "Flush cutters remove plastic plugs behind seat."),
            ("61026-3", "Hardware procurement", "Home Depot M6 bolts, washers — multiple lengths for plywood carrier trials."),
            ("61026-4", "Low flat battery orientation", "Lower CoG desired — cart tips forward and scrapes frame under heavy riders."),
            ("61026-5", "Seat mounting bolts", "Four bolt locations under foam seat covers."),
            ("61026-6", "Seat removed", "Measurements for rear electronics CAD carrier."),
            ("61026-7", "M6 bolts in auxiliary holes", "Note lip/clearance for CAD."),
            ("61026-8", "Foam board layout template", "Initial laser-cut / sheet-metal carrier concept."),
            ("61026-9", "Rear clearance constraint", "Minimize rear overhang for Coda storage racks."),
            ("61026-10", "Preferred battery orientation", "Flat, rear, visible charge indicator, plug not crushed by seat."),
            ("61026-11", "Battery extension check", "Ideation — not final."),
            ("61026-12", "Base board polygon", "Triangular carrier with power-button access cutout."),
            ("61026-13", "Foam board measurements", "Bolt spacing and clearances marked."),
            ("61026-14", "Foam board reverse side", "Opposite face layout."),
            ("61026-15", "Electronics volume study", "Battery + ESC + wire bulk on limited rear area."),
            ("61026-16", "Motor leads to rear", "Hoverboard harness routed aft; needs strain relief."),
            ("61026-17", "Hook-and-loop clamp attempt", "3M Velcro failed on cast metal — need new clamp method."),
            ("61026-18", "Reused hoverboard clamp", "Stock bracket + tape grips harness adequately."),
            ("61026-19", "Harness breakout", "Split harness for easier routing."),
            ("61026-20", "Full foam-board prototype", "Carrier concept with all major components."),
            ("61026-21", "MDF laser-cut minimal carrier", "MDF trial — prefer plywood or sheet metal (MDF moisture sensitive)."),
            ("61026-22", "Vertical battery clearance", "Seat-to-battery spacing when mounted vertically."),
            ("61026-23", "Vertical mount perspective", "Alternate view of vertical battery clearance."),
            ("61026-24", "Baseboard not flush", "Gap between MDF and Mecha shell — bolts compress plastic over time."),
            ("61026-25", "2×4 quick mount prototype", "Temporary wood blocks for test mounting."),
            ("61026-26", "2×4 battery support concept", "Wood sandwich structure ideation."),
            ("61026-27", "2×4 lip sandwich", "Sandwich lip on battery — woodworking not scalable for fleet upgrade; sheet metal preferred."),
        ],
    },
    {
        "file": "2026-06-12.md",
        "title": "2026-06-12 — Battery CAD, 3D-printed frame iterations",
        "summary": "Fusion 360 battery model, split 3D prints, alignment fixes for stock power port.",
        "entries": [
            ("61226-1", "Battery photo for CAD", "Ruler reference for Fusion 360 hole layout."),
            ("61226-2", "Fusion 360 calibration", "Image scaled; parallax limits accuracy."),
            ("61226-3", "Hole and plug layout concept", "Battery mount holes and power port cutout check."),
            ("61226-4", "Split frame for print bed", "Two-part print — printer too small for one piece."),
            ("61226-5", "Print orientation v1", "Alternate view of split frame."),
            ("61226-6", "Bambu slicer prep", "~1.5 h per part."),
            ("61226-7", "MDF + printed insert concept", "Iterative tolerance on power plug opening."),
            ("61226-8", "Reference image mesh", "Assembly spacing study — parallax limited usefulness."),
            ("61226-9", "First print complete", "Missing correct screws for stock plug mount."),
            ("61226-10", "Alignment issues v1", "Battery corner holes misaligned; plug off-center."),
            ("61226-11", "Plug attaches to print", "Harness works but frame dimensions wrong."),
            ("61226-12", "Protruding screws", "Wrong screw length — still secures plug for alignment test."),
            ("61226-13", "Forced fit analysis", "Right side pops off; center plug aligns — lengthen frame + fix holes."),
            ("61226-14", "Custom bullet power tap", "Small BMS pins unused — Segway app data not needed."),
        ],
    },
    {
        "file": "2026-06-13.md",
        "title": "2026-06-13 — Frame interlock and ESC CAD mock-up",
        "summary": "Two-part battery frame fit issues; Fusion 360 ESC envelope for rear assembly.",
        "entries": [
            ("61326-1", "Split frame length short", "Halves lack overlap length — add interlocking fingers."),
            ("61326-2", "ESC photo for CAD", "Reference capture for ESC mock-up model."),
            ("61326-3", "ESC CAD mock-up", "Heights, clearances, and lead wire bandwidth for full assembly."),
        ],
    },
    {
        "file": "2026-06-14.md",
        "title": "2026-06-14 — Battery frame v2 alignment",
        "summary": "Updated 3D-printed battery frame; minor gap between halves accepted for now.",
        "entries": [
            ("61426-1", "Frame v2 overview", "Updated alignment; small gap between halves — forced fit for now."),
            ("61426-2", "Plug fit close-up", "Stock plug seats correctly in printed frame."),
        ],
    },
    {
        "file": "2026-06-20.md",
        "title": "2026-06-20 — Battery support brackets and ESC placement",
        "summary": "Carbon-fiber PLA vertical battery supports; CAD full assembly and seat angle measurements.",
        "entries": [
            ("62026-1", "Battery support prototypes", "Slot-in supports for fore/aft battery position trials (CF-PLA, wood-screwed to plywood)."),
            ("62026-2", "Supports side view", "Battery seated in printed adapters."),
            ("62026-3", "Support performance", "Small prints proved surprisingly rigid under battery mass."),
            ("62026-4", "ESC placement ideation", "Compact stack: ESC on battery back, Arduino to the side."),
            ("62026-5", "Seat angle scratch measurements", "Seat back ~67.8° from horizontal — battery must sit below this."),
            ("62026-6", "Full CAD assembly", "Visualized target rear packaging."),
        ],
    },
    {
        "file": "2026-06-22.md",
        "title": "2026-06-22 — First rear-panel integrated prototype",
        "summary": "Plywood carrier with ESC, Mega, RC receiver; ready for first rider-on tests.",
        "entries": [
            ("62226-1", "Electronics on plywood base", "Wood-screw mounted ESC, Mega, Radiolink RX; hall wires connected."),
            ("62226-2", "Battery plugged in", "Plug clearance from seat back — ready for loaded drive tests."),
        ],
    },
    {
        "file": "2026-06-30_gap.md",
        "title": "2026-06-22 → 2026-06-30 — Progress gap and software transition",
        "summary": "Family emergency and exams paused hardware work; informal firmware iteration moved toward GitHub.",
        "entries": [],
        "narrative": """## Context (no images)

Hardware progress paused roughly **2026-06-22 through 2026-06-30** due to family emergency and exams. Firmware work continued but was not well versioned.

- Initial drive tests used simple **PPM** control.
- Planned migration to **UART telemetry** for battery voltage, currents, RPM, and time-series logging.
- Repository consolidation on **GitHub** began to track software properly.""",
    },
    {
        "file": "2026-07-01.md",
        "title": "2026-07-01 — UART bring-up and logic-level discovery",
        "summary": "Organized engineering logs; UART telemetry first success; 3.3 V ESC logic vs 5 V Mega mistake.",
        "entries": [
            ("70126-1", "Engineering log organization", "Pictures uploaded; dictation workflow for GitHub-linked documentation."),
            ("70126-2", "UART work begins", "Attempt to read battery voltage from cart over UART."),
            ("70126-3", "3.3 V logic discovery", "FT85BD UART is 3.3 V — Mega 5 V connection was a wiring mistake; damage assessment needed."),
            ("70126-4", "Oscilloscope probe", "NI myDAQ ~3.3 V stable on ESC TX — initial sign of life."),
            ("70126-5", "Dual ESC telemetry", "Master 101 and slave 102 report matching pack voltage, RPM, current, and fault codes."),
            ("70126-6", "Telemetry under PPM load", "Under PPM throttle: ERPM, battery current, motor current visible; no temperature or encoder angle from hall-only setup."),
            ("70126-7", "Flipsky control setup screenshot", "Throttle response level 2; signal ramp pos/neg; input type set UART (later found UART read-only on hobby FT85BD — PPM still required for drive)."),
        ],
    },
    {
        "file": "2026-07-03.md",
        "title": "2026-07-03 — Flipsky tool tuning and motor fault discovery",
        "summary": "Hall startup, ERPM threshold 400, current vs differential instability; motor 1 hall fault on lab-tested hoverboard.",
        "entries": [
            ("70326-1", "VESC tool comparison", "Original VESC tool vs simplified Flipsky package — far more options on VESC variant."),
            ("70326-2", "Motor startup settings", "Hall start; direction set in tool not firmware; zero-duty keep-output (brake not coast)."),
            ("70326-3", "Observer ERPM threshold 400", "Motors struggle below ~400 ERPM in speed mode testing."),
            ("70326-4", "Max current limits", "60 A/motor bench — adequate hill climb with 200 lb payload but **current mode unstable** for differential drive."),
            ("70326-5", "Control setup page", "Throttle response L2, deadband, ramp times; **PPM required** — UART command not viable on hobby FT85BD."),
            ("70326-6", "Auto-calibrate fault", "Motor 1: hall sensor code **6**, startup IPD not Hall — damaged hall on lab-tested hoverboard."),
        ],
    },
    {
        "file": "2026-07-07.md",
        "title": "2026-07-07 — Duty-cycle mode, shudder debug, SD logging prep",
        "summary": "Duty control smooth at low ERPM; current mode spin-outs; speed mode buzz; diagnostic/logger firmware and repo updates.",
        "entries": [
            ("70726-1", "Alternate battery research", "48 V 20 Ah pack specs (AI-assisted) vs stock Segway ~15 A BMS limit — conjecture."),
            ("70726-2", "Product page reference", "Electrical parameters for Flipsky battery setup."),
            ("70726-3", "CAD spacer request", "Requested Mecha kit current-sensor spacer CAD from course staff to skip re-measure."),
            ("70726-4", "EV Grid summer project files", "Footprint templates for ESC and MCU rear-panel integration."),
        ],
        "narrative": """## Session notes (duty-cycle evaluation)

Switched primary tuning focus to **duty-cycle control**:

| Mode | Observation |
|------|-------------|
| **Current** | Strong low-speed torque but **unstable differential drive** — spin-outs with rider; wiped out multiple times. |
| **Speed** | Stable differential behavior but **buzz/hum and hang-up below ~400 ERPM**. |
| **Duty cycle** | **Smooth low-ERPM crawl** (~400 range) without speed-mode lock-up; current priority for rider safety. |

### Debug checklist (in progress)

- Battery sag and stock BMS trip points
- Logic-level noise on dual UART
- Metal screw shorts through plywood mounts
- Whether higher-discharge pack needed for duty mode

### Firmware / repo

- Added diagnostic tools and **UART telemetry SD logger** (`UartTelemetryLogger_NanoESP32`)
- Steering gain at low RPM still weak in `UartSpeedController_NanoESP32` — on TODO list""",
    },
]


def render_session(sess: dict) -> str:
    lines = [f"# {sess['title']}", "", sess["summary"], "", "---", ""]
    if sess.get("narrative"):
        lines.append(sess["narrative"])
        lines.append("")
    for img, title, body in sess.get("entries", []):
        lines += [
            f"## {img} — {title}",
            "",
            f"![{img}](../Images/{img}.png)",
            "",
            body,
            "",
        ]
    return "\n".join(lines)


def main():
    index_rows = []
    for sess in SESSIONS:
        path = ROOT / sess["file"]
        path.write_text(render_session(sess), encoding="utf-8")
        date = sess["file"].replace(".md", "")
        count = len(sess.get("entries", []))
        index_rows.append((date, sess["title"], count, sess["file"]))
        print(f"wrote {sess['file']} ({count} images)")

    idx_lines = [
        "# Engineering Logbook Index",
        "",
        "Dated entries transcribed from video dictation (`Docs/Notes/2026-07-07 16-58-03.mp4`, gitignored).",
        "Source transcript: [`_source/2026-07-07_video_transcript.txt`](_source/2026-07-07_video_transcript.txt)",
        "",
        "| Date | Session | Images | File |",
        "|------|---------|--------|------|",
    ]
    for date, title, count, fname in index_rows:
        short = title.split(" — ", 1)[-1] if " — " in title else title
        idx_lines.append(f"| {date} | {short} | {count} | [{fname}]({fname}) |")

    (ROOT / "INDEX.md").write_text("\n".join(idx_lines) + "\n", encoding="utf-8")
    print("wrote INDEX.md")


if __name__ == "__main__":
    main()
