#!/usr/bin/env python3
"""Fallback GP4 (package project) writer, used only when the toolchain's own
`create-gp4` is not available. Usage:
  make_gp4.py OUT.gp4 CONTENT_ID file1 file2 ...   (paths relative to the package root)
"""
import datetime, os, sys
from xml.sax.saxutils import quoteattr

def main():
    out, content_id, files = sys.argv[1], sys.argv[2], sys.argv[3:]
    ts = datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%d %H:%M:%S")
    dirs = {}
    for f in files:
        parts = f.split("/")[:-1]
        node = dirs
        for p in parts:
            node = node.setdefault(p, {})

    def emit(tree, indent):
        s = ""
        for name, sub in sorted(tree.items()):
            s += f'{indent}<dir targ_name={quoteattr(name)}>\n' + emit(sub, indent + "  ") + f"{indent}</dir>\n"
        return s

    x = ['<?xml version="1.0"?>', '<psproject fmt="gp4" version="1000">', "  <volume>",
         "    <volume_type>pkg_ps4_app</volume_type>", "    <volume_id>PS4VOLUME</volume_id>",
         f"    <volume_ts>{ts}</volume_ts>",
         f'    <package content_id={quoteattr(content_id)} passcode="00000000000000000000000000000000" storage_type="digital50" app_type="full" />',
         '    <chunk_info chunk_count="1" scenario_count="1">',
         '      <chunks><chunk id="0" layer_no="0" label="Chunk #0" /></chunks>',
         '      <scenarios default_id="0"><scenario id="0" type="sp" initial_chunk_count="1" label="Scenario #0">0</scenario></scenarios>',
         "    </chunk_info>", "  </volume>", '  <files img_no="0">']
    for f in files:
        x.append(f"    <file targ_path={quoteattr(f)} orig_path={quoteattr(f)} />")
    x += ["  </files>", "  <rootdir>", emit(dirs, "    ").rstrip("\n"), "  </rootdir>", "</psproject>", ""]
    with open(out, "w") as fh:
        fh.write("\n".join(x))

if __name__ == "__main__":
    main()
