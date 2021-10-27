# redway

Color temperature adjustment for Wayland compositors supporting `wlr-gamma-control-unstable-v1`.

# Why?

`redway` lacks automation.

`redway` facilitates manual control.

This combination of features appeared unavailable for [Sway](https://swaywm.org), so I forked [wlsunset](https://github.com/kennylevinsen/wlsunset) to create `redway`.

# How to build and install

```
meson build
ninja -C build
sudo ninja -C build install
```

# How to use

`redway {temperature}`
A `USR1` signal will increase the color temperature.
A `USR2` signal will decrease the color temperature.

## Example

```
# Start with a slightly warm color.
redway 5700

# Make the color bluer.
kill -SIGUSR1 $(pgrep redway)

# Make the color redder.
kill -SIGUSR2 $(pgrep redway)
```

## Suggestion

Bind `SIGUSR1` and `SIGUSR2` signals to keyboard shortcuts.
