#!/usr/bin/python
# -*- coding: utf-8 -*-

import struct
from ctypes import *
from os.path import expanduser, join
import sys
import os
from datetime import datetime

__adder = CDLL(join(os.getcwd(), r'hammerscope_lib/hammer_scope.so'))

OK = 0
BAD_INDEX = -1
NOT_ALLOCATED_ROWS = -2
UNMAP_FAILED = -3
UNINIT_MEMORY_DB = -4
MAPPING_FAILED = -5
REMOVE_MAPPING_FAILED = -6
CANT_HAMMER_ROW = -7
MAX_NUMBER_OF_READS = 2700*1024
MAX_AGGR_NUM = 20

'''
struct HammerLocation
    {
        uint64_t pages_physical_addresses[MAX_AGGR_NUM]; 
        uint64_t row_index;
        uint64_t base_index;
        uint64_t n_sides;
        uint64_t dram_index;
	    uint64_t physical_address;
	    uint64_t qword_value;
        uint64_t qword_index;
	    uint64_t mod;
    };
'''

class HammerLocation(object):
    def __init__(self, pages_physical_addresses, row_index, base_index, n_sides, dram_index, physical_address, qword_value, qword_index, mod):
        self.pages_physical_addresses = list(pages_physical_addresses) 
        self.row_index = row_index 
        self.base_index = base_index
        self.n_sides = n_sides
        self.dram_index = dram_index
        self.physical_address = physical_address
        self.qword_value = qword_value
        self.qword_index = qword_index
        self.mod = mod
    
    @classmethod
    def from_c_HammerLocation(cls, hl):
        return cls(pages_physical_addresses = [int(o) for o in hl.pages_physical_addresses],
                    row_index = int(hl.row_index),
                    base_index = int(hl.base_index),
                    n_sides = int(hl.n_sides),
                    dram_index = int(hl.dram_index),
                    physical_address = int(hl.physical_address),
                    qword_value = int(hl.qword_value),
                    qword_index = int(hl.qword_index),
                    mod = int(hl.mod))

    def __str__(self):
        return 'Hammering location {}\tfound bitflip at {}\tvalue:{}'.format('\t'.join([hex(h) for h in self.pages_physical_addresses]), hex(self.physical_address), hex(self.qword_value))

    def to_c_HammerLocation(self):
        return c_HammerLocation(
                    pages_physical_addresses = (c_uint64*MAX_AGGR_NUM)(*self.pages_physical_addresses),
                    row_index = self.row_index,
                    base_index = self.base_index,
                    n_sides = self.n_sides,
                    dram_index = self.dram_index,
                    physical_address = self.physical_address,
                    qword_value = self.qword_value,
                    qword_index = self.qword_index,
                    mod = self.mod)

class c_HammerLocation(Structure):
    _fields_ = [("pages_physical_addresses", c_uint64*MAX_AGGR_NUM),
                ("row_index", c_uint64),
                ("base_index", c_uint64),
                ("n_sides", c_uint64),
                ("dram_index", c_uint64),
                ("physical_address", c_uint64),
                ("qword_value", c_uint64),
                ("qword_index", c_uint64),
                ("mod", c_uint64)]

class c_TimeResult(Structure):
    _fields_ = [("start_time", c_uint64),
                ("end_time", c_uint64),
                ("flip", c_uint64),
                ("value", c_uint64)]


'''
╰─$ nm -D hammer_scope.so
0000000000006d56 T _Z10free_blockmm
0000000000006b6e T _Z10InitDBFuncmm
00000000000069a9 T _Z11GetHammerDBPh
00000000000069d6 T _Z11GetTotalRowv
0000000000006436 T _Z12MapSharedMemmmPmS_
00000000000068e2 T _Z12ReadTimeTestmmPm
00000000000069c1 T _Z13ClearHammerDBv
000000000000641f T _Z13GetTestNumbermmPm
00000000000067da T _Z14ReadTimeTestNSmmmPmS_
0000000000006c17 T _Z15alloc_1MB_blockPm
0000000000006c8f T _Z15alloc_2MB_blockPm
00000000000069e8 T _Z15GetAllocatedRowv
0000000000006994 T _Z15GetHammerDBSizev
000000000000642c T _Z15WriteTestNumbermmm
0000000000006aaa T _Z16MapSharedHugeMemdPmS_
000000000000802e T _Z17TestHammerBitflipN11HammerScope14HammerLocationEmmmPNS_10TimeResultE
0000000000006ba8 T _Z19ReadQWORDTimeTestNSmmmPm
0000000000007b5c T _Z20TestHammerBinBitflipN11HammerScope14HammerLocationEmmmPNS_10TimeResultE
0000000000006b8b T _Z23InitDBUserAllocHugepagePmm
0000000000006d85 T _Z24TestHammerBinBitflipUTRRN11HammerScope14HammerLocationEmmmmPNS_10TimeResultE
0000000000007287 T _Z26HammerRowAllReachablePagesmmmm
0000000000006b47 T _Z6InitDBmmmmmm
00000000000069fd T _Z9MapMemorydm
00000000000063dc T _Z9TestIndexm
'''

def test_hammer_bin_bitflip_utrr(hammer_location, vict_index, number_of_reads, cycle_time, bin_index):
    _test_hammer_bin_bitflip = __adder._Z24TestHammerBinBitflipUTRRN11HammerScope14HammerLocationEmmmmPNS_10TimeResultE
    _test_hammer_bin_bitflip.argtypes = (c_HammerLocation, c_uint64, c_uint64, c_uint64, c_uint64, POINTER(c_TimeResult))
    _test_hammer_bin_bitflip.restype = c_int64
    time_result = c_TimeResult()
    rc = _test_hammer_bin_bitflip(hammer_location.to_c_HammerLocation(), vict_index, number_of_reads, cycle_time, bin_index, byref(time_result))
    return rc, time_result


def test_hammer_bin_bitflip(hammer_location, number_of_reads, cycle_time, bin_index):
    _test_hammer_bin_bitflip = __adder._Z20TestHammerBinBitflipN11HammerScope14HammerLocationEmmmPNS_10TimeResultE
    _test_hammer_bin_bitflip.argtypes = (c_HammerLocation, c_uint64, c_uint64, c_uint64, POINTER(c_TimeResult))
    _test_hammer_bin_bitflip.restype = c_int64
    time_result = c_TimeResult()
    rc = _test_hammer_bin_bitflip(hammer_location.to_c_HammerLocation(), number_of_reads, cycle_time, bin_index, byref(time_result))
    return rc, time_result

def test_hammer_bitflip(hammer_location, number_of_reads, threshold, idle):
    _test_hammer_bitflip = __adder._Z17TestHammerBitflipN11HammerScope14HammerLocationEmmmPNS_10TimeResultE
    _test_hammer_bitflip.argtypes = (c_HammerLocation, c_uint64, c_uint64, c_uint64, POINTER(c_TimeResult))
    _test_hammer_bitflip.restype = c_int64
    time_result = c_TimeResult()
    rc = _test_hammer_bitflip(hammer_location.to_c_HammerLocation(), number_of_reads, threshold, idle, byref(time_result))
    return rc, time_result

def hammer_row_all_reachable_pages(row_index, n_sides, threshold, idle):
    _row_hammer_all_reachable_pages = __adder._Z26HammerRowAllReachablePagesmmmm
    _row_hammer_all_reachable_pages.argtypes = (c_uint64, c_uint64, c_uint64, c_uint64)
    _row_hammer_all_reachable_pages.restype = c_int64
    return _row_hammer_all_reachable_pages(row_index, n_sides, threshold, idle)


def get_test_number(shm_address, shm_offset):
    _get_test_number = __adder._Z13GetTestNumbermmPm
    _get_test_number.argtypes = (c_uint64, c_uint64, POINTER(c_uint64))
    _get_test_number.restype = c_int64
    test_num = (c_uint64)()
    rc = _get_test_number(shm_address, shm_offset, byref(test_num))
    res = int(test_num.value)
    return rc, res


def write_test_number(shm_address, shm_offset, test_number):
    _write_test_number = __adder._Z15WriteTestNumbermmm
    _write_test_number.argtypes = (c_uint64, c_uint64, c_uint64)
    _write_test_number.restype = c_int64
    rc = _write_test_number(shm_address, shm_offset, test_number)
    return rc


def read_time_test(row_index, number_of_reads):
    _read_time_test = __adder._Z12ReadTimeTestmmPm
    _read_time_test.argtypes = (c_uint64, c_uint64, POINTER(c_uint64))
    data_block = (c_uint64 * number_of_reads)()
    data_block_pointer = cast(addressof(data_block), POINTER(c_uint64))
    rc = _read_time_test(row_index, number_of_reads, data_block_pointer)
    return rc, data_block

def read_time_test_ns(row_index, number_of_rounds, number_of_round_reads):
    _read_time_test_ns = __adder._Z14ReadTimeTestNSmmmPmS_
    _read_time_test_ns.argtypes = (c_uint64, c_uint64, c_uint64, POINTER(c_uint64))
    data_block = (c_uint64 * number_of_rounds)()
    data_block_pointer = cast(addressof(data_block), POINTER(c_uint64))
    time_block = (c_uint64 * number_of_rounds)()
    time_block_pointer = cast(addressof(time_block), POINTER(c_uint64))
    rc = _read_time_test_ns(row_index, number_of_rounds, number_of_round_reads, data_block_pointer, time_block_pointer)
    return rc, data_block, time_block

def read_qword_time_test_ns(address1, address2, number_of_reads):
    _read_time_test = __adder._Z19ReadQWORDTimeTestNSmmmPm
    _read_time_test.argtypes = (c_uint64, c_uint64, c_uint64, POINTER(c_uint64))
    data_block = (c_uint64 * number_of_reads)()
    data_block_pointer = cast(addressof(data_block), POINTER(c_uint64))
    rc = _read_time_test(address1, address2, number_of_reads, data_block_pointer)
    return rc, [i for i in data_block]

def get_allocated_row():
    _get_allocated_row = __adder._Z15GetAllocatedRowv
    _get_allocated_row.restype = c_uint64
    return _get_allocated_row()


def map_memory(percent, cfg):
    _map_memory = __adder._Z9MapMemorydm
    _map_memory.argtypes = (c_double, c_uint64)
    _map_memory.restype = c_int
    return _map_memory(percent, cfg)

def map_shared_huge_mem(percent):
    _map_shared_huge_mem = __adder._Z16MapSharedHugeMemdPmS_
    _map_shared_huge_mem.argtypes = (c_double, POINTER(c_uint64), POINTER(c_uint64))
    _map_shared_huge_mem.restype = c_uint64
    huge_fd = (c_uint64)()
    shm_address = (c_uint64)()
    rc = _map_shared_huge_mem(percent, byref(huge_fd), byref(shm_address))
    return rc, huge_fd.value, shm_address.value

def init_db(cfg, hugetlb_fd, shm_addr, mapping_size, pid, shm_base_addr):
    _init_db = __adder._Z6InitDBmmmmmm
    _init_db.argtypes = (c_uint64, c_uint64, c_uint64, c_uint64, c_uint64, c_uint64)
    _init_db.restype = c_uint64
    rc = _init_db(cfg, hugetlb_fd, shm_addr, mapping_size, pid, shm_base_addr)
    return rc

def init_db_func(shm_addr, mapping_size):
    _init_db = __adder._Z10InitDBFuncmm
    _init_db.argtypes = (c_uint64, c_uint64)
    _init_db.restype = c_uint64
    rc = _init_db(shm_addr, mapping_size)
    return rc
    
def init_db_user_alloc_hugepage(hugepage_addr_arr, arr_len):
    addr_lst = (c_uint64*arr_len)(*hugepage_addr_arr)
    _init_db_user_alloc_hugepage = __adder._Z23InitDBUserAllocHugepagePmm
    _init_db_user_alloc_hugepage.argtypes = (POINTER(c_uint64), c_uint64)
    _init_db_user_alloc_hugepage.restype = c_uint64
    rc = _init_db_user_alloc_hugepage(addr_lst, arr_len)
    return rc

def map_shared_mem(dram_index, page_num):
    _map_shared_mem = __adder._Z12MapSharedMemmmPmS_
    _map_shared_mem.argtypes = (c_uint64, c_uint64, POINTER(c_uint64), POINTER(c_uint64))
    _map_shared_mem.restype = c_uint64
    fd = (c_uint64)()
    shm_address = (c_uint64)()
    rc = _map_shared_mem(dram_index, page_num, byref(fd), byref(shm_address))
    return rc, fd.value, shm_address.value

def alloc_1mb_block():
    _alloc_1mb_block = __adder._Z15alloc_1MB_blockPm
    _alloc_1mb_block.argtypes = (POINTER(c_uint64),)
    _alloc_1mb_block.restype = c_uint64
    block_address = (c_uint64)()
    rc = _alloc_1mb_block(byref(block_address))
    return rc, block_address.value
    
def alloc_2mb_block():
    _alloc_2mb_block = __adder._Z15alloc_2MB_blockPm
    _alloc_2mb_block.argtypes = (POINTER(c_uint64),)
    _alloc_2mb_block.restype = c_uint64
    block_address = (c_uint64)()
    rc = _alloc_2mb_block(byref(block_address))
    return rc, block_address.value
    
def free_block(block_address, alloc_type):
    _free_block = __adder._Z10free_blockmm
    _free_block.argtypes = (c_uint64, c_uint64)
    _free_block.restype = c_uint64
    rc = _free_block(block_address, alloc_type)
    return rc

def test_row_index(row_index):
    _test_row_index = __adder._Z9TestIndexm
    _test_row_index.argtypes = (c_uint64,)
    _test_row_index.restype = c_int64
    return _test_row_index(row_index)

def get_total_row():
    _get_total_row = __adder._Z11GetTotalRowv
    _get_total_row.restype = c_uint64
    return _get_total_row()

def get_hammer_db():
    _get_hammer_db_size = __adder._Z15GetHammerDBSizev
    _get_hammer_db_size.restype = c_uint64
    db_size = _get_hammer_db_size()
    
    hammer_location_list = (c_HammerLocation*db_size)()
    hammer_location_list_pointer = (c_char * (sizeof(c_HammerLocation)*db_size)).from_buffer(hammer_location_list)

    _get_hammer_db = __adder._Z11GetHammerDBPh
    _get_hammer_db.argtypes = (c_char_p, )
    _get_hammer_db(hammer_location_list_pointer)

    res = [HammerLocation.from_c_HammerLocation(hl) for hl in hammer_location_list]
    
    _clear_hammer_db = __adder._Z13ClearHammerDBv
    _clear_hammer_db()

    del hammer_location_list
    return res

def clear_hammer_db():
    _clear_hammer_db = __adder._Z13ClearHammerDBv
    _clear_hammer_db()

def print_log(*args, **kwargs):
    now = datetime.now()
    current_time = now.strftime("%H:%M:%S")
    sys.stdout.write('[{}] '.format(current_time))
    print ' '.join([str(a) for a in args])
