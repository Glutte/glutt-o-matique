Introduction
------------

This folder contains a set of scripts that can be used to automatically decode the PSK125 beacon at 22:00

Dependencies
------------

* Python 3
* GNURadio 3.7
* An SDR device and a suitable I/Q capture tool

Principle of operation
----------------------

1. Capture I/Q data, in u8 format, at 20148ksps, centered on 145.700MHz, into a file called `iq.raw`
1. Demodulate FM and PSK using the GNURadio flowgraph `analyse_capture.grc`. It will write a file called `psk125.bit`
1. Run the `varidecode.py` script, which will read `psk125.bit` and write `psk125.txt` with decoded beacon data

Example for RTLSDR: `rtl_sdr -f 145700000 -n 204800000 iq.raw` will capture 100 seconds worth of IQ data.

References
----------

https://sdradventure.wordpress.com/2011/10/15/gnuradio-psk31-decoder-part-1/

https://sdradventure.wordpress.com/2011/10/15/gnuradio-psk31-decoder-part-2/

For GnuRadio 3.8, maybe consider https://github.com/dl1ksv/gr-radioteletype
