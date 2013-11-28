import csv

x = []
y = []
with open("log.txt", "rb") as f:
    reader = csv.reader(f, delimiter=';')
    for row in reader:
        y.append(row[0])
        x.append(row[1])

