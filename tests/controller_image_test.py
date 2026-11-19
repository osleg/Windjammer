#!/usr/bin/env python3
"""Check the linked controller image's audio and protocol ownership boundary."""
import re
import sys

text = open(sys.argv[1], encoding="utf-8").read()
symbols = set(re.findall(r"^([A-Za-z_][A-Za-z_0-9.]*)\:$", text, re.M))
required = {"fm1_alnk0_irq", "cafa_audio_block", "uac_play", "cafa_input", "cafa_draw", "ed_service"}
forbidden = {"audio_block", "mix_block", "events_block", "keyboard_block", "autosave_boot", "autosave_poll",
             "analog_render", "phase_render", "lofi_render", "sample_render", "formant_render",
             "trio_render", "wheel_render", "grain_render", "phys_render", "drum_render",
             "drumx_render", "noise_render", "fm6_render", "slice_render", "digital_render"}
linked = {line.split()[-1] for line in open(sys.argv[2], encoding="utf-8") if line.split()}
forbidden_data = {"eng_mem", "eng_lite", "dly_buf", "cho_buf"}
missing = sorted(required - symbols)
retained = sorted((forbidden & symbols) | (forbidden_data & linked))
if missing or retained:
    raise SystemExit(f"controller image: missing {missing}; forbidden {retained}")
print("Controller image: USB return/protocol linked; standalone mixer, engine callbacks/buffers, sequencer render and autosave absent")
