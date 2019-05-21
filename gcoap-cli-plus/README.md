This application provides command line access to gcoap, a high-level API for
CoAP messaging. See the [CoAP spec][1] for background, and the
Modules>Networking>CoAP topic in the source documentation for detailed usage
instructions and implementation notes.

This application extends the [gcoap example][2] from the RIOT distribution. See the
documentation for that example for background.

## Block size

Adds a "-b size" option to specify the block size for a request.

[1]: https://tools.ietf.org/html/rfc7252    "CoAP spec"
[2]: https://github.com/RIOT-OS/RIOT/tree/master/examples/gcoap    "gcoap example"
