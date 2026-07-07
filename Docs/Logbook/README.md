# GT-EV-Testbed Engineering Logbook

Chronological engineering record for the ME 8803 EV cart (Mecha kit + hoverboard drivetrain + FT85BD dual ESC).

## How this logbook was created

1. Photos were captured during build/test sessions and saved under [`../Images/`](../Images/).
2. Image filenames use **`MMDDYY-N.png`** (e.g. `70726-3` = 2026-07-07, image 3).
3. On **2026-07-07**, a narration video (`Docs/Notes/2026-07-07 16-58-03.mp4`) dictated what each picture shows. That video is **gitignored** (local only).
4. The narration was transcribed to [`_source/2026-07-07_video_transcript.txt`](_source/2026-07-07_video_transcript.txt) and converted into dated entries below.

## Using the logbook

| Resource | Purpose |
|----------|---------|
| [**INDEX.md**](INDEX.md) | Table of all sessions by date |
| **YYYY-MM-DD.md** | One file per work session with per-image entries |
| [`../TestLogs/`](../TestLogs/) | Structured test-run templates |
| [`../Notes/`](../Notes/) | Informal scratch notes |

## Entry format

Each image entry includes:

- **Image ID** matching the filename in `Docs/Images/`
- Embedded image preview
- **Supporting text** — cleaned engineering description from narration

## Sessions at a glance

See [**INDEX.md**](INDEX.md) for the full list (May 2026 → July 2026):

- Power harness and battery adapter work
- Motor phase / hall harness bring-up
- Mecha kit rear carrier mechanical design
- Battery 3D-print frame iterations
- First integrated rear-panel prototype
- UART telemetry and control-mode evaluation
- Duty-cycle low-speed tuning (July 2026)

## Regenerating entries

If the narration video is re-recorded or captions are corrected, edit `_generate_logbook.py` and run:

```bash
python Docs/Logbook/_generate_logbook.py
```

Or edit individual `YYYY-MM-DD.md` files directly for manual corrections.
