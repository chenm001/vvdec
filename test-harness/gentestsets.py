#!/usr/bin/env python

# Copyright (C) 2020 Min Chen <chenm003@163.com>
#
# This software may be used and distributed according to the terms of the
# GNU General Public License version 2 or any later version. See COPYING

import os
import sys
import shutil
import tempfile
import zipfile
from six.moves import urllib

from conf import my_sequences

def filter_input(filename):
    startcap = 0
    with open(filename, 'r') as f:
        for line in f:
            if line.lstrip().startswith('#'):
                continue
            if 'APPEND BITSTREAM_FILES' in line:
                startcap = 1
                if not '"' in line:
                    continue
            if startcap and '"' in line:
                yield line
            if ')' in line:
                startcap = 0

bitstream = ''
for line in filter_input('../CMakeLists.txt'):
    bitstream += line.strip().replace('"', ' ')

filename = bitstream.split()

bitstream_base = 'https://www.itu.int/wftp3/av-arch/jvet-site/bitstream_exchange/VVC/under_test/VTM-11.0'

tmpfolder = tempfile.mkdtemp(prefix='chen-tmp')
seqfolder = r'seq'
zipfolder = r'zip'

if not os.path.exists(seqfolder):
    os.makedirs(seqfolder)

if not os.path.exists(zipfolder):
    os.makedirs(zipfolder)

_file = []
_hash = []

#filename = filename[:1]
for x in filename:
    url = bitstream_base + r'/' + x
    _zip = os.path.join(zipfolder, x)
    _seq = os.path.join(seqfolder, x)

    print 'Processing ' + x
    if not os.path.exists(_zip):
        urllib.request.urlretrieve(url, _zip)

    with zipfile.ZipFile(_zip, 'r') as zip_ref:
        zip_ref.extractall(tmpfolder)

    for root, dir, files in os.walk(tmpfolder):
        for name in files:
            _fname = os.path.join(root, name)
            if '.bit' in name:
                _file.append(name)
                _bitfile = os.path.join(seqfolder, name)
                if not os.path.exists(_bitfile):
                    shutil.copyfile(_fname, _bitfile)
            elif '.yuv.md5' in name:
                with open(_fname, 'r') as f:
                    _md5 = f.readline()[:32]
                    _hash.append(_md5.lower())

    if len(_file) != len(_hash):
        raise Exception('count of items mismatch')

    _items = [(_file[i], _hash[i]) for i in range(0, len(_file))]

    with open('smoke-tests-dummy.txt', 'w') as f:
        f.write('#%-36s%-36s%s\n' % ('Sequence', 'MD5', 'Comments'))
        for item in _items:
            f.write('%-36s%-36s\n' % item)

    shutil.rmtree(tmpfolder)

