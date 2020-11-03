#!/usr/bin/python
# -*- coding: utf-8 -*-

import plistlib
import biplist
import sys
import os

path = sys.argv[1]
if os.path.exists(path):
    plistData = biplist.readPlist(path)
    plistlib.writePlist(plistData, path)
else:
    print 'file not exists'
