# Contributing to AT-D878 Open Firmware Project

Thank you for your interest in contributing to the **AnyTone AT-D878 Open Firmware & AT-D878 Studio** project!

This project is an open-source initiative dedicated to providing modern, transparent, and modular firmware and programming software for the AnyTone AT-D878 series (starting with AT-D878UVII Plus / D878UV2A-BORD).

---

## 🛡️ Critical Safety Guidelines (Non-Destructive Guarantee)

All contributions must strictly respect our safety rules:
1. **Never overwrite the manufacturer ROM bootloader:** The GD32F303 ROM bootloader (`0x08000000 - 0x08003FFF`) must remain untouched. All application firmware must link at `0x08004000`.
2. **Dry-Run first:** Any flasher or communication feature must support non-destructive simulation modes before interacting with live hardware.
3. **No proprietary codec redistribution:** Do NOT commit proprietary AMBE+2 codec blobs. The baseband leverages the on-board hardware vocoder (SCT3258) or open formats (Codec2 / M17).

---

## 🚀 How to Contribute

1. **Fork the Repository:** Create your own branch (`git checkout -b feature/amazing-feature`).
2. **Develop & Test:**
   - Firmware changes: Verify compilation with `arm-none-eabi-gcc`.
   - Studio/Desktop changes: Test with PySide6 and the built-in Unicorn Engine simulator.
3. **Commit your changes:** Write clear, concise commit messages.
4. **Open a Pull Request:** Describe the problem solved and test evidence.

---

## 📬 Contact & Ham Radio Community

- **Project Initiator:** TA5THK (Fatih ATCI)
- **License:** GNU General Public License v3.0 (GPL-3.0)
