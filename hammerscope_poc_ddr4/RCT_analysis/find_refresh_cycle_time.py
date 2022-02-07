import csv
from collections import defaultdict
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.cm as cm
import sys
import numpy as np
from scipy.stats import shapiro
from tqdm import tqdm

START_TIME = 2
FLIP = 4

START_NS = int(30500000)
REFRESH_NS = 30601323
END_NS = int(30550000)

def main():
    with open(sys.argv[1]) as f:
        my_dict = defaultdict(list)
        csv_reader = csv.reader(f)
        next(csv_reader)  # skip_header

        counter = 0
        for row in csv_reader:
            if row[FLIP].isdigit() and int(row[FLIP]) != 0:
                my_dict[0].append((int(row[START_TIME])))
                counter += 1

        for key in my_dict:
            print key, len(my_dict[key])

        res = []
        min_score = 1
        factor = 1
        for cycle_time in tqdm (range(START_NS, END_NS, factor), desc="Loading..."):
            
            for key in my_dict:
                lst = my_dict[key]
                tmp = [hammer_time % cycle_time for hammer_time in lst]
                break
            
            num_bins = int((cycle_time//1e6)+1)*200
            frequency, bins = np.histogram(tmp, bins=num_bins, range=[0, cycle_time])
            if frequency[0] != 0 and frequency[-1] != 0:
                if np.count_nonzero(frequency == 0) == 0:
                    res.append(num_bins*1000)
                    continue
                frequency = np.roll(frequency, -(np.where(frequency == 0)[0][0]))

            start = np.where(frequency != 0)[0][0]
            end = np.size(frequency) - 1 - np.where(frequency[::-1] != 0)[0][0]
            new_score = float(end - start + 1)/num_bins
            res.append(new_score)

        
        val, idx = min((val, idx) for (idx, val) in enumerate(res))
        cycle_time = START_NS+(idx*factor)
        tmp = [hammer_time % cycle_time for hammer_time in my_dict[my_dict.keys()[0]]]
        num_bins = int((cycle_time//1e6)+1)*10
        frequency, bins = np.histogram(tmp, bins=num_bins, range=[0, cycle_time])
        print 'min time is:', (START_NS+idx*factor), 'score:', val
        plt.bar(range(num_bins), frequency, facecolor='blue'  , alpha=0.75, label='Rowhammer success')
        plt.legend()
        plt.xlabel('start time % RCTC ({} ns)'.format(cycle_time))
        plt.ylabel('Number of bitflips')
        plt.legend()
        plt.show()

if __name__ == '__main__':
    main()

