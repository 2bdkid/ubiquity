#!/bin/sh

STATEDIR=/var/lib/initramfs-tools

get_sorted_versions()
{
        version_list=""

        for gsv_x in "${STATEDIR}"/*; do
                gsv_x="$(basename "${gsv_x}")"
                if [ "${gsv_x}" = '*' ]; then
                        return 0
                fi
                worklist=""
                for gsv_i in $version_list; do
                        if dpkg --compare-versions "${gsv_x}" '>' "${gsv_i}"; then
                                worklist="${worklist} ${gsv_x} ${gsv_i}"
                                gsv_x=""
                        else
                                worklist="${worklist} ${gsv_i}"
                        fi
                done
                if [ "${gsv_x}" != "" ]; then
                        worklist="${worklist} ${gsv_x}"
                fi
                version_list="${worklist}"
        done
}

highest_installed_version()
{
        get_sorted_versions
        if [ -z "${version_list}" ]; then
                return
        fi
        set -- ${version_list}
        echo ${1}
}

installed="$(highest_installed_version)"

if dpkg --compare-versions "${installed}" '>' "${1}"; then
	echo "Warning, there is already a newer kernel installed, default is now $1"
	echo "If you instead want to use the latest installed kernel on your system, call"
	echo "sudo flash-kernel ${installed}"
fi
