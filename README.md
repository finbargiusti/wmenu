# wmenu

wmenu is an efficient dynamic menu for Sway and wlroots based Wayland
compositors. It provides a Wayland-native dmenu replacement which maintains the
look and feel of dmenu.

> NOTE: this is my fork of [wmenu](https://codeberg.org/adnano/wmenu). The original project is awesome and I do not
> take credit for it. 

## New Features

1. Added center flag (-c), which centers the menu vertically and horizontally.
1. Added minimum width flag (-w \[number#\]), which is self explanatory, and only
   applies to centered wmenus.

TODO:
1. [ ] Update documentation.

## Installation

Dependencies:

- cairo
- pango
- wayland
- xkbcommon
- scdoc (optional)

```
$ meson setup build
$ ninja -C build
# ninja -C build install
```

## Usage

See wmenu(1)

To use wmenu with Sway, you can add the following to your configuration file:

```
set $menu wmenu-run
bindsym $mod+d exec $menu
```
