import os
import glob

def delete_from_folder(folder, exceptions=[]):
    for filename in os.listdir(folder):
        file_path = os.path.join(folder, filename)
        try:
            if (os.path.isfile(file_path) or os.path.islink(file_path)) and filename not in exceptions:
                os.unlink(file_path)
        except Exception as e:
            print('Failed to delete %s. Reason: %s' % (file_path, e))

def delete_by_regex(path_regex):
    file_list = glob.glob(path_regex)
    for file_path in file_list:
        try:
            os.remove(file_path)
        except:
            print("Error while deleting file : ", file_path)

if __name__ == "__main__":
    logs_regex = ["*.txt", "*.pcap"]
    logs_regex_png = ["*.txt", "*.pcap", "*.png"]

    for regex in logs_regex:
        delete_by_regex(regex)

    script_folder = "scripts"
    for regex in logs_regex:
        script_folder_log = os.path.join(script_folder, regex)
        delete_by_regex(script_folder_log)
    log_folders = ["./scripts/traces", "./scripts/results", "./scripts/moving"]
    # log_folders = ["./scripts/traces"]
    for log_folder in log_folders:
        for filename in os.listdir(log_folder):
            dir_path = os.path.join(log_folder, filename)
            if os.path.isdir(dir_path):
                for regex in logs_regex:
                    regex_full = os.path.join(dir_path, regex)
                    delete_by_regex(regex_full)
