import math
import numpy as np
import matplotlib.pyplot as plt


TONE_FREQ = 60_000
diffs = [i - (math.floor((1024 * i << 8) / TONE_FREQ) * TONE_FREQ >> 8) / 1024 for i in range(100, 8000)]
max_i = max(diffs)
i_m = np.mean(diffs)
i_d = np.std(diffs)
plt.figure(constrained_layout=True)
# plt.subplot(1, 2, 1)
plt.plot(diffs)
# plt.subplot(1, 2, 2)
# plt.plot(diffs[1:150])
plt.title('Разность задаваемой и получаемой на выходе частот')
plt.xlabel('Частота, Гц')
plt.ylabel('Разность частот, Гц')
# plt.show()
graphic_filename = 'Frequency shifting.jpg'
plt.savefig(graphic_filename, dpi=300)
