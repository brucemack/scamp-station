Overview
========

A simple SCAMP station based on the specification by Daniel Marks (KW4TI).  Target processor
is the RP2040 (Pi Pico).

![alt](docs/img.png)

Updating
========
        git submodule update --recursive --remote

Flash
=====

        openocd -f interface/raspberrypi-swd.cfg -f target/rp2040.cfg -c "program main.elf verify reset exit"


Filter Notes
============

* TI Single Supply LPF Notes: https://www.ti.com/lit/an/sboa231/sboa231.pdf?ts=1704451873900
* TI LPF Coef Notes: https://www.ti.com/lit/an/sloa049d/sloa049d.pdf?ts=1704460167951&ref_url=https%253A%252F%252Fwww.google.com%252F
* TI video: https://www.youtube.com/watch?v=L-V4sIJ9SNQ
* LPF Part: https://www.analog.com/media/en/technical-documentation/data-sheets/max7400-max7407.pdf

Copyright
=========

Copyright (C) 2023 - Bruce MacKinnon KC1FSZ

This work is covered under the terms of the GNU Public License (V3). Please consult the LICENSE file for more information.

This work is being made available for non-commercial use by the amateur radio community. Redistribution, commercial use or sale of any part is prohibited.
