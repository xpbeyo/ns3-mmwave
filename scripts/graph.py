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
    markers=["s", "p", "P", "*", "h"]
):
    fig, axes = plt.subplots(nrows=len(attributes), sharex=True)
    plt.xlabel("Time(s)")
    for j in range(len(attributes)):
        axes[j].set_title(attributes[j])
        if attributes[j] == "throughput":
            axes[j].set_ylabel("Mb/s")
            for k in range(len(cong_controls)):
                rates = []
                cong_dir = f"scripts/traces/{cong_controls[k]}"
                data_sink_file = os.path.join(cong_dir, f"mmWave-tcp-data-0.txt")
                if os.path.isfile(data_sink_file):
                    rx_tcp = read_trace(data_sink_file, "TcpRx").values
                    rates = get_rate_smooth(rx_tcp, delta_t=time_interval)
                    axes[j].plot(rates[:, 0], rates[:, 1], label=f"{cong_controls[k]}", marker=markers[k], linestyle='None')
        if attributes[j] == "rtt":
            axes[j].set_ylabel("ms")
            for k in range(len(cong_controls)):
                cong_dir = f"scripts/traces/{cong_controls[k]}"
                dlpdcpstats_file = os.path.join(cong_dir, "DlPdcpStats.txt")
                if os.path.isfile(dlpdcpstats_file):
                    dlpdcp = read_trace(dlpdcpstats_file, "DlPdcpStats")
                    dlpdcp = dlpdcp[dlpdcp["action"]=="Rx"][["time", "delay"]]
                    dlpdcp["delay"] = dlpdcp["delay"] / 1e6
                    delays = sample_arr(dlpdcp.values, delta_t=time_interval)
                    axes[j].plot(delays[:, 0], delays[:, 1], label=f"{cong_controls[k]}", marker=markers[k], linestyle='None')
        axes[j].legend()
    plt.savefig(os.path.join("scripts", "comparison.png"))

if __name__ == "__main__":
    cong_controls = ["TcpIllinois", "TcpVegas", "TcpCubic", "Tcp5G"]
    attributes = ["throughput", "rtt"]
    plot_attributes_by_cong_control(attributes, cong_controls)
