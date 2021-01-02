# Copyright (C) 2020 Min Chen <chenm003@163.com>
#
# This software may be used and distributed according to the terms of the
# GNU General Public License version 2 or any later version. See COPYING

import atexit
import datetime
import filecmp
import hashlib
import os
import platform
import random
import shutil
import shlex
import sys
import tempfile
import time
import urllib
import re
from subprocess import Popen, PIPE
from distutils.spawn import find_executable
import multiprocessing

run_make  = True     # run cmake and make/msbuild
rebuild   = False    # delete build folders prior to build
test_file = None     # filename or full path of file containing test cases
skip_string = None   # filter tests - all except those matching this string
only_string = None   # filter tests - only those matching this string
only_yuv = False     # verify output with YUV file
run_adb = False      # verify through adb interface

android_ndk = None
logger = None
buildObj = {}



try:
    from conf import my_machine_name, my_machine_desc
    from conf import my_vvdec_source, my_vvdec_decoder, my_sequences
    from conf import my_pastebin_key, my_tempfolder, my_progress
    from conf import my_builds, my_make_flags, option_strings, version_control
    from conf import my_adb_cmd, my_adb_sequences, my_adb_decoder, my_adb_tmpfolder

    # support ~/repos/vvdec syntax
    my_sequences = os.path.expanduser(my_sequences)
    my_vvdec_source = os.path.expanduser(my_vvdec_source)
    my_vvdec_decoder = os.path.expanduser(my_vvdec_decoder)
    my_tempfolder = os.path.expanduser(my_tempfolder)

    # backward compatibility check
    for key in my_builds:
        opts = my_builds[key][4]
        if 'mingw' in opts:
            print '** `mingw` keyword for MinGW path is deprecated, use PATH'
            opts['PATH'] = opts['mingw']
except ImportError, e:
    print e
    print 'Copy conf.py.example to conf.py and edit the file as necessary'
    sys.exit(1)

try:
    from conf import my_shellpath
except ImportError, e:
    my_shellpath = ''

try:
    from conf import decoder_binary_name
except ImportError, e:
    decoder_binary_name = 'vvdecapp'


osname = platform.system()
if osname == 'Windows':
    exe_ext         = '.exe'
    if my_shellpath:
        dll_ext         = '.a'
    else:
        dll_ext         = '.dll'
    static_lib      = 'vvdec-static.lib'
    extra_link_flag = ''
else:
    exe_ext         = ''
    static_lib      = 'libvvdec.a'
    extra_link_flag = r'-DEXTRA_LINK_FLAGS=-L.'
    if osname == 'Darwin':
        dll_ext     = '.dylib'
    elif osname == 'Linux':
        dll_ext     = '.so'


if os.name == 'nt':

    # LOL Windows
    # Use two threads to poll stdout and stderr and write into queues
    # http://stackoverflow.com/questions/375427/non-blocking-read-on-a-subprocess-pipe-in-python

    def enqueue_output(fd, q):
        try: 
            while True:
                line = fd.readline()
                if line:
                    q.put(line)
                else:
                    return
        except:
            fd.close()

    def async_poll_process(proc, fulloutput):
        from Queue import Queue, Empty
        from threading import Thread
        qout = Queue()
        tout = Thread(target=enqueue_output, args=(proc.stdout, qout))
        tout.start()

        qerr = Queue()
        terr = Thread(target=enqueue_output, args=(proc.stderr, qerr))
        terr.start()

        out = []
        errors = ''
        exiting = False
        output = ''
        while True:
            # note that this doesn't guaruntee we get the stdout and stderr
            # lines in the intended order, but they should be close
            try:
                while not qout.empty():
                    line = qout.get()
                    if fulloutput: output += line
                    if my_progress: print line,
                    out.append(line)
            except Empty:
                pass

            if not qerr.empty():
                try:
                    while not qerr.empty():
                        line = qerr.get()
                        if fulloutput: output += line
                        if my_progress: print line,
                        if 'PIE' not in line:
                            errors += ''.join(out[-3:])
                            out = []
                            errors += line
                except Empty:
                    pass

            if proc.poll() != None and not exiting:
                tout.join()
                terr.join()
                exiting = True
            elif exiting and qerr.empty() and qout.empty():
                break

        if proc.returncode and not errors:
            errors = ''.join(out[-10:])
        if proc.returncode == -11:
            errors += 'SIGSEGV\n'
        elif proc.returncode == -10:
            errors += 'SIGBUS\n'
        elif proc.returncode == -6:
            errors += 'SIGABRT\n'
        elif proc.returncode == -4:
            errors += 'SIGILL\n'
        elif proc.returncode:
            errors += 'return code %d\n' % proc.returncode
        if fulloutput:
            return output, errors
        else:
            return errors

else:

    # POSIX systems have select()

    import select

    def async_poll_process(proc, fulloutput):
        out = []
        errors = ''
        output = ''
        exiting = False

        # poll stdout and stderr file handles so we get errors in the context
        # of the stdout compile progress reports
        while True:
            fds = [proc.stdout.fileno(), proc.stderr.fileno()]
            ret = select.select(fds, [], [])

            empty = True
            for fd in ret[0]:
                if fd == proc.stdout.fileno():
                    line = proc.stdout.readline()
                    if line:
                        empty = False
                        if fulloutput: output += line
                        if my_progress: print line,
                        out.append(line)
                if fd == proc.stderr.fileno():
                    line = proc.stderr.readline()
                    if line:
                        empty = False
                        if fulloutput: output += line
                        if my_progress: print line,
                        if 'PIE' not in line:
                            errors += ''.join(out[-3:])
                            out = []
                            errors += line
            if proc.poll() != None and not exiting:
                exiting = True
            elif exiting and empty:
                break

        if proc.returncode and not errors:
            errors = ''.join(out[-10:])
        if proc.returncode == -11:
            errors += 'SIGSEGV\n'
        elif proc.returncode == -10:
            errors += 'SIGBUS\n'
        elif proc.returncode == -6:
            errors += 'SIGABRT\n'
        elif proc.returncode == -4:
            errors += 'SIGILL\n'
        elif proc.returncode:
            errors += 'return code %d\n' % proc.returncode
        if fulloutput:
            return output, errors
        else:
            return errors

def gitversion(reporoot):
    out, err = Popen(['git', 'rev-parse', 'HEAD'], stdout=PIPE, stderr=PIPE, cwd=reporoot).communicate()
    if err:
        raise Exception('Unable to determine source version: ' + err)
    # note, if the ID ends with '+' it means the user's repository has
    # uncommitted changes. We will never want to save golden outputs from these
    # repositories.
    return out[:-1] # strip line feed

def gitrevisioninfo(rev):
    addstatus = False
    out, err = Popen(['git', 'diff-index', '--name-status', '--exit-code', 'HEAD'], stdout=PIPE, stderr=PIPE, cwd=my_vvdec_source).communicate()
    if out:
        addstatus = True
    out, err = Popen(['git' ,'show', '-s', rev], stdout=PIPE, stderr=PIPE, cwd=my_vvdec_source).communicate()
    if err:
        raise Exception('Unable to determine revision info: ' + err)

    if addstatus:
        out_changes, err = Popen(['git' ,'show', '-s', rev], stdout=PIPE, stderr=PIPE, cwd=my_vvdec_source).communicate()
        out += 'Uncommitted changes in the working directory:\n' + out_changes
    return out

def getcommits():
    fname = 'output-changing-commits.txt'
    hashlen = 40

    def testrev(lines):
        for line in lines[::-1]:
            if len(line) < hashlen or line[0] == '#': continue
            rev = line[:hashlen]
            out, err = Popen(['git' ,'show', '-s', rev], stdout=PIPE, stderr=PIPE, cwd=my_vvdec_source).communicate()
            if not ': unknown revision' in err:
                return lines
            else:
                return open(os.path.abspath(os.path.join(my_vvdec_source, 'test', fname))).readlines()

    out = Popen(['git', 'diff', fname], stdout=PIPE).communicate()[0]

    if 'M' in out or 'diff' in out:
        if my_local_changers:
            print 'local %s is modified, disabling download' % fname
            l = testrev(open(fname).readlines())
            return l
        else:
            print 'changes in %s ignored, my_local_changers is false' % fname
    l = testrev(open(fname).readlines())
    return l

def cmake(key, generator, buildfolder, cmakeopts, **opts):
    # buildfolder is the relative path to build folder
    logger.settitle('cmake ' + buildfolder)

    cmds = ['cmake', '-Wno-dev', os.path.abspath(my_vvdec_source)]

    if generator:
        cmds.append('-G')
        cmds.append(generator)
        if 'CFLAGS' in opts:
            cmds.append('-DCMAKE_C_COMPILER_ARG1=' + opts['CFLAGS'])
            cmds.append('-DCMAKE_CXX_COMPILER_ARG1=' + opts['CFLAGS'])
        for opt in opts:
            if '-' in opt:
                cmds.append('%s %s' % (opt, opts[opt]))
            else:
                cmds.append('-D%s=%s' % (opt, opts[opt]))

    cmds.extend(cmakeopts)

    env = os.environ.copy()
    if generator:
        if 'CC' in opts:
            env['CC'] = opts['CC']
        if 'CXX' in opts:
            env['CXX'] = opts['CXX']
        if 'LD_LIBRARY_PATH' in opts:
            env['LD_LIBRARY_PATH'] = opts['LD_LIBRARY_PATH']
        if 'PKG_CONFIG_PATH' in opts:
            env['PKG_CONFIG_PATH'] = opts['PKG_CONFIG_PATH']

    # note that it is not enough to insert the path into the subprocess
    # environment; it must be in the system PATH in case the compiler
    # spawns subprocesses of its own that take the system PATH (cough, mingw)
    origpath = os.environ['PATH']
    if 'PATH' in opts:
        os.environ['PATH'] += os.pathsep + opts['PATH']
        env['PATH'] += os.pathsep + opts['PATH']

    if (osname == 'Windows' and not my_shellpath):
        proc = Popen(cmds, stdout=PIPE, stderr=PIPE, cwd=buildfolder, env=env)
    else:
        cmds = [my_shellpath, './configure']
        cmds.append(' '.join(cmakeopts))
        proc = Popen(' '.join(cmds), cwd=my_vvdec_source, stdout=PIPE, stderr=PIPE, env=env, shell=True)

    out, err = proc.communicate()
    os.environ['PATH'] = origpath
    return out, err

vcvars = ('include', 'lib', 'mssdk', 'path', 'regkeypath', 'sdksetupdir',
          'sdktools', 'targetos', 'vcinstalldir', 'vcroot', 'vsregkeypath')

def get_sdkenv(vcpath, arch):
    '''extract environment vars set by vcvarsall for compiler target'''

    vcvarsall = os.path.abspath(os.path.join(vcpath, 'vcvarsall.bat'))
    if not os.path.exists(vcvarsall):
        raise Exception(vcvarsall + ' not found')

    out, err = Popen(r'cmd /e:on /v:on /c call "%s" %s && set' % (vcvarsall, arch), shell=False, stdout=PIPE, stderr=PIPE).communicate()
    newenv = {}
    for line in out.splitlines():
        if '=' in line:
            k, v = line.split('=', 1)
            if k.lower() in vcvars:
                newenv[k.upper()] = v
    return newenv

def gmake(key, buildfolder, generator, **opts):
    logger.settitle('make ' + buildfolder)
    if 'MinGW' in generator:
        cmds = ['mingw32-make']
    elif 'ndk' in key:
        cmds = [android_make]
    else:
        cmds = ['make']

    if my_make_flags:
        cmds.extend(my_make_flags)

    origpath = os.environ['PATH']
    if 'PATH' in opts:
        os.environ['PATH'] += os.pathsep + opts['PATH']
    p = Popen(cmds, stdout=PIPE, stderr=PIPE, cwd=buildfolder)
    errors = async_poll_process(p, False)
    os.environ['PATH'] = origpath
    return errors

def msbuild(buildkey, buildfolder, generator, cmakeopts):
    '''Build visual studio solution using specified compiler'''
    logger.settitle('msbuild ' + buildfolder)
    if os.name != 'nt':
        raise Exception('Visual Studio builds only supported on Windows')

    vcpath = ''
    vcbasepath = ''

    vcver = re.findall(r'Visual Studio\s+(\d+)\s+.*', generator)[0]

    # Look for Visual Studio install location within the registry
    if vcver >= 15:
        import subprocess
        out = subprocess.check_output([os.path.join(os.environ['ProgramFiles(x86)'], 'Microsoft Visual Studio', 'Installer', 'vswhere.exe'), '-utf8', '-property', 'installationPath']).decode('utf-8')
        vcbasepath = out.rstrip()
        vcpath = os.path.abspath(vcbasepath + r'\VC\Auxiliary\Build')
    else:
        key = r'SOFTWARE\Wow6432Node\Microsoft\VisualStudio'
        if '12' in generator:
            key += r'\12.0'
        elif '11' in generator:
            key += r'\11.0'
        elif '10' in generator:
            key += r'\10.0'
        elif '9' in generator:
            key += r'\9.0'
        else:
            raise Exception('Unsupported VC version')

        import _winreg
        win32key = 'SOFTWARE' + key[20:] # trim out Wow6432Node\
        for k in (key, win32key):
            try:
                hkey = _winreg.OpenKey(_winreg.HKEY_LOCAL_MACHINE, k)
                pfx = _winreg.QueryValueEx(hkey, 'InstallDir')[0]
                if pfx and os.path.exists(pfx):
                    vcpath = os.path.abspath(pfx + r'\..\..\VC')
                    break;
            except (WindowsError, EnvironmentError), e:
                pass

    if not vcpath:
        raise Exception('msbuild not found or is invalid')

    if 'Win64' in generator:
        arch = 'x86_amd64'
    else:
        arch = 'x86'

    sdkenv = get_sdkenv(vcpath, arch)
    env = os.environ.copy()
    env.update(sdkenv)

    build = buildObj[buildkey]
    target = ''.join(['/p:Configuration=', build.target])

    # use the newest MSBuild installed
    for f in (os.path.join(vcbasepath, r'MSBuild\15.0\Bin\MSBuild.exe'),
              os.path.join(vcbasepath, r'MSBuild\Current\Bin\amd64\MSBuild.exe'),
              os.path.join(vcbasepath, r'MSBuild\Current\Bin\MSBuild.exe'),
              os.path.join(os.environ["ProgramFiles(x86)"], r'MSBuild\15.0\Bin\MSBuild.exe'),
              r'C:\Windows\Microsoft.NET\Framework\v4.0.30319\MSBuild.exe',
              r'C:\Windows\Microsoft.NET\Framework\v3.5\MSBuild.exe'):
        if os.path.exists(f):
            msbuild = f
            break
    else:
        msbuild = find_executable('msbuild')
        if not msbuild:
            raise Exception('Unable to find msbuild.exe')

    p = Popen([msbuild, '/clp:disableconsolecolor', target, decoder_binary_name[:-3] + '.sln'],
              stdout=PIPE, stderr=PIPE, cwd=buildfolder, env=env)
    out, err = async_poll_process(p, True)
    if not err:
        warnings = []
        for line in out.splitlines(True):
            if 'MSBUILD : warning MSB' in line: # vc9 is a mess
                continue
            if 'warning MSB' in line or 'warning LNK' in line: # vc15 is a mess
                continue
            if 'warning' in line:
                warnings.append(line.strip())
                logger.write(line.strip())
        if warnings:
            err = '\n'.join(warnings)
    return err

class Build():
    def __init__(self, key, *args):
        self.folder, self.group, self.gen, self.cmakeopts, self.opts = args
        co = self.cmakeopts.split()

        if 'Visual Studio' in self.gen or 'ndk' in key:
            if 'debug' in co:
                self.target = 'Debug'
            elif 'reldeb' in co:
                self.target = 'RelWithDebInfo'
            else:
                self.target = 'Release'
            self.exe = os.path.abspath(os.path.join(r'..\bin', self.target + '-static', decoder_binary_name + ('' if 'ndk' in key else exe_ext)))
        else:
            # TODO: Need fix this path
            self.target = ''
            self.exe = os.path.abspath(os.path.join(decoder_binary_name, self.folder, 'default', decoder_binary_name + exe_ext))

    def cmakeoptions(self, key, cmakeopts, prof):
        for o in self.cmakeopts.split():
            if o in option_strings:
                for tok in option_strings[o].split():
                    cmakeopts.append(tok)
            else:
                logger.write('Unknown cmake option', o)

        if 'ndk' in key:
            global android_ndk, android_make
            android_ndk = os.environ['ANDROID_NDK']
            android_ndk.replace(os.path.sep, '/')
            android_make = 'make'
            # TODO: Support Linux
            if osname == 'Windows':
                android_make = os.path.join(android_ndk, 'prebuilt/windows-x86_64/bin') + '/make'  + exe_ext
                cmakeopts.append('-DCMAKE_MAKE_PROGRAM=' + android_make)
            cmakeopts.append('-DCMAKE_TOOLCHAIN_FILE=%s/build/cmake/android.toolchain.cmake' % android_ndk)
        elif 'Makefiles' not in self.gen:
            pass # our cmake script does not support PGO for MSVC yet
        elif prof is 'generate':
            cmakeopts.append('-DFPROFILE_GENERATE=ON')
            cmakeopts.append('-DFPROFILE_USE=OFF')
        elif prof is 'use':
            cmakeopts.append('-DFPROFILE_GENERATE=OFF')
            cmakeopts.append('-DFPROFILE_USE=ON')
        #cmakeopts.append('-DCMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE=%s' % (self.exe))
        cmakeopts.append('-DBUILD_SHARED_LIBS=OFF')

        # force the default of release build if not already specified
        if '-DCMAKE_BUILD_TYPE=' not in ' '.join(cmakeopts):
            cmakeopts.append('-DCMAKE_BUILD_TYPE=Release')

        cmakeopts.append('--no-warn-unused-cli')

    def cmake_build(self, key, cmakeopts, buildfolder):
        cout, cerr = cmake(key, self.gen, buildfolder, cmakeopts, **self.opts)
        empty = True
        if cerr:
            prefix = 'cmake errors reported for %s:: ' % key
            errors = cout + cerr
            #_test.failuretype = 'cmake errors'
        elif 'Makefiles' in self.gen:
            errors = gmake(key, buildfolder, self.gen, **self.opts)
            prefix = 'make warnings or errors reported for %s:: ' % key
            #_test.failuretype = 'make warnings or errors'
        elif 'Visual Studio' in self.gen:
            errors = msbuild(key, buildfolder, self.gen, cmakeopts)
            prefix = 'msbuild warnings or errors reported for %s:: ' % key
            #_test.failuretype = 'msbuild warnings or errors'
        else:
            raise NotImplemented()

        if errors:
            logger.writeerr(prefix + '\n' + errors + '\n')

def parsetestfile():
    global test_file, test_hash
    missing = set()
    tests = []
    for line in open(test_file).readlines():
        line = line.strip()
        if len(line) < 3 or line[0] == '#':
            continue

        v = line.split();
        seq = v[0].strip()
        md5 = v[1].strip()
        cmt = v[2].strip() if len(v) > 2 else ''

        if not run_adb and not os.path.exists(os.path.join(my_sequences, seq)):
            if seq not in missing:
                logger.write('Ignoring missing sequence', seq)
                missing.add(seq)
            continue
        tests.append((seq, md5, cmt))
    return tests

class Logger():
    def __init__(self, testfile):
        nowdate = datetime.datetime.now().strftime('log-%y%m%d%H%M')
        self.testname = os.path.splitext(os.path.basename(testfile))[0]
        self.logfname = '%s-%s.txt' % (nowdate, self.testname)
        print 'Logging test results to %s\n' % self.logfname
        self.start_time = datetime.datetime.now()
        self.errors = 0
        self.testcount = 0
        self.totaltests = 0
        self.newoutputs = {}
        self.logfp = open(os.path.join(self.logfname), 'wb')
        self.header  = '\nsystem:      %s\n' % my_machine_name
        self.header += 'hardware:    %s\n' % my_machine_desc
        self.header += '%s\n' % gitrevisioninfo(gitversion(my_vvdec_source))
        htmltable = "style='font-size:15px; font-family: Times New Roman'"
        self.tableheader = r'<tr><th rowspan = "2">{0}</th><th rowspan = "2">{1}</th><th rowspan = "2">{2}</th><th rowspan = "2">{3}</th><th colspan = "3">{4}</th><th rowspan = "2">{5}</th><th colspan = "3">{6}</th></tr>'.format('Failure Type','Failure Commands','Build','Previous Good Revision','Previous Values','Current Revision','Current Values',)
        self.tableheader2 = r'<tr> <th>Bitrate</th><th>SSIM</th><th>PSNR</th> <th>Bitrate</th><th>SSIM</th><th>PSNR</th> </tr>'
        self.table = ['<htm><body ' + htmltable +' ><table border="1">']
        self.table.append(self.tableheader)
        self.table.append(self.tableheader2)
        self.logfp.write(self.header + '\n')
        self.logfp.write('Running %s\n\n' % testfile)
        self.logfp.flush()

    def setbuild(self, key):
        '''configure current build info'''
        b = buildObj[key]
        self.build  = 'cur build: %s group=%s\n' % (key, b.group)
        self.build += 'generator: %s\n' % b.gen
        self.build += 'options  : %s (' % b.cmakeopts
        for opt in b.opts:
            if '-' in opt:
                self.build += '%s %s' % (opt, b.opts[opt])
            else:
                self.build += '-D%s=%s' % (opt, b.opts[opt])
        self.build += ')\n'
        self.logfp.write(self.build + '\n')
        self.logfp.flush()

    def logrevs(self, lastchange):
        '''configure current revision info'''
        self.write('Revision under test:')
        self.write(gitrevisioninfo(testrev))

    def read(self):
        return open(self.logfname, 'r').read()

    def write(self, *args):
        '''print text to stdout and maybe write to file'''
        print ' '.join(args)

    def writefp(self, message):
        if os.linesep == '\r\n':
            message = message.replace(os.linesep, '\n')
        self.logfp.write(message + '\n')
        self.logfp.flush()

    def summaryfile(self, commit):
        self.write('summary.txt file does not exist for <%s> \n\n' % commit)
        self.logfp.write(self.test)
        self.logfp.write('summary.txt file does not exist <%s>\n\n' % commit)
        self.logfp.flush()			

    def writeerr(self, message):
        '''cmake, make, or testbench errors'''
        # TODO: wrapper for pastebin
        if os.linesep == '\r\n':
            message = message.replace(os.linesep, '\n')
        self.logfp.write(message + '\n')
        self.logfp.flush()
        self.errors += 1

    def settest(self, seq, command, extras, hash):
        '''configure current test case'''
        self.test  = 'command: %s %s\n' % (seq, command)
        self.test += '   hash: %s\n' % hash
        self.test += ' extras: ' + ' '.join(extras) + '\n\n'
        self.tablecommand = '%s %s'% (seq, command)
        self.tablecommand += '  '.join(extras)        
        nofn = '[%d/%d]' % (self.testcount, self.totaltests)
        self.settitle(' '.join([nofn, seq, command]))
        print nofn,

    def settestcount(self, count):
        self.totaltests = count

    def settitle(self, str):
        '''set console title'''
        title = '%s: %s' % (platform.node(), str)
        if os.name == 'nt':
            try:
                import ctypes
                ctypes.windll.kernel32.SetConsoleTitleA(title)
                return
            except ImportError:
                pass
            try:
                import win32console
                win32console.SetConsoleTitle(title)
                return
            except ImportError:
                pass
        elif 'xterm' in os.getenv('TERM', ''):
            sys.stdout.write("\x1b]2;%s\x07" % title)

    def testfail(self, prefix, errors, logs):
        '''decoder test failures'''
        if my_pastebin_key:
            url = pastebin('\n'.join([self.header, self.build, self.test,
                                      prefix, errors, logs]))
            self.write(' '.join([prefix, url]))
            self.logfp.write('\n'.join(['**', self.test, prefix, url, '']))
        else:
            message = '\n'.join([prefix, errors, logs])
            if os.linesep == '\r\n':
                message = message.replace(os.linesep, '\n')
            self.write(prefix)
            self.logfp.write('**\n\n' + self.test)
            self.logfp.write(message + '\n')
        self.logfp.flush()
        self.errors += 1

    def close(self, logfp):
        for co, count in self.newoutputs.iteritems():
            msg = '%d test case output changes credited to %s\n' % (count, co)
            print msg
            logfp.write(msg)
        if self.errors:
            print 'Errors written to %s' % self.logfname
        else:
            msg = '\nAll tests passed for %s on %s' % (testrev, my_machine_name)
            print msg
            logfp.write(msg)
        logfp.close()
        self.settitle(os.path.basename(test_file) + ' complete')

def setup(argv, preferredlist):
    if not find_executable(version_control):
        raise Exception('Unable to find Git executable %s' %version_control)
    if not find_executable('cmake'):
        raise Exception('Unable to find cmake executable')
    if not os.path.exists(os.path.join(my_vvdec_source, 'CMakeLists.txt')) and not os.path.exists(os.path.join(my_vvdec_source, 'configure')):
        raise Exception('my_vvdec_source does not point to vvdec source/ folder')
    #if not find_executable(my_vtm_binary):
    #    raise Exception('Unable to find decoder')

    global run_make, rebuild, only_yuv, run_adb, test_file
    global only_string, skip_string

    if my_tempfolder:
        tempfile.tempdir = my_tempfolder

    test_file = preferredlist

    import getopt
    longopts = ['builds=', 'help', 'no-make', 'rebuild', 'yuv', 'adb', 'only=', 'skip=', 'tests=']
    optlist, args = getopt.getopt(argv[1:], 'hb:t:', longopts)
    for opt, val in optlist:
        # restrict the list of target builds to just those specified by -b
        # for example: ./smoke-test.py -b "gcc32 gcc10"
        if opt in ('-b', '--builds'):
            userbuilds = val.split()
            delkeys = [key for key in my_builds if not key in userbuilds]
            for key in delkeys:
                del my_builds[key]
        elif opt == '--skip':
            skip_string = val
        elif opt == '--only':
            only_string = val
        elif opt in ('-t', '--tests'):
            test_file = val
        elif opt == '--no-make':
            run_make = False
        elif opt == '--rebuild':
            rebuild = True
        elif opt == '--yuv':
            only_yuv = True
        elif opt == '--adb':
            run_adb = True
        elif opt in ('-h', '--help'):
            print sys.argv[0], '[OPTIONS]\n'
            print '\t-h/--help            show this help'
            print '\t-b/--builds <string> space seperated list of build targets'
            print '\t-t/--tests <fname>   location of text file with test cases'
            print '\t   --skip <string>   skip test cases matching string'
            print '\t   --only <string>   only test cases matching string'
            print '\t   --no-make         do not compile sources'
            print '\t   --rebuild         remove old build folders and rebuild'
            print '\t   --yuv             verify through generate yuv file'
            print '\t   --adb             verify through adb interface'
            sys.exit(0)

    listInRepo = os.path.join(my_vvdec_source, 'test-harness', test_file)
    if os.sep not in test_file and os.path.exists(listInRepo):
        test_file = listInRepo
    elif not os.path.exists(test_file):
        raise Exception('Unable to find test list file ' + test_file)

    global buildObj
    for key in my_builds:
        buildObj[key] = Build(key, *my_builds[key])

    global logger, testrev, changers
    logger = Logger(test_file)

    def closelog(logfp):
        logger.close(logfp)
    atexit.register(closelog, logger.logfp)

    testrev = gitversion(my_vvdec_source)

    if testrev.endswith('+'):
        logger.write('NOTE: Revision under test is not public or has uncommited changes.')
        logger.write('No new golden outputs will be generated during this run, neither')
        logger.write('will it create pass/fail files.\n')

    logger.logrevs(testrev)

def buildall(prof=None, buildoptions=None):
    global rebuild
    if not run_make:
        return
    if not buildoptions == None:
        rebuild = True
        global buildObj
        buildObj = {}
        for key in buildoptions:
            buildObj[key] = Build(key, *buildoptions[key])
    for key in buildObj:
        logger.setbuild(key)
        logger.write('Building %s...'% key)
        build = buildObj[key]
        global work_folder
        work_folder = os.path.join(my_tempfolder, decoder_binary_name, build.folder)
        if rebuild and os.path.exists(work_folder):
            shutil.rmtree(work_folder)
        if os.name == 'nt': time.sleep(1)
        if not os.path.exists(work_folder):
            os.makedirs(work_folder)
        if not os.path.exists(os.path.join(work_folder, 'default')):
            os.makedirs(os.path.join(work_folder, 'default'))
        else:
            generator = None
        defaultco = []
        extra_libs = []
        if extra_libs:
            defaultco.append('-DEXTRA_LIB=' + ';'.join(extra_libs))
            if extra_link_flag: defaultco.append(extra_link_flag)
        build.cmakeoptions(key, defaultco, prof)
        build.cmake_build(key, defaultco, os.path.join(work_folder, 'default'))
        if not os.path.isfile(build.exe):
            logger.write('vvdec executable not found')
            logger.writeerr('vvdec <%s> cli not compiled\n\n' % build.exe)
        elif run_adb:
            cmds = my_adb_cmd + ' push ' + build.exe + ' ' + my_adb_tmpfolder
            p = Popen(cmds, stdout=PIPE, stderr=PIPE, cwd=work_folder)
            stdout, stderr = p.communicate()
            cmds = my_adb_cmd + ' shell chmod +x ' + my_adb_decoder
            p = Popen(cmds, stdout=PIPE, stderr=PIPE, cwd=work_folder)
            stdout, stderr = p.communicate()

def testcasehash(command):
    m = hashlib.md5()
    m.update(command)
    return m.hexdigest()[:12]

def hashbitstream(infn, blocksize=2**20):
    m = hashlib.md5()
    with open(infn, "rb" ) as f:
        while True:
            buf = f.read(blocksize)
            if not buf:
                break
            m.update(buf)
    return m.hexdigest()

def runtest(key, seq, md5, extras):
    '''
    Execute one complete test case (one line in a testcase file):
       key      - build keyname
       seq      - sequence basename
       md5      - expect checksum
       extras   - non-output changing arguments

    Creates a temp-folder, runs the test, verifies, then removes the temp-
    folder.
    '''

    def skip(*matchers):
        if skip_string:
            if [True for f in matchers if skip_string in f]:
                logger.write('Skipping test', f)
                return True
        if only_string:
            if not [True for f in matchers if only_string in f]:
                return True
        return False

    logger.testcount += 1
    build = buildObj[key]
    isNdk = 'ndk' in key;

    vvdec = build.exe if not run_adb else my_adb_decoder
    tmpfolder = tempfile.mkdtemp(prefix='vvdec-tmp')
    if not run_adb:
        if isNdk:
            return
        seqfullpath = os.path.join(my_sequences, seq)
        tmpfile = os.path.join(tmpfolder, r'tmp.yuv')
    else:
        seqfullpath = my_adb_sequences + '/' + seq
        tmpfile = my_adb_tmpfolder + r'/tmp.yuv'

    if run_adb:
        command_prefix = my_adb_cmd + ' shell '
    else:
        command_prefix = ''

    #chmod to avoid premission issue after manual upload binary
    if run_adb and not run_make:
        command_prefix += ' chmod +x ' + vvdec + ' && '

    command = command_prefix + vvdec + r' -b ' + seqfullpath
    command += r' -o ' + tmpfile if only_yuv else r' --md5'
    cmdhash = testcasehash(command)

    try:
        logger.settest(seq, command, extras, cmdhash)
        logger.write('[%s] testing %s' % (key, seq))
        #print 'extras: %s ...' % ' '.join(extras),
        sys.stdout.flush()

        logs, errors, summary = '', '', ''
        if not os.path.isfile(seqfullpath) and not isNdk:
            logger.write('Sequence not found' + seqfullpath)
            errors = 'sequence <%s> not found\n\n' % seqfullpath
        else:
            def prefn():
                import resource # enable core dumps
                resource.setrlimit(resource.RLIMIT_CORE, (-1, -1))

            origpath = os.environ['PATH']
            if 'PATH' in build.opts:
                os.environ['PATH'] += os.pathsep + build.opts['PATH']
            if os.name == 'nt':
                p = Popen(command, cwd=tmpfolder, stdout=PIPE, stderr=PIPE, shell=True)
            else:
                p = Popen(command, cwd=tmpfolder, stdout=PIPE, stderr=PIPE, preexec_fn=prefn)
            stdout, stderr = p.communicate()
            os.environ['PATH'] = origpath

            # prune progress reports
            el = [l for l in stderr.splitlines(True) if not l.endswith('\r')]
            # prune debug and full level log messages
            el = [l for l in el if not l.startswith(('vvdec [debug]:', 'vvdec [full]:'))]
            logs = 'Full decoder logs without progress reports or debug/full logs:\n'
            logs += ''.join(el) + stdout

            # Parse output
            if p.returncode == -11:
                errors += 'vvdec encountered SIGSEGV\n\n'
            elif p.returncode == -6:
                errors += 'vvdec encountered SIGABRT (usually check failure)\n\n'
            elif p.returncode == -4:
                errors += 'vvdec encountered SIGILL (usually -ftrapv)\n\n'
            elif p.returncode:
                errors += 'vvdec return code %d\n\n' % p.returncode
            else:
                if only_yuv:
                    _hash = hashbitstream(tmpfile)
                else:
                    _hash = ''
                    posHash = stdout.rfind('YUV_MD5')
                    _s = re.search('YUV_MD5=([0-9a-f]+)', stdout[posHash:]).group(1)
                    if _s:
                        _hash = _s.strip()

                if _hash.lower() == md5.lower():
                    logger.writefp('[%d/%d] [%s] %s (%s)' % (logger.testcount, logger.totaltests, key, seq, _hash))
                else:
                    logger.writefp('[%d/%d] [%s] %s (%s -> %s)' % (logger.testcount, logger.totaltests, key, seq, md5, _hash))
                    logger.testfail('hash mismatch', 'yuv is mismatched with reference yuv', '')

        #logger.write('')
        if errors:
            logger.writeerr(errors)

    except ImportError, e:
        print e
        sys.exit(1)

    finally:
        if not run_adb:
            shutil.rmtree(tmpfolder)

