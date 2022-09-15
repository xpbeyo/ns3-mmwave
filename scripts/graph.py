# x and y given as array_like objects
import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import os


def read_trace(file_path, file_type):
    if (file_type == "DlPdcpStats"):
        dlpdcp_dict = {}
        columns = ["action", "time", "cellID", "RNTI", "LCID", "packetSize", "delay"]
        for column in columns:
            dlpdcp_dict[column] = []
        file = open(file_path)
        line = file.readline()
        while line:
            splitted = line.split()
            for i in range(len(columns)):
                if columns[i] == "action":
                    dlpdcp_dict[columns[i]].append(splitted[i])
                elif columns[i] == "delay":
                    if splitted[0] == "Tx":
                        dlpdcp_dict[columns[i]].append(None)
                    else:
                        dlpdcp_dict[columns[i]].append(int(splitted[i]))
                elif columns[i] == "time":
                    dlpdcp_dict[columns[i]].append(float(splitted[i]))
                else:
                    dlpdcp_dict[columns[i]].append(int(splitted[i]))
            line = file.readline()
        file.close()
        rx = pd.DataFrame(dlpdcp_dict)
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
    elif(file_type in ["TcpRx", "BuildingTcpRx", "Rtt"]):
        rx = np.genfromtxt(file_path)

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

def get_rate_smooth(rx, delta_t=0.05):
    assert(rx.shape[0] > 0)
    rates = np.empty((0, 2))
    acc = 0
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

"""
Sample an array by some time interval
"""
def sample_arr(arr, delta_t=0.05):
    acc = 0
    time = 0
    cnt = 0
    sampled_arr = np.empty((0, 2))
    for i in range(arr.shape[0]):
        if arr[i, 0] <= time + delta_t:
            acc += arr[i, 1]
            cnt += 1
        else:
            if cnt == 0:
                sampled_arr = np.concatenate((sampled_arr, np.array([[time, 0]])))
            else:
                sampled_arr = np.concatenate((sampled_arr, np.array([[time, acc / cnt]])))
            time += delta_t
            acc = 0
            cnt = 0
    return sampled_arr

def plot_attributes_by_cong_control(
    attributes,
    cong_controls,
    time_interval=0.075,
    markers=["s", "p", "P", "*", "h"],
    graph_filename="comparison.png",
    base_dir="scripts/traces/",
):
    assert(len(markers) >= len(cong_controls))
    def get_style(index):
        return {
            "marker": markers[index],
            "markersize": 0.5,
            "linewidth": 0.4,
        }
    fig, axes = plt.subplots(nrows=len(attributes), sharex=True, figsize=(12, 8))
    plt.xlabel("Time(s)")
    for j in range(len(attributes)):
        axes[j].set_title(attributes[j])
        if attributes[j] == "throughput":
            axes[j].set_ylabel("Mb/s")
            for k in range(len(cong_controls)):
                rates = []
                data_sink_file = os.path.join(base_dir, cong_controls[k], "mmWave-tcp-data-0.txt")
                if os.path.isfile(data_sink_file):
                    rx_tcp = read_trace(data_sink_file, "TcpRx")
                    rates = get_rate_smooth(rx_tcp, delta_t=time_interval)
                    axes[j].plot(rates[:, 0], rates[:, 1], label=f"{cong_controls[k]}", **get_style(k))
        if attributes[j] == "rtt":
            axes[j].set_ylabel("ms")
            for k in range(len(cong_controls)):
                rtt_file = os.path.join(base_dir, cong_controls[k], "mmWave-tcp-rtt-0.txt")
                if os.path.isfile(rtt_file):
                    rtt = read_trace(rtt_file, "Rtt")[:, 0:2]
                    rtt[:, 1] = rtt[:, 1] * 1e3
                    rtt = sample_arr(rtt, delta_t=time_interval)
                    axes[j].plot(rtt[:, 0], rtt[:, 1], label=f"{cong_controls[k]}", **get_style(k))
        axes[j].legend()
    plt.savefig(os.path.join(base_dir, graph_filename))

if __name__ == "__main__":
    # cong_controls = ["TcpIllinois", "TcpVegas", "TcpCubic", "Tcp5G"]
    cong_controls = ["TcpCubic", "Tcp5G"]
    # cong_controls = ["Tcp5G"]
    attributes = ["throughput", "rtt"]
    base_dirs = ["scripts/results/", "scripts/moving/"]
    for base_dir in base_dirs:
        plot_attributes_by_cong_control(
            attributes,
            cong_controls,
            time_interval=0.5,
            base_dir=base_dir,
        )
