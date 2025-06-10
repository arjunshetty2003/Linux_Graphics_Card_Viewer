import re

def load_pci_ids(path="/usr/share/misc/pci.ids"):
    ids = {}
    with open(path) as f:
        for line in f:
            m = re.match(r"^([0-9a-f]{4})\s+(.+)$", line, re.I)
            if m:
                ids[m.group(1)] = m.group(2)
    return ids
