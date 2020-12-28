#!/usr/bin/env python

# Copyright (C) 2020 Min Chen <chenm003@163.com>
#
# This software may be used and distributed according to the terms of the
# GNU General Public License version 2 or any later version. See COPYING

import os
import sys
import shutil
from subprocess import Popen, PIPE
import utils

# setup will call sys.exit() if it determines the tests are unable to continue
utils.setup(sys.argv, 'smoke-tests.txt')

from conf import my_builds
from utils import logger

try:
    from conf import decoder_binary_name
except ImportError, e:
    print 'failed to import decoder_binary_name'
    decoder_binary_name = 'vvdecapp'

utils.buildall()

if logger.errors:
    sys.exit(1)

extras = []

if decoder_binary_name == 'ffmpeg':
    extras = []

try:
    tests = utils.parsetestfile()
    logger.settestcount(len(my_builds.keys()) * len(tests))

    for key in my_builds:
        #logger.setbuild(key)
        for (seq, md5, cmt) in tests:
            utils.runtest(key, seq, md5, extras)

        # here it applies specific patch and shares libraries

except KeyboardInterrupt:
    print 'Caught CTRL+C, exiting'

finally:
    pass

