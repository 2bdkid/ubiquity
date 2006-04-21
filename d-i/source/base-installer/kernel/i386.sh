arch_get_kernel_flavour () {
	VENDOR=`grep '^vendor_id' "$CPUINFO" | cut -d: -f2`
	FAMILY=`grep '^cpu family' "$CPUINFO" | cut -d: -f2`
	case "$VENDOR" in
		" AuthenticAMD"*)
			case "$FAMILY" in
				" 6")	echo k7 ;;
				" 5")	echo k6 ;;
				*)	echo 386 ;;
			esac
		;;
		" GenuineIntel"|" GenuineTMx86"*)
			case "$FAMILY" in
				" 6"|" 15")	echo 686 ;;
				" 5")		echo 586tsc ;;
				*)		echo 386 ;;
			esac
		;;
		*) echo 386 ;;
	esac
	return 0
}

arch_check_usable_kernel () {
	if expr "$1" : '.*-server.*' >/dev/null; then return 0; fi
	if expr "$1" : '.*-386.*' >/dev/null; then return 0; fi
	if [ "$2" = 386 ]; then return 1; fi
	if expr "$1" : '.*-586.*' >/dev/null; then return 0; fi
	if [ "$2" = 586tsc ]; then return 1; fi
	if [ "$2" = 686 ]; then
		if expr "$1" : '.*-686.*' >/dev/null; then return 0; fi
		return 1
	fi
	if expr "$1" : '.*-k6.*' >/dev/null; then return 0; fi
	if [ "$2" = k6 ]; then return 1; fi
	if expr "$1" : '.*-k7.*' >/dev/null; then return 0; fi

	# default to usable in case of strangeness
	warning "Unknown kernel usability: $1 / $2"
	return 0
}

arch_get_kernel () {
	if [ -n "$NUMCPUS" ] && [ "$NUMCPUS" -gt 1 ]; then
		SMP=-smp
	else
		SMP=
	fi

	if [ "$1" = k7 ]; then
		if [ "$SMP" ]; then
			echo "linux-k7$SMP"
			echo "linux-image-k7$SMP"
		fi
		echo "linux-k7"
		echo "linux-image-k7"
		set k6
	fi
	if [ "$1" = k6 ]; then
		if [ "$KERNEL_MAJOR" = 2.4 ]; then
			echo "linux-k6"
			echo "linux-image-k6"
		fi
		set 586tsc
	fi
	if [ "$1" = 686 ]; then
		if [ "$SMP" ]; then
			echo "linux-686$SMP"
			echo "linux-image-686$SMP"
		fi
		echo "linux-686"
		echo "linux-image-686"
		set 586tsc
	fi
	if [ "$1" = 586tsc ]; then
		if [ "$KERNEL_MAJOR" = 2.4 ]; then
			echo "linux-586tsc"
			echo "linux-image-586tsc"
		fi
		set 386
	fi
	echo "linux-386"
	echo "linux-image-386"

	echo "linux-server"
	echo "linux-image-server"
	echo "linux-server-bigiron"
	echo "linux-image-server-bigiron"
}
