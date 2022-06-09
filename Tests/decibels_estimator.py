import math
import numpy as np
import matplotlib.pyplot as plt

frequencies = [125, 250, 500, 1000, 2000, 4000, 8000]
T1 = []
D1 = []

assert len(T1) == len(D1)

T0 = [T1[i] / 10 ** (D1[i] / 20) for i, _ in enumerate(T1)]
T0_mean = np.mean(T0)
plt.figure(constrained_layout=True)
plt.plot(T0)
# plt.show()
# plt.plot(frequencies, T0)
# plt.close(fig)
# plt.savefig(os.path.join(f'{image_path}', f'{t:.2f}.jpg'), dpi=300)
