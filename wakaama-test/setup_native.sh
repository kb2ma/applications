# Set up and tear down TAP networking.
# Must run as superuser.

start() {
    sudo ip tuntap add tap0 mode tap user kbee

    # Set HW address to similar format to the IP address we want for testing.
    sudo ip link set dev tap0 address 0:0:bb:bb:0:1

    # Hardware-based address
    sudo ip address add fe80::200:bbff:febb:1/64 dev tap0 scope link

    # Add unique local address so we don't rely on link-local address
    sudo ip address add fd00:bbbb::1/64 dev tap0 scope global

    sudo ip link set tap0 up
}

stop() {
    sudo ip link set tap0 down
    sudo ip link delete tap0
}

if [ -n "$1" ]; then
    action="$1"
    if [ "$action" = "start" ]; then
        start
        ifconfig
        exit 0
    elif [ "$action" = "stop" ]; then
        stop
        echo "stopped"
        exit 0
    fi
fi

echo "Must specify action: (start|stop)"
exit 1
