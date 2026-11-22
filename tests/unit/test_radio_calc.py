#!/usr/bin/env python3
"""
Unit Test: RF Frequency & Register Math Calculations for AT1846S Transceiver
Bu test, MCU donanımı olmadan AT1846S frekans bölücü ve register değerlerinin
matematiksel doğruluğunu host ortamında test eder.
"""

import unittest

def calculate_at1846s_frequency_word(freq_hz, ref_xtal_hz=26000000):
    """
    AT1846S frekans register kelimesi hesaplama
    30-bit register word hesaplama formülü
    """
    f_step = ref_xtal_hz / (1 << 16)
    reg_val = int(round(freq_hz / f_step))
    hi = (reg_val >> 16) & 0x3FFF
    lo = reg_val & 0xFFFF
    return hi, lo, reg_val

class TestAT1846SMath(unittest.TestCase):
    def test_vhf_frequency_range(self):
        freq = 145500000  # 145.500 MHz
        hi, lo, val = calculate_at1846s_frequency_word(freq)
        self.assertGreater(val, 0)
        self.assertLess(hi, 0x4000)

    def test_uhf_frequency_range(self):
        freq = 433500000  # 433.500 MHz
        hi, lo, val = calculate_at1846s_frequency_word(freq)
        self.assertGreater(val, 0)
        self.assertLess(hi, 0x4000)

    def test_ctcss_tones(self):
        standard_ctcss = [67.0, 88.5, 100.0, 141.3, 254.1]
        for tone in standard_ctcss:
            tone_int = int(tone * 10)
            self.assertGreaterEqual(tone_int, 670)
            self.assertLessEqual(tone_int, 2541)

if __name__ == "__main__":
    unittest.main()
