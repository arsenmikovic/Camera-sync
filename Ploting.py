with open("log.txt", "r") as file:
    file_content = file.read()

import matplotlib.pyplot as plt
s=file_content[1361:]
l=s.split("\n")
data=[]
for x in l:
    e=x.split()
    if(e!=[]):
        data.append(e[-1][1:-1])
#data now contains these pars of drift and sequence number

x=[]
y=[]
for d in data:
    e=d.split(",")
    if(abs(int(e[0][:-5]))<3000):
        x.append(int(e[1]))
        y.append(int(e[0][:-5]))
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
plt.show()

print("slope: "+ str(w[0]))
print("offset: "+ str(w[1]))
