#!/usr/bin/python
# -*- coding: utf-8 -*-

# Copyright (C) 2005 Javier Carranza and others for Guadalinex
# Copyright (C) 2005, 2006 Canonical Ltd.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

import sys
import os
import platform
import errno
import stat
import re
import textwrap
import shutil
import subprocess
import time
import struct
import socket
import fcntl
import traceback
import debconf
import apt_pkg
from apt.package import Package
from apt.cache import Cache
from apt.progress import FetchProgress, InstallProgress

sys.path.insert(0, '/usr/lib/ubiquity')

from ubiquity import misc
from ubiquity.components import language_apply, apt_setup, timezone_apply, \
                                clock_setup, kbd_chooser_apply, \
                                usersetup_apply, hw_detect, check_kernels

class DebconfFetchProgress(FetchProgress):
    """An object that reports apt's fetching progress using debconf."""

    def __init__(self, db, title, info_starting, info):
        FetchProgress.__init__(self)
        self.db = db
        self.title = title
        self.info_starting = info_starting
        self.info = info
        self.old_capb = None
        self.eta = 0.0

    def start(self):
        self.db.progress('START', 0, 100, self.title)
        if self.info_starting is not None:
            self.db.progress('INFO', self.info_starting)
        self.old_capb = self.db.capb()
        capb_list = self.old_capb.split()
        capb_list.append('progresscancel')
        self.db.capb(' '.join(capb_list))

    # TODO cjwatson 2006-02-27: implement updateStatus

    def pulse(self):
        FetchProgress.pulse(self)
        try:
            self.db.progress('SET', int(self.percent))
        except debconf.DebconfError:
            return False
        if self.eta != 0.0:
            time_str = "%d:%02d" % divmod(int(self.eta), 60)
            self.db.subst(self.info, 'TIME', time_str)
            try:
                self.db.progress('INFO', self.info)
            except debconf.DebconfError:
                return False
        return True

    def stop(self):
        if self.old_capb is not None:
            self.db.capb(self.old_capb)
            self.old_capb = None
            self.db.progress('STOP')

class DebconfInstallProgress(InstallProgress):
    """An object that reports apt's installation progress using debconf."""

    def __init__(self, db, title, info, error=None):
        InstallProgress.__init__(self)
        self.db = db
        self.title = title
        self.info = info
        self.error_template = error
        self.started = False
        # InstallProgress uses a non-blocking status fd; our run()
        # implementation doesn't need that, and in fact we spin unless the
        # fd is blocking.
        flags = fcntl.fcntl(self.statusfd.fileno(), fcntl.F_GETFL)
        fcntl.fcntl(self.statusfd.fileno(), fcntl.F_SETFL,
                    flags & ~os.O_NONBLOCK)

    def startUpdate(self):
        self.db.progress('START', 0, 100, self.title)
        self.started = True

    def error(self, pkg, errormsg):
        if self.error_template is not None:
            self.db.subst(self.error_template, 'PACKAGE', pkg)
            self.db.subst(self.error_template, 'MESSAGE', errormsg)
            self.db.input('critical', self.error_template)
            self.db.go()

    def statusChange(self, pkg, percent, status):
        self.percent = percent
        self.status = status
        self.db.progress('SET', int(percent))
        self.db.subst(self.info, 'DESCRIPTION', status)
        self.db.progress('INFO', self.info)

    def updateInterface(self):
        # TODO cjwatson 2006-02-28: InstallProgress.updateInterface doesn't
        # give us a handy way to spot when percentages/statuses change and
        # aren't pmerror/pmconffile, so we have to reimplement it here.
        if self.statusfd is None:
            return False
        try:
            while not self.read.endswith("\n"):
                r = os.read(self.statusfd.fileno(),1)
                if not r:
                    return False
                self.read += r
        except OSError, (err,errstr):
            print errstr
        if self.read.endswith("\n"):
            s = self.read
            (status, pkg, percent, status_str) = s.split(":", 3)
            if status == "pmerror":
                self.error(pkg, status_str)
            elif status == "pmconffile":
                # we get a string like this:
                # 'current-conffile' 'new-conffile' useredited distedited
                match = re.compile("\s*\'(.*)\'\s*\'(.*)\'.*").match(status_str)
                if match:
                    self.conffile(match.group(1), match.group(2))
            else:
                self.statusChange(pkg, float(percent), status_str.strip())
            self.read = ""
        return True

    def run(self, pm):
        # Create a subprocess to deal with turning apt status messages into
        # debconf protocol messages.
        child_pid = self.fork()
        if child_pid == 0:
            # child
            os.close(self.writefd)
            try:
                while self.updateInterface():
                    pass
            except (KeyboardInterrupt, SystemExit):
                pass # we're going to exit anyway
            except:
                traceback.print_exc(file=sys.stderr)
            os._exit(0)

        self.statusfd.close()

        # Redirect stdout to stderr to avoid it interfering with our
        # debconf protocol stream.
        saved_stdout = os.dup(1)
        os.dup2(2, 1)

        # Make sure all packages are installed non-interactively. We
        # don't have enough passthrough magic here to deal with any
        # debconf questions they might ask.
        saved_environ_keys = ('DEBIAN_FRONTEND', 'DEBIAN_HAS_FRONTEND',
                              'DEBCONF_USE_CDEBCONF')
        saved_environ = {}
        for key in saved_environ_keys:
            if key in os.environ:
                saved_environ[key] = os.environ[key]
        os.environ['DEBIAN_FRONTEND'] = 'noninteractive'
        if 'DEBIAN_HAS_FRONTEND' in os.environ:
            del os.environ['DEBIAN_HAS_FRONTEND']
        if 'DEBCONF_USE_CDEBCONF' in os.environ:
            # Probably not a good idea to use this in /target too ...
            del os.environ['DEBCONF_USE_CDEBCONF']

        res = pm.ResultFailed
        try:
            res = pm.DoInstall(self.writefd)
        finally:
            # Reap the status-to-debconf subprocess.
            os.close(self.writefd)
            while True:
                try:
                    (pid, status) = os.waitpid(child_pid, 0)
                    if pid != child_pid:
                        break
                    if os.WIFEXITED(status) or os.WIFSIGNALED(status):
                        break
                except OSError:
                    break

            # Put back stdout.
            os.dup2(saved_stdout, 1)
            os.close(saved_stdout)

            # Put back the environment.
            for key in saved_environ_keys:
                if key in saved_environ:
                    os.environ[key] = saved_environ[key]
                elif key in os.environ:
                    del os.environ[key]

        return res

    def finishUpdate(self):
        if self.started:
            self.db.progress('STOP')
            self.started = False

class InstallStepError(Exception):
    """Raised when an install step fails.

    Attributes:
        message -- message returned with exception

    """

    def __init__(self, message):
        self.message = message

class Install:

    def __init__(self):
        """Initial attributes."""

        if os.path.isdir('/rofs'):
            self.source = '/rofs'
        elif os.path.isdir('/UNIONFS'):
            # Klaus Knopper says this may not actually work very well
            # because it'll copy the WHOLE WORLD (~12GB).
            self.source = '/UNIONFS'
        else:
            self.source = '/source'
        self.target = '/target'
        self.unionfs = False
        self.kernel_version = platform.release()
        self.db = debconf.Debconf()

        apt_pkg.InitConfig()
        apt_pkg.Config.Set("Dir", "/target")
        apt_pkg.Config.Set("Dir::State::status", "/target/var/lib/dpkg/status")
        apt_pkg.Config.Set("APT::GPGV::TrustedKeyring",
                           "/target/etc/apt/trusted.gpg")
        apt_pkg.Config.Set("Acquire::gpgv::Options::",
                           "--ignore-time-conflict")
        apt_pkg.Config.Set("DPkg::Options::", "--root=/target")
        # We don't want apt-listchanges or dpkg-preconfigure, so just clear
        # out the list of pre-installation hooks.
        apt_pkg.Config.Clear("DPkg::Pre-Install-Pkgs")
        apt_pkg.InitSystem()

    def excepthook(self, exctype, excvalue, exctb):
        """Crash handler. Dump the traceback to a file so that it can be
        read by the caller."""

        if (issubclass(exctype, KeyboardInterrupt) or
            issubclass(exctype, SystemExit)):
            return

        tbtext = ''.join(traceback.format_exception(exctype, excvalue, exctb))
        print >>sys.stderr, "Exception during installation:"
        print >>sys.stderr, tbtext
        tbfile = open('/var/lib/ubiquity/install.trace', 'w')
        print >>tbfile, tbtext
        tbfile.close()

        sys.exit(1)

    def run(self):
        """Run the install stage: copy everything to the target system, then
        configure it as necessary."""

        self.db.progress('START', 0, 100, 'ubiquity/install/title')
        self.db.progress('INFO', 'ubiquity/install/mounting_source')

        try:
            if self.source == '/source':
                self.mount_source()

            self.db.progress('SET', 1)
            self.db.progress('REGION', 1, 78)
            self.copy_all()

            self.db.progress('SET', 78)
            self.db.progress('INFO', 'ubiquity/install/cleanup')
            if self.source == '/source':
                self.umount_source()

            self.db.progress('SET', 79)
            self.db.progress('REGION', 79, 80)
            self.run_target_config_hooks()

            self.db.progress('SET', 80)
            self.db.progress('REGION', 80, 81)
            self.db.progress('INFO', 'ubiquity/install/locales')
            self.configure_locales()

            self.db.progress('SET', 81)
            self.db.progress('REGION', 81, 82)
            self.db.progress('INFO', 'ubiquity/install/network')
            self.configure_network()

            self.db.progress('SET', 82)
            self.db.progress('REGION', 82, 83)
            self.db.progress('INFO', 'ubiquity/install/apt')
            self.configure_apt()

            self.db.progress('SET', 83)
            self.db.progress('REGION', 83, 87)
            # Ignore failures from language pack installation.
            try:
                self.install_language_packs()
            except InstallStepError:
                pass
            except IOError:
                pass
            except SystemError:
                pass

            self.db.progress('SET', 87)
            self.db.progress('REGION', 87, 88)
            self.db.progress('INFO', 'ubiquity/install/timezone')
            self.configure_timezone()

            self.db.progress('SET', 88)
            self.db.progress('REGION', 88, 90)
            self.db.progress('INFO', 'ubiquity/install/keyboard')
            self.configure_keyboard()

            self.db.progress('SET', 90)
            self.db.progress('REGION', 90, 91)
            self.db.progress('INFO', 'ubiquity/install/user')
            self.configure_user()

            self.db.progress('SET', 91)
            self.db.progress('REGION', 91, 95)
            self.db.progress('INFO', 'ubiquity/install/hardware')
            self.configure_hardware()

            self.db.progress('SET', 95)
            self.db.progress('REGION', 95, 96)
            self.remove_unusable_kernels()

            self.db.progress('SET', 96)
            self.db.progress('REGION', 96, 97)
            self.db.progress('INFO', 'ubiquity/install/bootloader')
            self.configure_bootloader()

            self.db.progress('SET', 97)
            self.db.progress('REGION', 97, 99)
            self.db.progress('INFO', 'ubiquity/install/removing')
            self.remove_extras()

            self.db.progress('SET', 99)
            self.db.progress('INFO', 'ubiquity/install/log_files')
            self.copy_logs()

            self.cleanup()

            self.db.progress('SET', 100)
        finally:
            try:
                self.db.progress('STOP')
            except (KeyboardInterrupt, SystemExit):
                raise
            except:
                pass


    def copy_all(self):
        """Core copy process. This is the most important step of this
        stage. It clones live filesystem into a local partition in the
        selected hard disk."""

        files = []
        total_size = 0

        self.db.progress('START', 0, 100, 'ubiquity/install/title')
        self.db.progress('INFO', 'ubiquity/install/scanning')

        # Obviously doing os.walk() twice is inefficient, but I'd rather not
        # suck the list into ubiquity's memory, and I'm guessing that the
        # kernel's dentry cache will avoid most of the slowness anyway.
        walklen = 0
        for entry in os.walk(self.source):
            walklen += 1
        walkpos = 0
        walkprogress = 0

        for dirpath, dirnames, filenames in os.walk(self.source):
            walkpos += 1
            if int(float(walkpos) / walklen * 10) != walkprogress:
                walkprogress = int(float(walkpos) / walklen * 10)
                self.db.progress('SET', walkprogress)

            sourcepath = dirpath[len(self.source) + 1:]

            for name in dirnames + filenames:
                relpath = os.path.join(sourcepath, name)
                fqpath = os.path.join(self.source, dirpath, name)

                total_size += os.lstat(fqpath).st_size
                files.append(relpath)

        self.db.progress('SET', 10)
        self.db.progress('INFO', 'ubiquity/install/copying')

        # Progress bar handling:
        # We sample progress every half-second (assuming time.time() gives
        # us sufficiently good granularity) and use the average of progress
        # over the last minute or so to decide how much time remains. We
        # don't bother displaying any progress for the first ten seconds in
        # order to allow things to settle down, and we only update the "time
        # remaining" indicator at most every two seconds after that.

        copy_progress = 0
        copied_size, counter = 0, 0
        directory_times = []
        time_start = time.time()
        times = [(time_start, copied_size)]
        long_enough = False
        time_last_update = time_start

        old_umask = os.umask(0)
        for path in files:
            sourcepath = os.path.join(self.source, path)
            targetpath = os.path.join(self.target, path)
            st = os.lstat(sourcepath)
            mode = stat.S_IMODE(st.st_mode)
            if stat.S_ISLNK(st.st_mode):
                if not os.path.lexists(targetpath):
                    linkto = os.readlink(sourcepath)
                    os.symlink(linkto, targetpath)
            elif stat.S_ISDIR(st.st_mode):
                if not os.path.isdir(targetpath):
                    os.mkdir(targetpath, mode)
            elif stat.S_ISCHR(st.st_mode):
                os.mknod(targetpath, stat.S_IFCHR | mode, st.st_rdev)
            elif stat.S_ISBLK(st.st_mode):
                os.mknod(targetpath, stat.S_IFBLK | mode, st.st_rdev)
            elif stat.S_ISFIFO(st.st_mode):
                os.mknod(targetpath, stat.S_IFIFO | mode)
            elif stat.S_ISSOCK(st.st_mode):
                os.mknod(targetpath, stat.S_IFSOCK | mode)
            elif stat.S_ISREG(st.st_mode):
                if not os.path.exists(targetpath):
                    shutil.copyfile(sourcepath, targetpath)

            copied_size += st.st_size
            os.lchown(targetpath, st.st_uid, st.st_gid)
            if not stat.S_ISLNK(st.st_mode):
                os.chmod(targetpath, mode)
            if stat.S_ISDIR(st.st_mode):
                directory_times.append((targetpath, st.st_atime, st.st_mtime))
            # os.utime() sets timestamp of target, not link
            elif not stat.S_ISLNK(st.st_mode):
                os.utime(targetpath, (st.st_atime, st.st_mtime))

            if int((copied_size * 90) / total_size) != copy_progress:
                copy_progress = int((copied_size * 90) / total_size)
                self.db.progress('SET', 10 + copy_progress)

            time_now = time.time()
            if (time_now - times[-1][0]) >= 0.5:
                times.append((time_now, copied_size))
                if not long_enough and time_now - times[0][0] >= 10:
                    long_enough = True
                if long_enough and time_now - time_last_update >= 2:
                    time_last_update = time_now
                    while (time_now - times[0][0] > 60 and
                           time_now - times[1][0] >= 60):
                        times.pop(0)
                    speed = ((times[-1][1] - times[0][1]) /
                             (times[-1][0] - times[0][0]))
                    time_remaining = int((total_size - copied_size) / speed)
                    time_str = "%d:%02d" % divmod(time_remaining, 60)
                    self.db.subst('ubiquity/install/copying_time',
                                  'TIME', time_str)
                    self.db.progress('INFO', 'ubiquity/install/copying_time')

        # Apply timestamps to all directories now that the items within them
        # have been copied.
        for dirtime in directory_times:
            (directory, atime, mtime) = dirtime
            os.utime(directory, (atime, mtime))

        os.umask(old_umask)

        self.db.progress('SET', 100)
        self.db.progress('STOP')


    def copy_logs(self):
        """copy log files into installed system."""

        target_dir = os.path.join(self.target, 'var/log/installer')
        if not os.path.exists(target_dir):
            os.makedirs(target_dir)

        for log_file in ('/var/log/installer/syslog', '/var/log/partman',
                         '/var/log/installer/version'):
            target_log_file = os.path.join(target_dir,
                                           os.path.basename(log_file))
            if not misc.ex('cp', '-a', log_file, target_log_file):
                misc.pre_log('error', 'Failed to copy installation log file')
            os.chmod(target_log_file, stat.S_IRUSR | stat.S_IWUSR)


    def mount_source(self):
        """mounting loop system from cloop or squashfs system."""

        self.dev = ''
        if not os.path.isdir(self.source):
            try:
                os.mkdir(self.source)
            except Exception, e:
                print e
            misc.pre_log('info', 'mkdir %s' % self.source)

        # Autodetection on unionfs systems
        for line in open('/proc/mounts'):
            if line.split()[2] == 'squashfs':
                misc.ex('mount', '--bind', line.split()[1], self.source)
                self.unionfs = True
                return

        # Manual Detection on non unionfs systems
        fsfiles = ['/cdrom/casper/filesystem.cloop',
                   '/cdrom/casper/filesystem.squashfs',
                   '/cdrom/META/META.squashfs']

        for fsfile in fsfiles:
            if os.path.isfile(fsfile):
                if os.path.splitext(fsfile)[1] == '.cloop':
                    self.dev = '/dev/cloop1'
                    break
                elif os.path.splitext(fsfile)[1] == '.squashfs':
                    self.dev = '/dev/loop3'
                    break

        if self.dev == '':
            raise InstallStepError("No source device found")

        misc.ex('losetup', self.dev, file)
        try:
            misc.ex('mount', self.dev, self.source)
        except Exception, e:
            print e


    def umount_source(self):
        """umounting loop system from cloop or squashfs system."""

        if not misc.ex('umount', self.source):
            raise InstallStepError("Failed to unmount source device")
        if self.unionfs:
            return
        if not misc.ex('losetup', '-d', self.dev) and self.dev != '':
            raise InstallStepError("Failed to detach loopback source device")


    def run_target_config_hooks(self):
        """Run hook scripts from /usr/lib/ubiquity/target-config. This allows
        casper to hook into us and repeat bits of its configuration in the
        target system."""

        hookdir = '/usr/lib/ubiquity/target-config'

        if os.path.isdir(hookdir):
            # Exclude hooks containing '.', so that *.dpkg-* et al are avoided.
            hooks = filter(lambda entry: '.' not in entry, os.listdir(hookdir))
            self.db.progress('START', 0, len(hooks), 'ubiquity/install/title')
            for hookentry in hooks:
                hook = os.path.join(hookdir, hookentry)
                if not os.access(hook, os.X_OK):
                    self.db.progress('STEP', 1)
                    continue
                self.db.subst('ubiquity/install/target_hook',
                              'SCRIPT', hookentry)
                self.db.progress('INFO', 'ubiquity/install/target_hook')
                # Errors are ignored at present, although this may change.
                subprocess.call(hook)
                self.db.progress('STEP', 1)
            self.db.progress('STOP')


    def configure_locales(self):
        """Apply locale settings to installed system."""
        dbfilter = language_apply.LanguageApply(None)
        ret = dbfilter.run_command(auto_process=True)
        if ret != 0:
            raise InstallStepError("LanguageApply failed with code %d" % ret)


    def configure_apt(self):
        """Configure /etc/apt/sources.list."""

        # Avoid clock skew causing gpg verification issues.
        # This file will be left in place until the end of the install.
        apt_conf_itc = open(os.path.join(
            self.target, 'etc/apt/apt.conf.d/00IgnoreTimeConflict'), 'w')
        print >>apt_conf_itc, ('Acquire::gpgv::Options {'
                               ' "--ignore-time-conflict"; };')
        apt_conf_itc.close()

        dbfilter = apt_setup.AptSetup(None)
        ret = dbfilter.run_command(auto_process=True)
        if ret != 0:
            raise InstallStepError("AptSetup failed with code %d" % ret)


    def get_cache_pkg(self, cache, pkg):
        # work around broken has_key in python-apt 0.6.16
        try:
            return cache[pkg]
        except KeyError:
            return None


    def record_installed(self, pkgs):
        """Record which packages we've explicitly installed so that we don't
        try to remove them later."""

        record_file = "/var/lib/ubiquity/apt-installed"
        if not os.path.exists(os.path.dirname(record_file)):
            os.makedirs(os.path.dirname(record_file))
        record = open(record_file, "a")

        for pkg in pkgs:
            print >>record, pkg

        record.close()


    def mark_install(self, cache, pkg):
        cachedpkg = self.get_cache_pkg(cache, pkg)
        if cachedpkg is not None and not cachedpkg.isInstalled:
            apt_error = False
            try:
                cachedpkg.markInstall()
            except SystemError:
                apt_error = True
            if cache._depcache.BrokenCount > 0 or apt_error:
                cachedpkg.markKeep()
                assert cache._depcache.BrokenCount == 0


    def install_language_packs(self):
        langpacks = []
        try:
            langpack_db = self.db.get('base-config/language-packs')
            langpacks = langpack_db.replace(',', '').split()
        except debconf.DebconfError:
            pass
        if not langpacks:
            try:
                langpack_db = self.db.get('pkgsel/language-packs')
                langpacks = langpack_db.replace(',', '').split()
            except debconf.DebconfError:
                pass
        if not langpacks:
            try:
                langpack_db = self.db.get('localechooser/supported-locales')
                langpack_set = set()
                for locale in langpack_db.replace(',', '').split():
                    langpack_set.add(locale.split('_')[0])
                langpacks = sorted(langpack_set)
            except debconf.DebconfError:
                pass
        if not langpacks:
            langpack_db = self.db.get('debian-installer/locale')
            langpacks = [langpack_db.split('_')[0]]
        misc.pre_log('info',
                     'keeping language packs for: %s' % ' '.join(langpacks))

        try:
            lppatterns = self.db.get('pkgsel/language-pack-patterns').split()
        except debconf.DebconfError:
            return

        to_install = []
        for lp in langpacks:
            # Basic language packs, required to get localisation working at
            # all. We install these almost unconditionally; if you want to
            # get rid of even these, you can preseed pkgsel/language-packs
            # to the empty string.
            to_install.append('language-pack-%s' % lp)
            # Other language packs, typically selected by preseeding.
            for pattern in lppatterns:
                to_install.append(pattern.replace('$LL', lp))
            # More extensive language support packages.
            to_install.append('language-support-%s' % lp)
        self.record_installed(to_install)

        self.db.progress('START', 0, 100, 'ubiquity/langpacks/title')

        self.db.progress('REGION', 0, 10)
        fetchprogress = DebconfFetchProgress(
            self.db, 'ubiquity/langpacks/title',
            'ubiquity/install/apt_indices_starting',
            'ubiquity/install/apt_indices')
        cache = Cache()
        try:
            # update() returns False on failure and 0 on success. Madness!
            if cache.update(fetchprogress) not in (0, True):
                fetchprogress.stop()
                self.db.progress('STOP')
                return
        except IOError, e:
            print >>sys.stderr, e
            sys.stderr.flush()
            self.db.progress('STOP')
            raise
        cache.open(None)
        self.db.progress('SET', 10)

        self.db.progress('REGION', 10, 100)
        fetchprogress = DebconfFetchProgress(
            self.db, 'ubiquity/langpacks/title', None,
            'ubiquity/langpacks/packages')
        installprogress = DebconfInstallProgress(
            self.db, 'ubiquity/langpacks/title', 'ubiquity/install/apt_info')

        for lp in to_install:
            self.mark_install(cache, lp)
        installed_pkgs = []
        for pkg in cache.keys():
            if (cache[pkg].markedInstall or cache[pkg].markedUpgrade or
                cache[pkg].markedReinstall or cache[pkg].markedDowngrade):
                installed_pkgs.append(pkg)
        self.record_installed(installed_pkgs)

        try:
            if not cache.commit(fetchprogress, installprogress):
                fetchprogress.stop()
                installprogress.finishUpdate()
                self.db.progress('STOP')
                return
        except IOError, e:
            print >>sys.stderr, e
            sys.stderr.flush()
            self.db.progress('STOP')
            raise
        except SystemError, e:
            print >>sys.stderr, e
            sys.stderr.flush()
            self.db.progress('STOP')
            raise
        self.db.progress('SET', 100)

        self.db.progress('STOP')


    def configure_timezone(self):
        """Set timezone on installed system."""

        dbfilter = timezone_apply.TimezoneApply(None)
        ret = dbfilter.run_command(auto_process=True)
        if ret != 0:
            raise InstallStepError("TimezoneApply failed with code %d" % ret)

        dbfilter = clock_setup.ClockSetup(None)
        ret = dbfilter.run_command(auto_process=True)
        if ret != 0:
            raise InstallStepError("ClockSetup failed with code %d" % ret)


    def configure_keyboard(self):
        """Set keyboard in installed system."""

        try:
            keymap = self.db.get('debian-installer/keymap')
            self.set_debconf('debian-installer/keymap', keymap)
        except debconf.DebconfError:
            pass

        dbfilter = kbd_chooser_apply.KbdChooserApply(None)
        ret = dbfilter.run_command(auto_process=True)
        if ret != 0:
            raise InstallStepError("KbdChooserApply failed with code %d" % ret)


    def configure_user(self):
        """create the user selected along the installation process
        into the installed system. Default user from live system is
        deleted and skel for this new user is copied to $HOME."""

        dbfilter = usersetup_apply.UserSetupApply(None)
        ret = dbfilter.run_command(auto_process=True)
        if ret != 0:
            raise InstallStepError("UserSetupApply failed with code %d" % ret)


    def get_resume_partition(self):
        biggest_size = 0
        biggest_partition = None
        swaps = open('/proc/swaps')
        for line in swaps:
            words = line.split()
            if words[1] != 'partition':
                continue
            size = int(words[2])
            if size > biggest_size:
                biggest_size = size
                biggest_partition = words[0]
        swaps.close()
        return biggest_partition

    def configure_hardware(self):
        """reconfiguring several packages which depends on the
        hardware system in which has been installed on and need some
        automatic configurations to get work."""

        dbfilter = hw_detect.HwDetect(None, self.db)
        ret = dbfilter.run_command(auto_process=True)
        if ret != 0:
            raise InstallStepError("HwDetect failed with code %d" % ret)

        self.db.progress('INFO', 'ubiquity/install/hardware')

        subprocess.call(['/usr/lib/ubiquity/debian-installer-utils'
                         '/register-module.post-base-installer'])

        resume = self.get_resume_partition()
        if resume is not None:
            if os.path.exists(os.path.join(self.target,
                                           'etc/initramfs-tools/conf.d')):
                configdir = os.path.join(self.target,
                                         'etc/initramfs-tools/conf.d')
            elif os.path.exists(os.path.join(self.target,
                                             'etc/mkinitramfs/conf.d')):
                configdir = os.path.join(self.target,
                                         'etc/mkinitramfs/conf.d')
            else:
                configdir = None
            if configdir is not None:
                configfile = open(os.path.join(configdir, 'resume'), 'w')
                print >>configfile, "RESUME=%s" % resume
                configfile.close()

        self.chrex('mount', '-t', 'proc', 'proc', '/proc')
        self.chrex('mount', '-t', 'sysfs', 'sysfs', '/sys')

        packages = ['linux-image-' + self.kernel_version,
                    'linux-restricted-modules-' + self.kernel_version]

        try:
            for package in packages:
                self.reconfigure(package)
        finally:
            self.chrex('umount', '/proc')
            self.chrex('umount', '/sys')


    def get_all_interfaces(self):
        """Get all non-local network interfaces."""
        ifs = []
        ifs_file = open('/proc/net/dev')
        # eat header
        ifs_file.readline()
        ifs_file.readline()

        for line in ifs_file:
            name = re.match('(.*?(?::\d+)?):', line.strip()).group(1)
            if name == 'lo':
                continue
            ifs.append(name)

        ifs_file.close()
        return ifs


    def configure_network(self):
        """Automatically configure the network.
        
        At present, the only thing the user gets to tweak in the UI is the
        hostname. Some other things will be copied from the live filesystem,
        so changes made there will be reflected in the installed system.
        
        Unfortunately, at present we have to duplicate a fair bit of netcfg
        here, because it's hard to drive netcfg in a way that won't try to
        bring interfaces up and down."""

        # TODO cjwatson 2006-03-30: just call netcfg instead of doing all
        # this; requires a netcfg binary that doesn't bring interfaces up
        # and down

        for path in ('/etc/network/interfaces', '/etc/resolv.conf'):
            if os.path.exists(path):
                shutil.copy2(path, os.path.join(self.target, path[1:]))

        try:
            hostname = self.db.get('netcfg/get_hostname')
        except debconf.DebconfError:
            hostname = ''
        if hostname == '':
            hostname = 'ubuntu'
        fp = open(os.path.join(self.target, 'etc/hostname'), 'w')
        print >>fp, hostname
        fp.close()

        hosts = open(os.path.join(self.target, 'etc/hosts'), 'w')
        print >>hosts, "127.0.0.1\tlocalhost"
        print >>hosts, "127.0.1.1\t%s" % hostname
        print >>hosts, textwrap.dedent("""\

            # The following lines are desirable for IPv6 capable hosts
            ::1     ip6-localhost ip6-loopback
            fe00::0 ip6-localnet
            ff00::0 ip6-mcastprefix
            ff02::1 ip6-allnodes
            ff02::2 ip6-allrouters
            ff02::3 ip6-allhosts""")
        hosts.close()

        # TODO cjwatson 2006-03-30: from <bits/ioctls.h>; ugh, but no
        # binding available
        SIOCGIFHWADDR = 0x8927
        # <net/if_arp.h>
        ARPHRD_ETHER = 1

        if_names = {}
        sock = socket.socket(socket.SOCK_DGRAM)
        interfaces = self.get_all_interfaces()
        for i in range(len(interfaces)):
            if_names[interfaces[i]] = struct.unpack('H6s',
                fcntl.ioctl(sock.fileno(), SIOCGIFHWADDR,
                            struct.pack('256s', interfaces[i]))[16:24])
        sock.close()

        iftab = open(os.path.join(self.target, 'etc/iftab'), 'w')

        print >>iftab, textwrap.dedent("""\
            # This file assigns persistent names to network interfaces.
            # See iftab(5) for syntax.
            """)

        for i in range(len(interfaces)):
            dup = False
            with_arp = False

            if_name = if_names[interfaces[i]]
            if if_name is None or if_name[0] != ARPHRD_ETHER:
                continue

            for j in range(len(interfaces)):
                if i == j or if_names[interfaces[j]] is None:
                    continue
                if if_name[1] != if_names[interfaces[j]][1]:
                    continue

                if if_names[interfaces[j]][0] == ARPHRD_ETHER:
                    dup = True

            if dup:
                continue

            line = (interfaces[i] + " mac " +
                    ':'.join(['%02x' % ord(if_name[1][c]) for c in range(6)]))
            line += " arp %d" % if_name[0]
            print >>iftab, line

        iftab.close()


    def configure_bootloader(self):
        """configuring and installing boot loader into installed
        hardware system."""

        misc.ex('mount', '--bind', '/proc', self.target + '/proc')
        misc.ex('mount', '--bind', '/dev', self.target + '/dev')

        try:
            from ubiquity.components import grubinstaller
            dbfilter = grubinstaller.GrubInstaller(None)
            ret = dbfilter.run_command(auto_process=True)
            if ret != 0:
                raise InstallStepError(
                    "GrubInstaller failed with code %d" % ret)
        except ImportError:
            try:
                from ubiquity.components import yabootinstaller
                dbfilter = yabootinstaller.YabootInstaller(None)
                ret = dbfilter.run_command(auto_process=True)
                if ret != 0:
                    raise InstallStepError(
                        "YabootInstaller failed with code %d" % ret)
            except ImportError:
                raise InstallStepError("No bootloader installer found")

        misc.ex('umount', '-f', self.target + '/proc')
        misc.ex('umount', '-f', self.target + '/dev')


    def do_remove(self, to_remove, recursive=False):
        self.db.progress('START', 0, 5, 'ubiquity/install/title')
        self.db.progress('INFO', 'ubiquity/install/find_removables')

        fetchprogress = DebconfFetchProgress(
            self.db, 'ubiquity/install/title',
            'ubiquity/install/apt_indices_starting',
            'ubiquity/install/apt_indices')
        cache = Cache()

        while True:
            removed = set()
            for pkg in to_remove:
                cachedpkg = self.get_cache_pkg(cache, pkg)
                if cachedpkg is not None and cachedpkg.isInstalled:
                    apt_error = False
                    try:
                        cachedpkg.markDelete(autoFix=False, purge=True)
                    except SystemError:
                        apt_error = True
                    if apt_error:
                        cachedpkg.markKeep()
                    elif cache._depcache.BrokenCount > 0:
                        # If we're recursively removing packages, or if all
                        # of the broken packages are in the set of packages
                        # to remove anyway, then go ahead and try to remove
                        # them too.
                        brokenpkgs = set()
                        for pkg in cache.keys():
                            if cache._depcache.IsInstBroken(cache._cache[pkg]):
                                brokenpkgs.add(pkg)
                        broken_removed = set()
                        if recursive or brokenpkgs <= to_remove:
                            for pkg in brokenpkgs:
                                cachedpkg2 = self.get_cache_pkg(cache, pkg)
                                if cachedpkg2 is not None:
                                    broken_removed.add(pkg)
                                    try:
                                        cachedpkg2.markDelete(autoFix=False,
                                                              purge=True)
                                    except SystemError:
                                        apt_error = True
                                        break
                        if apt_error or cache._depcache.BrokenCount > 0:
                            # That didn't work. Revert all the removals we
                            # just tried.
                            for pkg in broken_removed:
                                self.get_cache_pkg(cache, pkg).markKeep()
                            cachedpkg.markKeep()
                        else:
                            removed.add(pkg)
                            removed |= broken_removed
                    else:
                        removed.add(pkg)
                    assert cache._depcache.BrokenCount == 0
            if len(removed) == 0:
                break
            to_remove -= removed

        self.db.progress('SET', 1)
        self.db.progress('REGION', 1, 5)
        fetchprogress = DebconfFetchProgress(
            self.db, 'ubiquity/install/title', None,
            'ubiquity/install/fetch_remove')
        installprogress = DebconfInstallProgress(
            self.db, 'ubiquity/install/title', 'ubiquity/install/apt_info',
            'ubiquity/install/apt_error_remove')
        try:
            if not cache.commit(fetchprogress, installprogress):
                fetchprogress.stop()
                installprogress.finishUpdate()
                self.db.progress('STOP')
                return
        except SystemError, e:
            print >>sys.stderr, e
            sys.stderr.flush()
            self.db.progress('STOP')
            raise
        self.db.progress('SET', 5)

        self.db.progress('STOP')


    def remove_unusable_kernels(self):
        """Remove unusable kernels; keeping them may cause us to be unable
        to boot."""

        self.db.progress('START', 0, 6, 'ubiquity/install/title')

        self.db.progress('INFO', 'ubiquity/install/find_removables')

        # Check for kernel packages to remove.
        dbfilter = check_kernels.CheckKernels(None)
        dbfilter.run_command(auto_process=True)

        remove_kernels = set()
        if os.path.exists("/var/lib/ubiquity/remove-kernels"):
            for line in open("/var/lib/ubiquity/remove-kernels"):
                remove_kernels.add(line.strip())

        if len(remove_kernels) == 0:
            self.db.progress('STOP')
            return

        self.db.progress('SET', 1)
        self.db.progress('REGION', 1, 5)
        try:
            self.do_remove(remove_kernels, recursive=True)
        except:
            self.db.progress('STOP')
            raise
        self.db.progress('SET', 5)

        # Now we need to fix up kernel symlinks. Depending on the
        # architecture, these may be in / or in /boot.
        bootdir = os.path.join(self.target, 'boot')
        if self.db.get('base-installer/kernel/linux/link_in_boot') == 'true':
            linkdir = bootdir
            linkprefix = ''
        else:
            linkdir = self.target
            linkprefix = 'boot'

        # Remove old symlinks. We'll set them up from scratch.
        re_symlink = re.compile('vmlinu[xz]|initrd.img$')
        for entry in os.listdir(linkdir):
            if re_symlink.match(entry) is not None:
                filename = os.path.join(linkdir, entry)
                if os.path.islink(filename):
                    os.unlink(filename)
        if linkdir != self.target:
            # Remove symlinks in /target too, which may have been created on
            # the live filesystem. This isn't necessary, but it may help
            # avoid confusion.
            for entry in os.listdir(self.target):
                if re_symlink.match(entry) is not None:
                    filename = os.path.join(self.target, entry)
                    if os.path.islink(filename):
                        os.unlink(filename)

        # Create symlinks. Prefer our current kernel version if possible,
        # but if not (perhaps due to a customised live filesystem image),
        # it's better to create some symlinks than none at all.
        re_image = re.compile('(vmlinu[xz]|initrd.img)-')
        for entry in os.listdir(bootdir):
            match = re_image.match(entry)
            if match is not None:
                imagetype = match.group(1)
                linksrc = os.path.join(linkprefix, entry)
                linkdst = os.path.join(linkdir, imagetype)
                if os.path.exists(linkdst):
                    if entry.endswith('-' + self.kernel_version):
                        os.unlink(linkdst)
                    else:
                        continue
                os.symlink(linksrc, linkdst)

        self.db.progress('SET', 6)
        self.db.progress('STOP')


    def remove_extras(self):
        """Try to remove packages that are needed on the live CD but not on
        the installed system."""

        # Looking through files for packages to remove is pretty quick, so
        # don't bother with a progress bar for that.

        # Check for packages specific to the live CD.
        if (os.path.exists("/cdrom/casper/filesystem.manifest-desktop") and
            os.path.exists("/cdrom/casper/filesystem.manifest")):
            desktop_packages = set()
            for line in open("/cdrom/casper/filesystem.manifest-desktop"):
                if line.strip() != '' and not line.startswith('#'):
                    desktop_packages.add(line.split()[0])
            live_packages = set()
            for line in open("/cdrom/casper/filesystem.manifest"):
                if line.strip() != '' and not line.startswith('#'):
                    live_packages.add(line.split()[0])
            difference = live_packages - desktop_packages
        else:
            difference = set()

        # Keep packages we explicitly installed.
        apt_installed = set()
        if os.path.exists("/var/lib/ubiquity/apt-installed"):
            for line in open("/var/lib/ubiquity/apt-installed"):
                apt_installed.add(line.strip())
        difference -= apt_installed

        if len(difference) == 0:
            return

        # Don't worry about failures removing packages; it will be easier
        # for the user to sort them out with a graphical package manager (or
        # whatever) after installation than it will be to try to deal with
        # them automatically here.
        self.do_remove(difference)


    def cleanup(self):
        """Miscellaneous cleanup tasks."""
        os.unlink(os.path.join(
            self.target, 'etc/apt/apt.conf.d/00IgnoreTimeConflict'))


    def chrex(self, *args):
        """executes commands on chroot system (provided by *args)."""

        msg = ''
        for word in args:
            msg += str(word) + ' '
        if not misc.ex('chroot', self.target, *args):
            misc.pre_log('error', 'chroot ' + msg)
            return False
        return True


    def copy_debconf(self, package):
        """setting debconf database into installed system."""

        # TODO cjwatson 2006-02-25: unusable here now because we have a
        # running debconf frontend that's locked the database; fortunately
        # this isn't critical. We still need to think about how to handle
        # preseeding in general, though.
        targetdb = os.path.join(self.target, 'var/cache/debconf/config.dat')

        misc.ex('debconf-copydb', 'configdb', 'targetdb', '-p',
                '^%s/' % package, '--config=Name:targetdb',
                '--config=Driver:File','--config=Filename:' + targetdb)


    def set_debconf(self, question, value):
        dccomm = subprocess.Popen(['chroot', self.target,
                                   'debconf-communicate',
                                   '-fnoninteractive', 'ubiquity'],
                                  stdin=subprocess.PIPE,
                                  stdout=subprocess.PIPE, close_fds=True)
        dc = debconf.Debconf(read=dccomm.stdout, write=dccomm.stdin)
        dc.set(question, value)
        dc.fset(question, 'seen', 'true')
        dccomm.stdin.close()
        dccomm.wait()


    def reconfigure(self, package):
        """executes a dpkg-reconfigure into installed system to each
        package which provided by args."""

        self.chrex('dpkg-reconfigure', '-fnoninteractive', package)


if __name__ == '__main__':
    if not os.path.exists('/var/lib/ubiquity'):
        os.makedirs('/var/lib/ubiquity')
    if os.path.exists('/var/lib/ubiquity/install.trace'):
        os.unlink('/var/lib/ubiquity/install.trace')

    install = Install()
    sys.excepthook = install.excepthook
    install.run()
    sys.exit(0)

# vim:ai:et:sts=4:tw=80:sw=4:
