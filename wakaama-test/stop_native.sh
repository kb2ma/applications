# Tears down tap interface created with start_native.sh
# Must run as superuser

sudo ip link set tap0 down
sudo ip link delete tap0
