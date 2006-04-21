#!/usr/bin/python
# -*- coding: utf-8 -*-

import sys
import os
import stat
import re
import subprocess


def part_label(dev):
    """returns an user-friendly device name from an unix device name."""

    drive_type = {'hd': 'IDE/ATA', 'sd': 'USB/SCSI/SATA'}
    dev, ext = dev.lower(), dev[7:]
    try:
        if int(dev[8:]) > 4:
            partition_type = _('Logical')
        else:
            partition_type = _('Primary')
    except:
        partition_type = _('Unknown')
    try:
        name = _('Partition %s Disc %s %s (%s) [%s]') % (ext[1:], drive_type[dev[5:7]], ord(ext[0])-ord('a')+1, partition_type, dev[5:])
    except:
        """For empty strings, other disk types and disks without partitions, like md1"""
        name = '%s' % (dev[5:])
    return name


def distribution():
    """Returns the name of the running distribution."""

    proc = subprocess.Popen(['lsb_release', '-is'], stdout=subprocess.PIPE)
    return proc.communicate()[0].strip()


def ex(*args):
    """runs args* in shell mode. Output status is taken."""

    import subprocess
    msg = ''
    for word in args:
        msg += str(word) + ' '
      
    try:
        status = subprocess.call(msg, shell=True)
    except IOError, e:
        pre_log('error', msg)
        pre_log('error', "OS error(%s): %s" % (e.errno, e.strerror))
        return False
    else:
        if status != 0:
            pre_log('error', msg)
            return False
        pre_log('info', msg)
        return True


def ret_ex(*args):
    import subprocess
    msg = ''
    for word in args:
        msg += str(word) + ' '
    try:
        proc = subprocess.Popen(args, stdout=subprocess.PIPE, close_fds=True)
    except IOError, e:
        pre_log('error', msg)
        pre_log('error', "I/O error(%s): %s" % (e.errno, e.strerror))
        return None
    else:
        pre_log('info', msg)
        return proc.stdout


def pre_log(code, msg=''):
    """logs install messages into /var/log on live filesystem."""

    import logging
    logging.basicConfig(level=logging.DEBUG,
                        format='%(asctime)s %(levelname)-8s %(message)s',
                        datefmt='%a, %d %b %Y %H:%M:%S',
                        stream=sys.stderr)
    getattr(logging, code)(msg)


def post_log(code, msg=''):
    """logs install messages into /var/log on installed filesystem."""

    log_file = '/target/var/log/installer/syslog'

    if not os.path.exists(os.path.dirname(log_file)):
        os.makedirs(os.path.dirname(log_file))

    import logging
    logging.basicConfig(level=logging.DEBUG,
                        format='%(asctime)s %(levelname)-8s %(message)s',
                        datefmt='%a, %d %b %Y %H:%M:%S',
                        filename=log_file,
                        filemode='a')
    getattr(logging, code)(msg)
    os.chmod(log_file, stat.S_IRUSR | stat.S_IWUSR)


def get_progress(str):
    """gets progress percentage of installing process from progress bar message."""

    num = int(str.split()[:1][0])
    text = ' '.join(str.split()[1:])
    return num, text


def get_partitions():
    """returns an array with fdisk output related to partition data."""

    import re

    # parsing partitions from the procfs
    # attention with the output format. the partitions list is without '/dev/'
    partitions = open('/proc/partitions')
    partition_table = partitions.read()
    regex = re.compile('[sh]d[a-g][0-9]+')
    partition = regex.findall(partition_table)
    partitions.close()

    return partition


def get_sizes():
    """Returns a dictionary of {partition: size} from /proc/partitions."""

    # parsing /proc/partitions and getting size data
    size = {}
    partitions = open('/proc/partitions')
    for line in partitions:
        try:
            size[line.split()[3]] = int(line.split()[2])
        except:
            continue
    partitions.close()
    return size


def disable_swap():
    """Disable swap so that an external partition manager can be used."""
    if not os.path.exists('/proc/swaps'):
        return
    swaps = open('/proc/swaps')
    for swap in swaps:
        if swap.startswith('/dev'):
            device = swap.split()[0]
            pre_log('info', "Disabling swap on %s" % device)
            subprocess.call(['swapoff', device])
    swaps.close()


def get_filesystems(fstype={}):
    """returns a dictionary with a skeleton { device : filesystem }
    with data from local hard disks. Only swap and ext3 filesystems
    are available."""

    import re, subprocess
    device_list = {}

    # building device_list dicts from "file -s" output from get_partitions
    #   returned list (only devices formatted as ext3, fat, ntfs or swap are
    #   parsed).
    partition_list = get_partitions()
    for device in partition_list:
        device = '/dev/' + device
        if device in fstype:
            # Filesystem is due to be formatted; honour the desired type.
            device_list[device] = fstype[device]
            continue
        filesystem_pipe = subprocess.Popen(['file', '-s', device], stdout=subprocess.PIPE)
        filesystem = filesystem_pipe.communicate()[0]
        if re.match('.*((ext3)|(swap)|(data)).*', filesystem, re.I):
            if 'ext3' in filesystem.split() or 'data' in filesystem.split():
                device_list[device] = 'ext3'
            elif 'swap' in filesystem.split():
                device_list[device] = 'linux-swap'
            elif 'FAT' in filesystem.split():
                device_list[device] = 'vfat'
            elif 'NTFS' in filesystem.split():
                device_list[device] = 'ntfs'
    return device_list


def get_default_partition_selection(size, fstype):
    """Return a default partition selection as a dictionary of
    {mountpoint: device}. The first partition with the biggest size and a
    reasonable POSIX filesystem will be marked as the root selection, and
    the first swap partition will be marked as the swap selection."""

    # Produce a list from size dict ({device: size}) ordered from largest to
    # smallest; devices we've just formatted take precedence.
    new_devices = [d for d in size.keys() if ('/dev/%s' % d) in fstype]
    new_devices.sort(None, lambda dev: size[dev], True)
    old_devices = [d for d in size.keys() if ('/dev/%s' % d) not in fstype]
    old_devices.sort(None, lambda dev: size[dev], True)

    # getting filesystem dict ({device: fs})
    device_list = get_filesystems(fstype)

    # building an initial mountpoint preselection dict. Assigning only
    # preferred partitions for each mountpoint (the highest ext3 partition
    # to '/' and the first swap partition to swap).
    selection = {}
    if len(device_list.items()) != 0:
        root, swap = 0, 0
        for partition in new_devices + old_devices:
            size_selected = size[partition]
            try:
                fs = device_list['/dev/%s' % partition]
            except:
                continue
            if swap == 1 and root == 1:
                break
            elif (fs in ('ext2', 'ext3', 'jfs', 'reiserfs', 'xfs') and
                  size_selected > 1024):
                if root == 0:
                    selection['/'] = '/dev/%s' % partition
                    root = 1
            elif fs == 'linux-swap':
                selection['swap'] = '/dev/%s' % partition
                swap = 1
            else:
                continue
    return selection


_supported_locales = None

def get_supported_locales():
    """Returns a list of all locales supported by the installation system."""
    global _supported_locales
    if _supported_locales is None:
        _supported_locales = {}
        supported = open('/usr/share/i18n/SUPPORTED')
        for line in supported:
            (locale, charset) = line.split(None, 1)
            _supported_locales[locale] = charset
        supported.close()
    return _supported_locales


_translations = None

def get_translations():
    """Returns a dictionary {name: {language: description}} of translatable
    strings."""
    global _translations
    if _translations is None:
        _translations = {}
        devnull = open('/dev/null', 'w')
        db = subprocess.Popen(
            ['debconf-copydb', 'templatedb', 'pipe',
             '--config=Name:pipe', '--config=Driver:Pipe',
             '--config=InFd:none', '--pattern=^(ubiquity|partman)'],
            stdout=subprocess.PIPE, stderr=devnull)
        question = None
        descriptions = {}
        fieldsplitter = re.compile(r':\s*')

        for line in db.stdout:
            line = line.rstrip('\n')
            if ':' not in line:
                if question is not None:
                    _translations[question] = descriptions
                    descriptions = {}
                    question = None
                continue

            (name, value) = fieldsplitter.split(line, 1)
            if value == '':
                continue
            name = name.lower()
            if name == 'name':
                question = value
            elif name.startswith('description'):
                namebits = name.split('-', 1)
                if len(namebits) == 1:
                    lang = 'c'
                else:
                    lang = namebits[1].lower()
                    # TODO: recode from specified encoding
                    lang = lang.split('.')[0]
                descriptions[lang] = value.replace('\\n', '\n')
            elif name.startswith('extended_description'):
                namebits = name.split('-', 1)
                if len(namebits) == 1:
                    lang = 'c'
                else:
                    lang = namebits[1].lower()
                    # TODO: recode from specified encoding
                    lang = lang.split('.')[0]
                if lang not in descriptions:
                    descriptions[lang] = value.replace('\\n', '\n')

        db.wait()
        devnull.close()

    return _translations

def get_string(name, lang):
    """Get the translation of a single string."""
    translations = get_translations()
    if name not in translations:
        return None

    if lang is None:
        lang = 'c'
    else:
        lang = lang.lower()

    if lang in translations[name]:
        text = translations[name][lang]
    else:
        lang = lang.split('_')[0]
        if lang in translations[name]:
            text = translations[name][lang]
        else:
            text = translations[name]['c']

    return text


# vim:ai:et:sts=4:tw=80:sw=4:
