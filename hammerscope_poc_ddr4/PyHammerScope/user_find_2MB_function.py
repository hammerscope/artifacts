from HammerScope import *
from CycleTime import *
from BuddyInfo import *
from sys import argv
import time

KB                 = (1 << 10)
MB                 = (KB << 10)
GB                 = (MB << 10)
####
ROW_SIZE = 8 * KB
SET_NUM = 16
####

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
          return block_addr_2mb
      
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

def map_2mb_page(shm_addr, row_size, set_num):
    set_dict = {i:list() for i in range(set_num)}
    for set_index in range((2 * MB) // (row_size*set_num)):
      for j in range(set_num):
        max_time = 0
        max_index = -1
        for i in range(set_num):
          rc, arr = read_qword_time_test_ns(shm_addr+(j*row_size), shm_addr+((i+(set_index*set_num))*row_size), 1000)
          time_sum = sum(arr)
          if time_sum > max_time:
            max_time = time_sum
            max_index = i+(set_index*set_num)
        set_dict[j].append(max_index % set_num)
    return set_dict

def main():
    page_num = int(argv[1])
    print_log('try to alloc {} 2MB pages'.format(page_num))
    min_1mb_blocks = find_min_1mb_block_number()
    hugepage_arr = alloc_2mb_pages(page_num, min_1mb_blocks)
    print_log('succeed to alloc {} 2MB pages'.format(len(hugepage_arr)))
    for hugepage_addr in hugepage_arr:
      func_dict = map_2mb_page(hugepage_addr, ROW_SIZE, SET_NUM)
      func_arr = zip(*[func_dict[key] for key in func_dict])
      print_log('2MB page func:')
      for row in func_arr:
        print '{'+str(row)[1:-1]+'}'
    return OK
    
if __name__ ==  '__main__':
    exit_code = main()
    print_log('test exit code:', exit_code)
    exit(exit_code)