# x and y given as array_like objects
import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import os


def read_trace(file_path, file_type):
    if (file_type == "DlPdcpStats"):
        file = open(file_path)
        lines = file.readlines()
        lines[0] = lines[0].replace("%", "").lstrip()
        file.close()
        file = open(file_path, "w")
        file.writelines(lines)
        file.close()

        rx = pd.read_csv(file_path, delim_whitespace=True)[["start", "end", "RxBytes"]]
    elif (file_type == "UlPdcpStats"):
        file = open(file_path)
        lines = file.readlines()
        lines[0] = lines[0].replace("%", "").lstrip()
        file.close()
        file = open(file_path, "w")
        file.writelines(lines)
        file.close()

        rx = pd.read_csv(file_path, delim_whitespace=True)[["start", "end", "RxBytes"]]
    elif (file_type == "DlPdcpStats_sampled"):
        rx = pd.read_csv(file_path, delim_whitespace=True)
    elif(file_type == "TcpRx" or file_type == "BuilindTcpRx"):
        rx = pd.read_csv(file_path, delim_whitespace=True)
    return rx

def get_rate(rx):
    assert(rx.shape[0] > 0)
    acc = 0
    time = rx[0, 0]
    rates = np.empty((0, 2))
    for i in range(1, rx.shape[0]):
        if time == rx[i, 0]:
            acc += rx[i, 1]
        else:
            delta_t = rx[i, 0] - time
            rate = acc / delta_t
            rates = np.concatenate((rates, np.array([[time, rate]])), axis=0)
            time = rx[i, 0]
            acc = 0
    rates[:, 1] /= 1e6
    return rates

def get_avg_diff(times):
    diff_times = np.diff(times)
    return np.average(diff_times)

def get_duration(times):
    return times[-1] - times[0]

def get_overall_throughput(rx):
    duration = get_duration(rx[:, 0])
    total_data = np.sum(rx[:, 1]) / 1e6
    throughput = total_data / duration;
    print(f"Received {total_data} Mb of data for duration of {duration} seconds. Overall throughput is {throughput} Mb/s.")
    return throughput

def get_rate_smooth(rx):
    assert(rx.shape[0] > 0)
    rates = np.empty((0, 2))
    acc = 0
    delta_t = 0.005
    cur_t = rx[0, 0]
    
    for i in range(rx.shape[0]):
        if cur_t + delta_t < rx[i, 0]:
            rates = np.concatenate((rates, np.array([[cur_t, acc / delta_t]])), axis=0)
            cur_t += delta_t
            acc = 0
        else:
            acc += rx[i, 1]
    rates[:, 1] /= 1e6
    rates[:, 1] *= 8
    return rates

def get_rate_for_interval(rx):
    start = rx[0, 0]
    end = rx[0, 1]
    acc = 0
    rates = np.empty((0, 2))
    for i in range(rx.shape[0]):
        if rx[i, 0] != start or rx[i, 1] != end:
            rates = np.concatenate((rates, np.array([[start, acc / (end - start)]])), axis=0)
            acc = rx[i, 2]
            start = rx[i, 0]
            end = rx[i, 1]
        else:
            acc += rx[i, 2]
    rates = np.concatenate((rates, np.array([[start, acc / (end - start)]])), axis=0)
    rates[:, 1] /= 1e6
    rates[:, 1] *= 8
    return rates


if __name__ == "__main__":
    plot_until = 200
    nUEs = 1
    markers = ["s", "p", "P", "*", "h"]
    rx_tcps = []
    for i in range(nUEs):
        rx_tcps.append(read_trace(f"mmWave-tcp-data-{i}.txt", "TcpRx").values)
    rates_tcps = []
    for i in range(nUEs):
        rates_tcps.append(get_rate_smooth(rx_tcps[i]))
    fig, ax = plt.subplots()
    for i in range(nUEs):
        ax.plot(rates_tcps[i][:plot_until, 0], rates_tcps[i][:plot_until, 1], label=f"mmWave-tcp-data-{i}", marker=markers[i], linestyle='None')
    plt.xlabel("Time(s)")
    plt.ylabel("Mb/s")
    plt.legend()
    plt.savefig('throughput.png')
