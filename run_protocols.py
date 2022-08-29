import os
cong_controls = ["TcpIllinois", "TcpVegas"]
run_cmds = []
for i in range(len(cong_controls)):
    run_cmds.append(f"./waf --run \"mmwave-tcp-multiple-ue --CongControl={cong_controls[i]} --log=false\"")

run_cmd = "&".join(run_cmds)
os.system(run_cmd)
