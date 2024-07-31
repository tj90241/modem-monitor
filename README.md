# modem-monitor

This is a connection manager for cellular modems that I wrote after wanting
something that had tighter integration with my WWAN modem than what I found
to be offered by the open-source `modem-manager` project -- even at the
expense of linking against libraries which are not freely available or under
more restrictive licensing terms.

## Backstory

My parents live full-time in their RV, and I'm the "tech guy" on the hook for
making sure they have a working internet connection. After having issues with
existing open-source solutions, I decided to write my own that was both simple
and pedantic.

Enter `modem-monitor`.

`modem-monitor` has no configuration file or options, and only does exactly
what _I_ need it to: maintain a dual-stack IPv4/v6 data connection with a
single provider (Verizon), and setup some static routes/wireguard tunnels/
DNS/NTP even in the face of an unstable connection. So far, it works great at
doing just that.

I hope you find it useful, too.

## MBPL headers and libraries

This modem connection manager has only been tested with the Sierra Wireless
MBPL (Mobile Broadband Package for Linux) SDK. Said SDK is licensed under
different terms than what appear in this repository and must be obtained
separately. Register with an account on Sierra Wireless's website and agree
to their terms at your own option.

The SDK package currently tested is: `MBPL_SDK_R35_ENG4-lite.bin.tar`. Once
you have obtained that software, copy the following directories from it into
the `swiinc/` directory within this repository as follows:
```
lite-common/inc/ -> swiinc/common/
lite-mbim/inc/ -> swiinc/mbim/
lite-qmi/inc/ -> swiinc/qmi/
lite-qmux/inc/ -> swiinc/qmux/
pkgs/qm/ -> swiinc/qm/
```

Next, similarly copy libraries into `swilib/` for your desired host/arch:
```
lite-common/lib/$(ARCH)/libcommon.a -> swilib/libcommon.a
lite-mbim/lib/$(ARCH)/liblite-mbim.a -> swilib/liblite-mbim.a
lite-qmi/lib/$(ARCH)/liblite-qmi.a -> swilib/liblite-qmi.a
lite-qmux/lib/$(ARCH)/liblite-qmux.a -> swilib/liblite-qmux.a
```

You may then build the software as one normally would with CMake.

## Operation

As written, `modem-monitor` assumes you have `chrony` and `unbound` configured
for DNS/NTP, respectively. It will restart both of them once the data session
becomes available. As such, both services should be configured, but set to a
disabled when the system boots -- they'll be managed entirely by
`modem-monitor` via `sd-bus`.

Start `modem-monitor` and leave it running. An included `systemd` unit file
may be leveraged to have `systemd` restart the service if it crashes for any
reason.
