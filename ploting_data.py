with open("/home/pi/CLIENT.txt", "r") as file:
    file_content = file.read()

import matplotlib.pyplot as plt
import numpy as np
s=file_content
l=s.split("\n")
data=[]
for x in l:
    if("NEXT" in x):
        e=x.split()
        if(e!=[]):
            data.append(e[-1])
    #data now contains these pars of drift and sequence number

x=[]
y=[]
for d in data:
    e=d.split(",")
    if(abs(int(e[0]))<1000000):
        x.append(int(e[1]))
        y.append(int(e[0]))

#x cntains se numbers and y contains the drift values, removing any values that are an order of frame off

xpoints = np.array(x)
ypoints = np.array(y)

plt.figure(figsize=(12,6))
plt.scatter(xpoints, ypoints , s=5)

# fit least-squares with an intercept
A=np.vstack([xpoints, np.ones(len(xpoints))]).T
w = np.linalg.lstsq(A, ypoints, rcond=None)[0]

# plot best-fit line
plt.plot(xpoints, w[0]*xpoints + w[1], '-k')

plt.title("slope: "+ str(w[0])+" and average "+str(np.average(ypoints))[:7])

plt.show()
"""
with open("/home/pi/SERVER.txt", "r") as file:
    file_content = file.read()

s=file_content
l=s.split("\n")
jitter_data=[]
for x in l:
    if("jitter" in x):
        e=x.split()
        if(e!=[]):
            jitter_data.append(e[-1])
xj = []
yj = []

for j in jitter_data:
    e=j.split(",")
    if(abs(int(e[0]))<300000):
        xj.append(int(e[1]))
        yj.append(int(e[0]))

xjpoints = np.array(xj)
yjpoints = np.array(yj)


plt.figure(figsize=(12,6))
plt.scatter(xjpoints, yjpoints , s=5)


plt.show()


plt.figure(figsize=(12,6))
plt.scatter(xpoints, ypoints, s=5)
plt.scatter(xjpoints, yjpoints , s=5, color='red')


plt.show()
"""