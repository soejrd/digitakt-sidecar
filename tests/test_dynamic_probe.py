#!/usr/bin/env python3
"""Local checks for I03 dynamic-probe ordering and evidence helpers."""
import sys
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))

import i03_dynamic_probe as probe


class ProbeHelpers(unittest.TestCase):
    def test_contiguous_blocks_and_sequences(self):
        events = [
            {'block': 1, 'sequence': 0, 'voice_start': False},
            {'block': 2, 'sequence': 1, 'voice_start': True},
            {'block': 3, 'sequence': 2, 'voice_start': False},
        ]
        self.assertTrue(probe._ordered(events))
        self.assertEqual([e['block'] for e in probe._start_events(events)], [2])

    def test_out_of_order_or_skipped_blocks_fail(self):
        events = [
            {'block': 1, 'sequence': 0, 'voice_start': False},
            {'block': 3, 'sequence': 1, 'voice_start': False},
        ]
        self.assertFalse(probe._ordered(events))
        events = [
            {'block': 1, 'sequence': 0, 'voice_start': False},
            {'block': 2, 'sequence': 2, 'voice_start': False},
        ]
        self.assertFalse(probe._ordered(events))

    def test_candidate_summary_is_revision_pinned(self):
        meta = {
            'firmware_label': 'I03P',
            'usb_track_version': '0.2',
            'syx_sha256': 'a' * 64,
            'stock_main_sha256': probe.MAIN_SHA,
            'source_hashes': {'mods/usb-track/usb_track.c': 'b' * 64},
        }
        summary = probe._candidate_summary(meta)
        self.assertEqual(summary['firmware_label'], 'I03P')
        self.assertEqual(summary['stock_main_sha256'], probe.MAIN_SHA)
        self.assertEqual(summary['source_hashes']['mods/usb-track/usb_track.c'], 'b' * 64)

    def test_evidence_scope_does_not_claim_hardware(self):
        self.assertIn('no hardware timing', probe.__doc__)

    def test_lfo_destination_is_a_sound_slot(self):
        source = (ROOT / 'tools/i03_dynamic_probe.py').read_text()
        self.assertIn('4: 17 << 8', source)
        self.assertNotIn('4: 132 << 8', source)

    def test_note_and_velocity_widths_are_explicit(self):
        source = (ROOT / 'tools/i03_dynamic_probe.py').read_text()
        self.assertIn('NOTE + 4 * track', source)
        self.assertIn('VELOCITY + 2 * track', source)
        self.assertIn("'velocity_8_8'", source)
        self.assertIn("'note_16_16'", source)


if __name__ == '__main__':
    unittest.main()
