---
layout: code
language: yaml
caption: requirements/low-level/llr-1.1-rpm-sampling.yaml
---

id: LLR-1.1-rpm-sampling
parent: HLR-CTRL-001
input:
  type: adc
  channel: 0
  stale_us: 5000
redundancy:
  pair_with: LLR-1.2-rpm-spi
  vote: median
  tolerance: 50
output:
  protocol: j1939
  pgn: 0xFEF1
  rate_hz: 10

Note:
En tagning. Tolv rader.

Notera redundansblocket. Det här LLR:et säger: vi har en RPM-sensor på ADC-kanal noll. Den ska paras ihop med LLR-1.2 som är samma RPM mätt över SPI. Median-voting används för att slå ihop dem. Om de skiljer sig mer än 50 RPM markeras inkonsistens.

Och utgången: J1939-protokollet, PGN 0xFEF1, tio gånger per sekund.

Tolv rader YAML genererar — vänta tills nästa slide — något i storleksordningen 387 rader produktionsfärdig C-kod. ISR-handlers, lock-free ringbuffer-pushes, validering, voter-logik, deadline-scheduling, J1939-frame-encoding, allt. Hand på hjärtat: hur många buggar tror ni det blir när en människa skriver de raderna?

Genererad kod har deterministiska buggar. Hittas en, fixas den i generatorn, försvinner från alla LLR:er.
