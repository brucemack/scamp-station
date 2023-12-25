Updating
========
        git submodule update --recursive --remote

Flash
=====

        openocd -f interface/raspberrypi-swd.cfg -f target/rp2040.cfg -c "program main.elf verify reset exit"
