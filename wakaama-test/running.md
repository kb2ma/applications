# Running Wakaama / Leshan

As of 2019-11-07, must use kb2ma pr/pkg/wakaama_rework branch. Includes rebase on master, and fix to get that to work.

## Local Leshan networking

### Native client
Setup for a single Linux native client node connected via TAP to the server node. Build with `Makefile.native`.

```
Wakaama node
Linux native
fd00:bbbb::2/64 (in Makefile)
   |
  TAP
   |
Leshan node
workstation (Linux)
fd00:bbbb::1/64 (in start_native.sh)
```

Must manually run `start_native.sh` to setup TAP, and `stop_native.sh` to tear it down.

### Single board client via USB (local networking)
Setup for a single physical board client node connected via USB/TAP to the server node. I use a samr21-xpro. Build with `Makefile.ula`.
```
Wakaama node
board (samr21-xpro)
fd00:bbbb::2/64 (via CLI)
   |
  USB/TAP
   |
Leshan node
workstation (Linux)
fd00:bbbb::1/64 (in start_ula.sh)
```
`start_ula.sh` executes automatically when running the `term` target, to setup and teardown the TAP interface.

After running the `term` target on the board, do the following:

**board**
```
   > ifconfig 5 add fd00:bbbb::2/64
   > nib neigh add 5 fd00:bbbb::1 <ether addr>
```
where <ether addr\> is the 6 hexadecimal colon separated MAC address of the tap interface

**workstation**
```
   $ sudo ip route add fd00:bbbb::/64 via <lladdr> dev tap0
```
where <lladdr\> is the link local address of the samr21-xpro


### Single board client via WiFi (Internet networking)
Setup for a single physical board client node connected via WiFi to a server node in the cloud. I use ESP-12x (8266) Adafruit Feather board because it has WiFi on board, and so does not require a USB connection to an Internet gateway, like the samr21-xpro. This approach means we expect the WiFi access point to provide IPv6 Internet connectivity. Build with `Makefile.inet`.
```
Wakaama node
board esp-12x
routeable IPv6 addr (auto-assigned)
   |
  WiFi
   |
Leshan node
cloud instance (Linux)
routeable cloud address (in setup_ula.sh)
```

Must define environment variables for `Makefile.inet`:
```
export RIOT_WIFI_SSID="<ssid>"
export RIOT_WIFI_PASS="<passphrase>"
export SERVER_URI=\\\"coap://[<server addr>]

```


## With Bootstrap Server
Must add client configuration via web UI before it attempts to connect. See "Add new client bootstrap configuration" button in web UI.