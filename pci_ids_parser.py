import os

def load_pci_ids(path="/usr/share/misc/pci.ids"):
    ids = {}
    try:
        with open(path, "r", encoding="utf-8", errors="ignore") as f:
            for line in f:
                if line and not line.startswith("\t") and not line.startswith("#"):
                    parts = line.strip().split(" ", 1)
                    if len(parts) == 2:
                        vid = parts[0].lower()
                        name = parts[1].strip()
                        ids[vid] = name
    except Exception as e:
        print(f"Could not load pci.ids: {e}")
    return ids

