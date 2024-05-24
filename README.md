# dwm - dynamic window manager

dwm is an extremely fast, small, and dynamic window manager for X11.

This repo is a fork of dwm made for myself, I don't recommend using it similar to how I don't recommend ever touching dwm.

## Patches

The code has been completley mangled and taking a diff with the original will not give you anything useful. Here are the patches which were applied, they all have merge conflicts and have to be edited / rewritten to work at all.

* [https://dwm.suckless.org/patches/barpadding/dwm-barpadding-20211020-a786211.diff](https://dwm.suckless.org/patches/barpadding/dwm-barpadding-20211020-a786211.diff)
* [https://dwm.suckless.org/patches/cool_autostart/dwm-cool-autostart-20240312-9f88553.diff](https://dwm.suckless.org/patches/cool_autostart/dwm-cool-autostart-20240312-9f88553.diff)
* [https://dwm.suckless.org/patches/fixmultimon/dwm-fixmultimon-6.4.diff](https://dwm.suckless.org/patches/fixmultimon/dwm-fixmultimon-6.4.diff)
* [https://dwm.suckless.org/patches/pango/dwm-pango-20230520-e81f17d.diff](https://dwm.suckless.org/patches/pango/dwm-pango-20230520-e81f17d.diff)
* [https://dwm.suckless.org/patches/steam/dwm-steam-6.2.diff](https://dwm.suckless.org/patches/steam/dwm-steam-6.2.diff)
* [https://dwm.suckless.org/patches/clientindicators/dwm-clientindicators-6.2.diff](https://dwm.suckless.org/patches/clientindicators/dwm-clientindicators-6.2.diff)
* [https://dwm.suckless.org/patches/fancybar/dwm-fancybar-20220527-d3f93c7.diff](https://dwm.suckless.org/patches/fancybar/dwm-fancybar-20220527-d3f93c7.diff)
* [https://dwm.suckless.org/patches/moveresize/dwm-moveresize-20221210-7ac106c.diff](https://dwm.suckless.org/patches/moveresize/dwm-moveresize-20221210-7ac106c.diff)
* [https://dwm.suckless.org/patches/fullgaps/dwm-fullgaps-6.4.diff](https://dwm.suckless.org/patches/fullgaps/dwm-fullgaps-6.4.diff)
* [https://dwm.suckless.org/patches/alpha/dwm-alpha-20230401-348f655.diff](https://dwm.suckless.org/patches/alpha/dwm-alpha-20230401-348f655.diff)
* [https://dwm.suckless.org/patches/alwaysontop/alwaysontop-6.2.diff](https://dwm.suckless.org/patches/alwaysontop/alwaysontop-6.2.diff)
* [https://dwm.suckless.org/patches/winicon/dwm-winicon-6.3-v2.1.diff](https://dwm.suckless.org/patches/winicon/dwm-winicon-6.3-v2.1.diff)
* [https://dwm.suckless.org/patches/columngaps/dwm-columngaps-20210124-f0792e4.diff](https://dwm.suckless.org/patches/columngaps/dwm-columngaps-20210124-f0792e4.diff)


## Requirements
* XLib
* XFT
* Pango
* Imlib2

## Installation

Compile configuration is in `config.mk`, the default install directory is `/usr/local`

You can compile and install with
```sh
make clean install
```

## Running

Add the following line to your .xinitrc to start dwm using startx:
```sh
exec dwm
```

The root / status text can be updated using `xsetroot`
```sh
while true; do
    xsetroot -name "$(date)"
done
```

## Configuration

Configuration can be found in `config.h`, a default config can be found in `config.def.h`
