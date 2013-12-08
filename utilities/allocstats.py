#!/usr/bin/env python
#
# Copyright (C) 2013 Alex Smith
#
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#

import sys

if len(sys.argv) != 2:
    sys.stderr.write('Usage: %s <log file>\n' % (sys.argv[0]))
    sys.exit(1)

allocations = {}
f = open(sys.argv[1], 'r')
for line in f.readlines():
    line = line.strip().split(' ', 6)
    if len(line) != 7 or line[0] != 'slab:':
        continue

    if line[1] == 'allocated':
        allocations[line[2]] = [line[4], line[6]]
    elif line[1] == 'freed':
        try:
            del allocations[line[2]]
        except KeyError:
            pass

addr_width = 0
name_width = 0
for (k, v) in allocations.items():
    addr_width = max(addr_width, len(k))
    name_width = max(name_width, len(v[0]))

print "%s %s Caller" % ("Address".ljust(addr_width), "Cache".ljust(name_width))
print "%s %s ======" % ("=======".ljust(addr_width), "=====".ljust(name_width))

for (k, v) in allocations.items():
    print "%s %s %s" % (k.ljust(addr_width), v[0].ljust(name_width), v[1])
