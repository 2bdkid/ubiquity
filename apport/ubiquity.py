# apport hook for ubiquity; adds various log files

import os.path

def add_info(report):
    if os.path.exists('/var/log/syslog'):
        report['UbiquitySyslog'] = ('/var/log/syslog',)
    if os.path.exists('/var/log/partman'):
        report['UbiquityPartman'] = ('/var/log/partman',)