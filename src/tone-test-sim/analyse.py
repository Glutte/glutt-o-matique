#!/usr/bin/env python

import numpy as np
import matplotlib.pyplot as plt

dat = np.loadtxt("tone.csv", dtype=np.int32, delimiter=",")
# columns :"freq, threshold, num_samples, detector_output"

fig, ax = plt.subplots()
plt.scatter(dat[...,0], dat[...,1], c=dat[...,3].astype(np.float32))
plt.show()

if 0:
    thresholds = [800, 2000, 4000, 8000]

    fig, ax = plt.subplots()

    for th in thresholds:
        dat_th = dat[dat[...,1] == th]
        dat_th[...,1]
        ax.plot(dat_th[...,0], dat_th[...,3], label="{}".format(th))

    legend = ax.legend(loc='upper left', shadow=True)
    plt.show()
