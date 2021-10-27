redway(1)

# NAME

redway - Color temperature adjustments for Wayland compositors supporting
wlr-gamma-control-unstable-v1

# SYNOPSIS

*redway* _temperature_

# DESCRIPTION

redway sets the display(s) to the given _temperature_ while running. _temperature_ is nominally in degrees Kelvin, but accuracy is not guaranteed. Sensible values might range from 3300 (yellowish, as with Edison bulbs) to 6500 (bluish, as with overcast skies).

A USR1 signal will increase the color temperature.

A USR2 signal will decrease the color temperature.

# EXAMPLES

```
# Start with a slightly warm color.
redway 5700

# Make the color bluer.
kill -SIGUSR1 $(pgrep redway)

# Make the color redder.
kill -SIGUSR2 $(pgrep redway)
```

# AUTHORS

Forked from wlsunset as maintained by Kenny Levinsen <contact@kl.wtf> [https://sr.ht/~kennylevinsen/wlsunset], commit f32f6963b9. Many features were removed, including geolocation automation.
