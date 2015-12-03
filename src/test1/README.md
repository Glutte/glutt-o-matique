This example code using FreeRTOS is taken from:

* [Deploy FreeRTOS Embedded OS in under 10 seconds!](http://istarc.wordpress.com/2014/07/10/stm32f4-deploy-an-embedded-os-under-10-seconds/)

Build:
* `make clean`
* `make -j4 # Non-optimized build`
* Other build options:
        * `make -j4 release # Non-optimized build`
        * `make -j4 release-memopt # Size-optimized build`
        * `make debug # Debug build`

Deploy to target via OpenOCD:
* `make deploy`

