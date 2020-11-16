#!/usr/bin/env python2
# -*- coding: utf-8 -*-
##################################################
# GNU Radio Python Flow Graph
# Title: Analyse RTLSDR capture
# Author: HB9EGM
# GNU Radio version: 3.7.13.5
##################################################


from gnuradio import analog
from gnuradio import blocks
from gnuradio import digital
from gnuradio import digital;import cmath
from gnuradio import eng_notation
from gnuradio import filter
from gnuradio import gr
from gnuradio.eng_option import eng_option
from gnuradio.filter import firdes
from optparse import OptionParser
import pmt


class analyse_capture(gr.top_block):

    def __init__(self):
        gr.top_block.__init__(self, "Analyse RTLSDR capture")

        ##################################################
        # Variables
        ##################################################
        self.samp_rate = samp_rate = 2048000

        self.taps = taps = firdes.low_pass(1.0/256, samp_rate, 3e3, 10e2, firdes.WIN_HAMMING, 6.76)

        self.decim = decim = 16
        self.audio_rate = audio_rate = int(32e3)

        ##################################################
        # Blocks
        ##################################################
        self.low_pass_filter_0 = filter.fir_filter_ccf(4, firdes.low_pass(
        	1, audio_rate, 200, 20, firdes.WIN_HAMMING, 6.76))
        self.freq_xlating_fft_filter_ccc_0 = filter.freq_xlating_fft_filter_ccc(decim, (taps), 145725e3-145700e3, samp_rate)
        self.freq_xlating_fft_filter_ccc_0.set_nthreads(1)
        self.freq_xlating_fft_filter_ccc_0.declare_sample_delay(0)
        self.digital_mpsk_receiver_cc_0 = digital.mpsk_receiver_cc(2, 0, cmath.pi/100.0, -0.25, 0.25, 0.25, 0.04, 64, 64*64/4, 0.005)
        self.digital_diff_phasor_cc_0 = digital.diff_phasor_cc()
        self.digital_binary_slicer_fb_0 = digital.binary_slicer_fb()
        self.blocks_uchar_to_float_0_0 = blocks.uchar_to_float()
        self.blocks_uchar_to_float_0 = blocks.uchar_to_float()
        self.blocks_multiply_xx_0 = blocks.multiply_vcc(1)
        self.blocks_float_to_complex_1 = blocks.float_to_complex(1)
        self.blocks_float_to_complex_0 = blocks.float_to_complex(1)
        self.blocks_file_source_0 = blocks.file_source(gr.sizeof_char*1, './iq.raw', False)
        self.blocks_file_source_0.set_begin_tag(pmt.PMT_NIL)
        self.blocks_file_sink_0 = blocks.file_sink(gr.sizeof_char*1, './psk125.bit', False)
        self.blocks_file_sink_0.set_unbuffered(False)
        self.blocks_deinterleave_0 = blocks.deinterleave(gr.sizeof_char*1, 1)
        self.blocks_complex_to_float_0 = blocks.complex_to_float(1)
        self.analog_sig_source_x_1 = analog.sig_source_c(audio_rate, analog.GR_COS_WAVE, 588, 1, 0)
        self.analog_nbfm_rx_0 = analog.nbfm_rx(
        	audio_rate=audio_rate,
        	quad_rate=samp_rate/decim,
        	tau=75e-6,
        	max_dev=5e3,
          )
        self.analog_const_source_x_0 = analog.sig_source_f(0, analog.GR_CONST_WAVE, 0, 0, 0)
        self.analog_agc_xx_0 = analog.agc_ff(1e-4, 0.2, 1.0)
        self.analog_agc_xx_0.set_max_gain(1)



        ##################################################
        # Connections
        ##################################################
        self.connect((self.analog_agc_xx_0, 0), (self.blocks_float_to_complex_1, 0))
        self.connect((self.analog_const_source_x_0, 0), (self.blocks_float_to_complex_1, 1))
        self.connect((self.analog_nbfm_rx_0, 0), (self.analog_agc_xx_0, 0))
        self.connect((self.analog_sig_source_x_1, 0), (self.blocks_multiply_xx_0, 1))
        self.connect((self.blocks_complex_to_float_0, 0), (self.digital_binary_slicer_fb_0, 0))
        self.connect((self.blocks_deinterleave_0, 0), (self.blocks_uchar_to_float_0, 0))
        self.connect((self.blocks_deinterleave_0, 1), (self.blocks_uchar_to_float_0_0, 0))
        self.connect((self.blocks_file_source_0, 0), (self.blocks_deinterleave_0, 0))
        self.connect((self.blocks_float_to_complex_0, 0), (self.freq_xlating_fft_filter_ccc_0, 0))
        self.connect((self.blocks_float_to_complex_1, 0), (self.blocks_multiply_xx_0, 0))
        self.connect((self.blocks_multiply_xx_0, 0), (self.low_pass_filter_0, 0))
        self.connect((self.blocks_uchar_to_float_0, 0), (self.blocks_float_to_complex_0, 0))
        self.connect((self.blocks_uchar_to_float_0_0, 0), (self.blocks_float_to_complex_0, 1))
        self.connect((self.digital_binary_slicer_fb_0, 0), (self.blocks_file_sink_0, 0))
        self.connect((self.digital_diff_phasor_cc_0, 0), (self.blocks_complex_to_float_0, 0))
        self.connect((self.digital_mpsk_receiver_cc_0, 0), (self.digital_diff_phasor_cc_0, 0))
        self.connect((self.freq_xlating_fft_filter_ccc_0, 0), (self.analog_nbfm_rx_0, 0))
        self.connect((self.low_pass_filter_0, 0), (self.digital_mpsk_receiver_cc_0, 0))

    def get_samp_rate(self):
        return self.samp_rate

    def set_samp_rate(self, samp_rate):
        self.samp_rate = samp_rate

    def get_taps(self):
        return self.taps

    def set_taps(self, taps):
        self.taps = taps
        self.freq_xlating_fft_filter_ccc_0.set_taps((self.taps))

    def get_decim(self):
        return self.decim

    def set_decim(self, decim):
        self.decim = decim

    def get_audio_rate(self):
        return self.audio_rate

    def set_audio_rate(self, audio_rate):
        self.audio_rate = audio_rate
        self.low_pass_filter_0.set_taps(firdes.low_pass(1, self.audio_rate, 200, 20, firdes.WIN_HAMMING, 6.76))
        self.analog_sig_source_x_1.set_sampling_freq(self.audio_rate)


def main(top_block_cls=analyse_capture, options=None):

    tb = top_block_cls()
    tb.start()
    tb.wait()


if __name__ == '__main__':
    main()
