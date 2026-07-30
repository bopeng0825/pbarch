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
