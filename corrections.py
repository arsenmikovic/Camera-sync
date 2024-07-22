import numpy as np
with open("oneCorrectio.txt", "r") as file:
    file_content = file.read()

import matplotlib.pyplot as plt
s=file_content[1400:]
l=s.split("\n")
A=[]
for x in l:
    y=x.split()
    if y:
        A.append(int(y[-1][:-5]))
ypoints = np.array(A)
xpoints = np.array(range(len(A)))

plt.figure(figsize=(12,6))
plt.scatter(xpoints, ypoints , s=5)

A.pop(0)
ypoints = np.array(A)
xpoints = np.array(range(len(A)))

plt.figure(figsize=(12,6))
plt.scatter(xpoints, ypoints , s=5)
