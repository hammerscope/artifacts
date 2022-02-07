HammerScope source code for KASLR POC   
### Requirements 
The POC code uses Python 2.7.

Install python
```terminal
sudo apt update 
sudo apt install python2
```

Install pip
```terminal
curl https://bootstrap.pypa.io/pip/2.7/get-pip.py --output get-pip.py
sudo python get-pip.py
```

Install dependencies:
```terminal
python -m pip install numpy matplotlib tqdm
```

### DRAM physical address mapping
The script [user_find_2MB_function.py](https://github.com/hammerscope/artifacts/blob/main/hammerscope_poc_ddr4/PyHammerScope/user_find_2MB_function.py) 
will allocate <number_of_hugepages> 2MB hugepages only using user permission [[1]](#1),
and split memory rows to [N](https://github.com/hammerscope/artifacts/blob/main/hammerscope_poc_ddr4/PyHammerScope/user_find_2MB_function.py#L12) sets
using time access measurements [[2]](#2).
```terminal
python PyHammerScope/user_find_2MB_function.py <number_of_hugepages>
```
[The DRAM Function we currently use](https://github.com/hammerscope/artifacts/blob/main/hammerscope_poc_ddr4/hammerscope_lib/PageData.cc#L24)

### Find DRAM refresh cycle time
The script [user_record_flip.py](https://github.com/hammerscope/artifacts/blob/main/hammerscope_poc_ddr4/PyHammerScope/user_record_flip.py) 
will allocate <number_of_hugepages> 2MB hugepages and search for bitflip using rowhammer attack, 
The script will record [N](https://github.com/hammerscope/artifacts/blob/main/hammerscope_poc_ddr4/PyHammerScope/CycleTime.py#L13) bitflips to csv file
```terminal
./hammerscope_launcher.sh PyHammerScope/user_record_flip.py <number_of_hugepages> 
```

The script [find_refresh_cycle_time.py](https://github.com/hammerscope/artifacts/blob/main/hammerscope_poc_ddr4/RCT_analysis/find_refresh_cycle_time.py) 
will parse the output .csv file and scan for best cycle time between 
[start time](https://github.com/hammerscope/artifacts/blob/main/hammerscope_poc_ddr4/RCT_analysis/find_refresh_cycle_time.py#L14) to 
[end time](https://github.com/hammerscope/artifacts/blob/main/hammerscope_poc_ddr4/RCT_analysis/find_refresh_cycle_time.py#L16) 
```terminal
python RCT_analysis/find_refresh_cycle_time.py <bitflip_record_file_path>
```
Update POC refresh cycle [start time](https://github.com/hammerscope/artifacts/blob/main/hammerscope_poc_ddr4/PyHammerScope/CycleTime.py#L14) and 
[end time](https://github.com/hammerscope/artifacts/blob/main/hammerscope_poc_ddr4/PyHammerScope/CycleTime.py#L15) with the result.

### POC RUN
The script [user_measure_app_kaslr.py](https://github.com/hammerscope/artifacts/blob/main/hammerscope_poc_ddr4/PyHammerScope/user_measure_app_kaslr.py)
will allocate <number_of_hugepages> 2MB hugepages, search for bitflips using RowHammer attack, and do a HammerScope measurement in parallel to the 
[KASLR process](https://github.com/hammerscope/artifacts/blob/main/hammerscope_poc_ddr4/kaslr/kaslr.cc)  
```terminal
./hammerscope_launcher.sh PyHammerScope/user_measure_app_kaslr.py <number_of_hugepages> 
```

## References
<a id="1">[1]</a> 
Youssef Tobah, Andrew Kwong, Ingab Kang, Daniel Genkin, and
Kang G Shin. [SpecHammer: Combining Spectre and RowHammer for
new speculative attacks](https://rtcl.eecs.umich.edu/rtclweb/assets/publications/2022/oakland22-tobah.pdf). In S&P, May 2022.

<a id="2">[2]</a> 
Peter Pessl, Daniel Gruss, Clémentine Maurice, Michael Schwarz, and
Stefan Mangard. [DRAMA: exploiting DRAM addressing for cross-cpu
attacks](https://www.usenix.org/conference/usenixsecurity16/technical-sessions/presentation/pessl). In USENIX Security Symposium, pages 565–581. USENIX
Association, 2016.
