# mednafen_wswan Audio Defaults Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make `mednafen_wswan` default to lower-cost video and audio settings that reduce audio underruns on low-power devices.

**Architecture:** Keep the change entirely in the existing core-option override table. Add a focused source-level regression test that parses that table, then set four exact `default_value` fields without modifying shared SDL audio behavior.

**Tech Stack:** C option-override declarations, Python `unittest`, Git.

## Global Constraints

- Apply the defaults to `mednafen_wswan` on every supported platform.
- Do not change defaults for any other core.
- Existing per-core and per-game configuration files must continue to take precedence.
- Do not modify the frontend-wide audio buffer or shared SDL audio callback.

---

### Task 1: Set and verify the WonderSwan defaults

**Files:**
- Create: `tests/test_mednafen_wswan_defaults.py`
- Modify: `overrides/mednafen_wswan.h:3-42`

**Interfaces:**
- Consumes: `struct core_override_option.default_value` and the existing option keys in `overrides/mednafen_wswan.h`.
- Produces: Defaults `wswan_gfx_colors=16bit`, `wswan_frameskip=auto`, `wswan_60hz_mode=enabled`, and `wswan_sound_sample_rate=22050`.

- [ ] **Step 1: Write the failing source-level regression test**

```python
import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "overrides" / "mednafen_wswan.h"


class MednafenWswanDefaultsTest(unittest.TestCase):
    def test_audio_stability_defaults(self):
        text = SOURCE.read_text(encoding="utf-8")
        expected = {
            "wswan_gfx_colors": "16bit",
            "wswan_frameskip": "auto",
            "wswan_60hz_mode": "enabled",
            "wswan_sound_sample_rate": "22050",
        }

        for key, value in expected.items():
            entry = re.search(
                rf'\{{(?:(?!\n\t\}}).)*?\.key = "{re.escape(key)}"'
                rf'(?:(?!\n\t\}}).)*?\.default_value = "{re.escape(value)}"'
                rf'(?:(?!\n\t\}}).)*?\n\t\}}',
                text,
                re.DOTALL,
            )
            self.assertIsNotNone(entry, f"{key} should default to {value}")


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: Run the test and confirm the missing defaults fail**

Run:

```powershell
python -m unittest tests.test_mednafen_wswan_defaults -v
```

Expected: FAIL, reporting at least `wswan_gfx_colors should default to 16bit`.

- [ ] **Step 3: Add the four exact defaults**

Update the existing entries in `overrides/mednafen_wswan.h`:

```c
	{
		.key = "wswan_gfx_colors",
		.desc = "Color Depth",
		.info = "24-bit is slower and not available on all platforms. Restart required.",
		.default_value = "16bit",
		.options = {
			{ "16bit", "16-bit" },
			{ "24bit", "24-bit" },
		}
	},
```

```c
	{
		.key = "wswan_frameskip",
		.info = "Skip frames to avoid audio crackling. Improves performance at the expense of visual smoothness.",
		.default_value = "auto",
	},
```

Keep the existing `wswan_60hz_mode` entry with:

```c
		.default_value = "enabled"
```

Update the sample-rate entry:

```c
	{
		.key = "wswan_sound_sample_rate",
		.desc = "Sample Rate",
		.default_value = "22050",
	},
```

- [ ] **Step 4: Run focused and repository checks**

Run:

```powershell
python -m unittest tests.test_mednafen_wswan_defaults -v
python -m unittest discover -s tests -p "test_*.py" -v
git diff --check
```

Expected: all Python tests PASS and `git diff --check` prints no errors.

- [ ] **Step 5: Check build availability**

Run:

```powershell
make platform=h150101 -n
```

Expected: the dry run resolves the H150101 target. If the cross-toolchain is installed, follow with `make platform=h150101`; otherwise record device build verification as pending.

- [ ] **Step 6: Commit the implementation**

```powershell
git add tests/test_mednafen_wswan_defaults.py overrides/mednafen_wswan.h
git commit -m "stabilize wswan audio defaults"
```
