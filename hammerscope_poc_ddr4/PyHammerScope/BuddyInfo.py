import os
import re
from collections import defaultdict

#Taken From http://andorian.blogspot.com/2014/03/making-sense-of-procbuddyinfo.html
class BuddyInfo(object):
    """BuddyInfo DAO"""
    def __init__(self):
        super(BuddyInfo, self).__init__()
        self.buddyinfo = self.load_buddyinfo()

    def parse_line(self, line):
        line = line.strip()
        parsed_line = re.match("Node\s+(?P<numa_node>\d+).*zone\s+(?P<zone>\w+)\s+(?P<nr_free>.*)", line).groupdict()
        return parsed_line

    def read_buddyinfo(self):
        buddyhash = defaultdict(list)
        with open("/proc/buddyinfo") as f:
          buddyinfo = f.readlines()
          f.close()
        for line in map(self.parse_line, buddyinfo):
            numa_node =  int(line["numa_node"])
            zone = line["zone"]
            free_fragments = map(int, line["nr_free"].split())
            max_order = len(free_fragments)
            fragment_sizes = self.get_order_sizes(max_order)
            usage_in_bytes =  [block[0] * block[1] for block in zip(free_fragments, fragment_sizes)]
            buddyhash[numa_node].append({
                "zone": zone,
                "nr_free": free_fragments,
                "sz_fragment": fragment_sizes,
                "usage": usage_in_bytes })
        return buddyhash

    def load_buddyinfo(self):
        buddyhash = self.read_buddyinfo()
        return buddyhash

    def page_size(self):
        return os.sysconf("SC_PAGE_SIZE")

    def get_order_sizes(self, max_order):
        return [self.page_size() * 2**order for order in range(0, max_order)]

    def __str__(self):
        ret_string = ""
        width = 20
        for node in self.buddyinfo:
            ret_string += "Node: %s\n" % node
            for zoneinfo in self.buddyinfo.get(node):
                ret_string += " Zone: %s\n" % zoneinfo.get("zone")
                ret_string += " Free KiB in zone: %.2f\n" % (sum(zoneinfo.get("usage")) / (1024.0))
                ret_string += '\t{0:{align}{width}} {1:{align}{width}} {2:{align}{width}}\n'.format(
                        "Fragment size", "Free fragments", "Total available KiB",
                        width=width,
                        align="<")
                for idx in range(len(zoneinfo.get("sz_fragment"))):
                    ret_string += '\t{order:{align}{width}} {nr:{align}{width}} {usage:{align}{width}}\n'.format(
                        width=width,
                        align="<",
                        order = zoneinfo.get("sz_fragment")[idx],
                        nr = zoneinfo.get("nr_free")[idx],
                        usage = zoneinfo.get("usage")[idx] / 1024.0)

        return ret_string
