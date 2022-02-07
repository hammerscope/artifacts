from HammerScope import *
from tqdm import tqdm
import numpy as np
import random
import copy
import csv


####
NO_THRESHOLD       = -1
NO_IDLE            = 0
####
FLIPS_NUM = 500 #31*10*10
START_NS = 30500000
END_NS   = 30650000
####
ROW_BITS       = 16
PRE_READ_OP    = 500*2
TRYS           = 20
MAX_ROW_NUMBER = (1<<ROW_BITS)
BANK_CNT = 16


def get_random_hl_arr(hl, trys, max_rows):
    tmp_hl_arr = []
    counter = 0
    index_var = 1
    orig_index = hl.base_index
    tmp_hl = copy.deepcopy(hl)
    trys_counter = 0
    while counter < trys:
      bank_num = (orig_index & (0xF << ROW_BITS)) >> ROW_BITS
      row_num = orig_index & (MAX_ROW_NUMBER - 1)
      allign_row_num = row_num & 0xFFF0
      #tmp_hl.base_index = (((bank_num + index_var) % BANK_CNT) << ROW_BITS) + random.randint(allign_row_num, allign_row_num+0x10)
      tmp_hl.base_index = (bank_num << ROW_BITS) + random.randint(allign_row_num, allign_row_num+0x10)
      #print_log('tmp_hl.base_index:', hex(tmp_hl.base_index), hex(orig_index))
      rc, _ = test_hammer_bitflip(tmp_hl, PRE_READ_OP, NO_THRESHOLD, NO_IDLE)
      if rc == OK:
        tmp_hl_arr.append(copy.deepcopy(tmp_hl))
        counter += 1
      index_var *= -1
      trys_counter += 1
      if trys_counter > 1000:
        return None
    return tmp_hl_arr


def get_cycle_time_record(hl, num_read, tmp_hl_arr, file_path):
  print_log('get_cycle_time - num_read:', num_read)
  start_time_arr = []
  # collect flips data
  file_rec = open(file_path, 'a', buffering=0)
  writer = csv.writer(file_rec, delimiter=',')
  writer.writerow(['base_index', 'flip_index', 'start_time', 'end_time', 'flip', 'flip_value'])
  for flip_num in tqdm (range(FLIPS_NUM), desc="record flips..."):
      counter = 0
      while True:
        hammer_row_all_reachable_pages(tmp_hl_arr[random.randint(0, TRYS-1)].base_index, PRE_READ_OP, NO_THRESHOLD, NO_IDLE)
        clear_hammer_db()
             
        rc, value = test_hammer_bitflip(hl, int(num_read*1000), NO_THRESHOLD, NO_IDLE)
        if rc != OK:
          continue
        
        if value.flip == 0:
          counter += 1
          if counter == 1000:
            return -1, -1, -1
        
        writer.writerow([hl.base_index, hl.row_index, value.start_time, value.end_time, value.flip, value.value])
        
        if value.flip == 1:
          start_time_arr.append(value.start_time)
          break
        
  file_rec.close()      
  # find best cycle_time
  cycle_time_score = []
  num_bins = int((START_NS//1e6)+1)*200
  for cycle_time in tqdm (range(START_NS, END_NS, 1), desc="find best cycle time..."):
      tmp = [hammer_time % cycle_time for hammer_time in start_time_arr]
      frequency, bins = np.histogram(tmp, bins=num_bins, range=[0, cycle_time])
      if frequency[0] != 0 and frequency[-1] != 0:
        if np.count_nonzero(frequency == 0) == 0:
          res.append(num_bins*1000)
          continue
        frequency = np.roll(frequency, -(np.where(frequency == 0)[0][0]))
      start = np.where(frequency != 0)[0][0]
      end = np.size(frequency) - 1 - np.where(frequency[::-1] != 0)[0][0]
      cycle_time_score.append(end - start + 1)
  
  val, idx = min((val, idx) for (idx, val) in enumerate(cycle_time_score))
  best_time = START_NS+idx
  print_log('min time is:', (best_time), 'score:', val)
  
  num_bins = int((START_NS//1e6)+1)
  tmp = [hammer_time % best_time for hammer_time in start_time_arr]
  frequency, bins = np.histogram(tmp, bins=num_bins, range=[0, best_time])
  print_log(frequency) 
  
  if 0 not in frequency:
    return -1, -1, -1
  
  roll = 0
  if frequency[0] != 0 and frequency[-1] != 0:
    roll = (np.where(frequency == 0)[0][0])
    frequency = np.roll(frequency, -roll)
  
  start = np.where(frequency != 0)[0][0]
  end = np.size(frequency) - 1 - np.where(frequency[::-1] != 0)[0][0]
  
  bin_index = (start + roll + (end - start + 1) // 2) % num_bins
  bin_max_index = np.argmax(frequency)
  print_log('bin_index:', bin_index, 'bin_max_index:', bin_max_index)
  return best_time, bin_max_index, (start + roll + (end - start + 1) // 2) % num_bins
  
def get_cycle_time(hl, num_read, tmp_hl_arr):
  print_log('get_cycle_time - num_read:', num_read)
  start_time_arr = []
  # collect flips data
  for flip_num in tqdm (range(FLIPS_NUM), desc="record flips..."):
      counter = 0
      while True:
        
        #for j in range(TRYS):
        #     test_hammer_bitflip(tmp_hl_arr[j], PRE_READ_OP, NO_THRESHOLD, NO_IDLE)
        
        hammer_row_all_reachable_pages(tmp_hl_arr[random.randint(0, TRYS-1)].base_index, PRE_READ_OP, NO_THRESHOLD, NO_IDLE)
        clear_hammer_db()
             
        rc, value = test_hammer_bitflip(hl, int(num_read*1000), NO_THRESHOLD, NO_IDLE)
        if rc != OK:
          continue
        
        if value.flip == 0:
          counter += 1
          if counter == 1000:
            return -1, -1, -1
        
        if value.flip == 1:
          start_time_arr.append(value.start_time)
          break
             
  # find best cycle_time
  cycle_time_score = []
  num_bins = int((START_NS//1e6)+1)*200
  for cycle_time in tqdm (range(START_NS, END_NS, 1), desc="find best cycle time..."):
      tmp = [hammer_time % cycle_time for hammer_time in start_time_arr]
      frequency, bins = np.histogram(tmp, bins=num_bins, range=[0, cycle_time])
      if frequency[0] != 0 and frequency[-1] != 0:
        if np.count_nonzero(frequency == 0) == 0:
          res.append(num_bins*1000)
          continue
        frequency = np.roll(frequency, -(np.where(frequency == 0)[0][0]))
      start = np.where(frequency != 0)[0][0]
      end = np.size(frequency) - 1 - np.where(frequency[::-1] != 0)[0][0]
      cycle_time_score.append(end - start + 1)
  
  val, idx = min((val, idx) for (idx, val) in enumerate(cycle_time_score))
  best_time = START_NS+idx
  print_log('min time is:', (best_time), 'score:', val)
  
  num_bins = int((START_NS//1e6)+1)
  tmp = [hammer_time % best_time for hammer_time in start_time_arr]
  frequency, bins = np.histogram(tmp, bins=num_bins, range=[0, best_time])
  print_log(frequency) 
  
  if 0 not in frequency:
    return -1, -1, -1
  
  roll = 0
  if frequency[0] != 0 and frequency[-1] != 0:
    roll = (np.where(frequency == 0)[0][0])
    frequency = np.roll(frequency, -roll)
  
  start = np.where(frequency != 0)[0][0]
  end = np.size(frequency) - 1 - np.where(frequency[::-1] != 0)[0][0]
  
  bin_index = (start + roll + (end - start + 1) // 2) % num_bins
  bin_max_index = np.argmax(frequency)
  print_log('bin_index:', bin_index, 'bin_max_index:', bin_max_index)
  return best_time, bin_max_index, (start + roll + (end - start + 1) // 2) % num_bins
  