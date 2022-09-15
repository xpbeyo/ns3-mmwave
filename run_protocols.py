import subprocess
import argparse
import time
import random
import threading
import atexit
import signal
import os
"""
python3 run_protocols.py --result_folder "scripts/results/" --num_UEs 4 --num_packets 50000000
python3 run_protocols.py --result_folder "scripts/moving/" --num_UEs 4 --num_packets 50000000 --moving_UEs
python3 run_protocols.py --result_folder "scripts/traces/" --num_UEs 1 --num_packets 5000000 --moving_UEs --cong_controls Tcp5G
"""
def run_cmd(cmd):
    while True:
        # this is added because some undebuggable error happen
        # if run waf cmds immediately after build
        time.sleep(random.randint(1, 10))
        print(f"Running {cmd}")
        p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, shell=True)
        res = p.communicate()
        if p.returncode == 0:
            break

def interrupt(proc):
    print(str(proc), "listening for interrupt")
    try:
        while proc.poll() is None:
            time.sleep(0.1)

    except KeyboardInterrupt:
        proc.terminate()
        print(f"{str(proc)} terminated")
        raise

def quit_all_subprocesses(processes):
    for p in processes:
        os.killpg(os.getpgid(p.pid), signal.SIGTERM)
        print(f"{p} terminated")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--cong_controls", type=str, nargs="*", default=["TcpCubic", "Tcp5G"])
    parser.add_argument("--num_UEs", type=int, default=1)
    parser.add_argument("--num_enbs", type=int, default=1)
    parser.add_argument("--num_packets", type=int, default=5e6)
    parser.add_argument("--result_folder", type=str, default="scripts/traces/")
    parser.add_argument("--moving_UEs", default=False, action="store_true")

    args = parser.parse_args()
    configure_cmd = "./waf configure --disable-python --enable-examples --disable-werror --build-profile=optimized"
    build_cmd = "./waf build"
    run_cmds = []
    for i in range(len(args.cong_controls)):
        run_cmds.append(
            f"./waf --run \"mmwave-tcp-multiple-ue "
            f"--CongControl={args.cong_controls[i]} "
            f"--log=false "
            f"--movingUEs={str(args.moving_UEs).lower()} "
            f"--numUEs={args.num_UEs} "
            f"--numEnbs={args.num_enbs} "
            f"--numPackets={args.num_packets} "
            f"--ResultFolder='{args.result_folder}'\""
        )

    configure = subprocess.Popen(configure_cmd, shell=True)
    configure.wait()
    build = subprocess.Popen(build_cmd, shell=True)
    build.wait()
    
    # pool = multiprocessing.Pool()
    # pool.map(run_cmd, [cmd for cmd in run_cmds])
    processes = []
    for cmd in run_cmds:
        # time.sleep(random.randint(1, 10))
        print(f"Running {cmd}")
        p = subprocess.Popen(cmd, shell=True)
        processes.append(p)
    
    for p in processes:
        p.wait()

