# Default Makefile, for host native GNRC-based networking

# name of your application
APPLICATION = nano_block_client

# If no BOARD is found in the environment, use this default:
BOARD ?= native

# This has to be the absolute path to the RIOT base directory:
RIOTBASE ?= $(CURDIR)/../../../riot/repo

BOARD_INSUFFICIENT_MEMORY := arduino-duemilanove arduino-mega2560 arduino-uno \
                             chronos mega-xplained msb-430 msb-430h nucleo-f030r8 \
                             nucleo-f031k6 nucleo-f042k6 nucleo-f303k8 \
                             nucleo-f334r8 nucleo-l031k6 nucleo-l053r8 \
                             stm32f0discovery telosb waspmote-pro z1

# Include packages that pull up and auto-init the link layer.
# NOTE: 6LoWPAN will be included if IEEE802.15.4 devices are present
USEMODULE += gnrc_netdev_default
USEMODULE += auto_init_gnrc_netif
# Specify the mandatory networking modules
USEMODULE += gnrc_ipv6_default
USEMODULE += gnrc_udp
USEMODULE += gnrc_sock_udp

USEMODULE += nanocoap_sock

# Additional networking modules that can be dropped if not needed
USEMODULE += gnrc_icmpv6_echo
#USEMODULE += gnrc_rpl

# Required by app
USEMODULE += od
# Add also the shell, some shell commands
USEMODULE += shell
USEMODULE += shell_commands

# Comment this out to disable code in RIOT that does safety checking
# which is not needed in a production environment but helps in the
# development process:
DEVELHELP ?= 1

# Change this to 0 show compiler invocation lines by default:
QUIET ?= 1

include $(RIOTBASE)/Makefile.include
