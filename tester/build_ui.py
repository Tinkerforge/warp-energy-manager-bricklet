#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import sys
if (sys.hexversion & 0xFF000000) != 0x03000000:
    print('Python 3.x required')
    sys.exit(1)

import os
import subprocess

basedir = os.path.dirname(os.path.realpath(__file__))

# build .ui files and call build_extra.py scripts
for dirpath, dirnames, filenames in os.walk(basedir):
    if 'build_extra.py' in filenames:
        print('calling build_extra.py in ' + os.path.relpath(dirpath, basedir))

        subprocess.check_call([sys.executable, 'build_extra.py'], cwd=dirpath)

    if os.path.basename(dirpath) != 'ui':
        continue

    for filename in filenames:
        name, ext = os.path.splitext(filename)

        if ext != '.ui':
            continue

        out_file = os.path.normpath(os.path.join(dirpath, "..", "ui_" + name + ".py"))
        in_file = os.path.join(dirpath, filename)

        # Arch Linux complains, if a built package contains references to $SRCDIR.
        # (e.g. the directory in which the package was built)
        # Thus use the relative path here, as pyuic writes the in_file path it is called with into the out file.
        in_file = os.path.relpath(in_file, basedir)

        print('building ' + in_file)

        subprocess.check_call([sys.executable, 'pyuic5-fixed.py', '-o', out_file, in_file], cwd=basedir)
