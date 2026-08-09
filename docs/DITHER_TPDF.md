# TPDF dither in Navalha 2 WAV exports

Implemented on 9 August 2026 as the first deterministic unit of output-audio
P1.

## Policy

- PCM16 and PCM24 receive TPDF dither immediately before fixed-point
  quantization;
- float32 never receives dither and preserves its floating-point samples;
- dither is an encoding concern, not a process inserted into the realtime
  floating-point audio bus;
- non-finite input is replaced with digital silence at the writer boundary for
  all three formats;
- full-scale values remain bounded by the existing symmetric integer range.

`WavStreamWriter` enables TPDF by default for integer formats. Scientific
fixtures and legacy golden files may pass `WavDitherMode::none` explicitly when
exact undithered bytes are part of the test definition. Production recording,
TRACK MASTER, ALBUM MASTER and portable PCM24 conversion use the default
dithered policy.

## Algorithm and reproducibility

Two independent uniform values from a xorshift32 sequence are subtracted for
each encoded sample. The result is triangular noise in the range ±1 LSB before
rounding. Left and right consume successive values from the sequence rather
than sharing one dither value.

The seed is configurable through `WavEncodingOptions` and has a fixed non-zero
default. Therefore the same input and options produce the same WAV bytes,
which keeps renders auditable. A zero seed is safely replaced with the default
because zero is a locked state for xorshift32.

No noise shaping is applied in this first implementation. Noise shaping would
require a separately named profile, sample-rate-aware stability analysis and
listening tests; it must not be hidden inside the base writer.

## Automated evidence

The core contracts verify that:

- a fixed seed is byte-deterministic;
- another seed changes the integer payload;
- zero-input PCM16 stays within ±1 integer LSB;
- the non-zero occurrence rate remains within 23–27%, consistent with TPDF
  followed by rounding at digital silence;
- mean integer error stays within 0.01 LSB of zero over 131,072 samples;
- PCM24 also receives dither;
- explicitly undithered PCM16 silence remains all zero;
- float32 silence remains all zero even when TPDF is requested;
- `NaN` and `Inf` are encoded as finite digital silence.

These contracts establish algorithmic behavior. The remaining P1 work still
includes BS.1770 loudness, post-encoding analysis, a quality mastering profile
and listening acceptance.
