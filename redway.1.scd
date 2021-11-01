redway(1)

# NAME

redway - Color temperature adjustments for Wayland compositors supporting
wlr-gamma-control-unstable-v1

# SYNOPSIS

*redway* [[t] _temp_ | +|-] [g _gamma_modifier_ | +|-] [c _contrast_ | +|-]

# DESCRIPTION
Without any arguments or a running server, *redway* launches a foreground server and sets the default settings, currently set at compile time to a slightly warm 5600 degrees. The server will remain running in the foreground.

If *redway* is invoked when a server is already running, it will pass its arguments verbatim to the FIFO input of the server and exit immediately. See *INPUT*, below.

*redway* [t{emperature}] _temp_   will set the color temperature to _temp_. The optional keyword _t_ may be capitalized and may be followed by any alphabetical or blank character ([a-zA-Z \\t\\n]), such that "temperature" may be spelled out to make scripts more readable. The color temperature is bounded within 1200 degrees (very red) to 20,000 degrees (blue/violet).

*redway* g{amma} _gamma_modifier_   will modify the curve by which contrast is rendered. Lower values increase contrast in the shadows, lift midtones, and decrease contrast in the highlights. Higher values decrease contrast in the shadows, darken midtones, and increase contrast in the highlights. _gamma_modifier_ is bounded within 0.33 and 4.00. The keyword _g_ may be capitalized and may be followed by any alphabetical or blank character.

*redway* c{ontrast} _contrast_   will modify the range within which contrast is rendered. Negative values decrease the white point; positive values increase the black point; the strength of effect increases with values further from zero. _contrast_ is bounded within -80 and 80. The keyword _c_ may be capitalized and may be followed by any alphabetical or blank character.

*redway* +   will increment color temperature.

*redway* -   will decrement color temperature.

*redway* c+   will increment contrast.

*redway* c-   will decrement contrast.

*redway* g+   will increment gamma_modifier.

*redway* g-   will decrement gamma_modifier.

# INPUT

*redway* accepts four types of input: on server invocation, on client invocation, from signals, and from its FIFO input.

Server and client arguments are discussed above.

SIGUSR1 and SIGUSR2 will increase and decrease color temperature, respectively. Note that different shells may use different syntax: Bash, OpenBSD ksh, and Zsh appear to use a builtin while Dash and Fish appear to use the system utility, while Sway invokes /bin/sh which differs among distributions.

Input may also be passed through a FIFO named 'in' located in the subdirectory 'redway' of the first of these folders found: $XDG_RUNTIME_DIR, $XDG_STATE_HOME, or $HOME/.local/state. Note that argument processing is whitespace-sensitive: arguments with spaces passed through a shell require protection from word splitting, such as by using single quotes.

# EXAMPLES

Start with a slightly warm color:
```
redway 5700
```

Make the color bluer:
```
kill -s USR1 $(pgrep redway)
```

Make the color redder:
```
kill -s USR2 $(pgrep redway)
```

Reduce contrast (for a bright room):
```
echo 'contrast 50' > /run/user/1000/redway/in
```

Increase gamma to clarify tones within reduced contrast range:
```
echo 'gamma 1.5' > /run/user/1000/redway/in
```

Set the color to very red and very dim (for a dark room):
```
redway 1200
redway c -50
```

Reset to neutral:
```
redway 6500
redway c 0
redway g 1
```

Note that at least one style of these commands is likely to be suitable for use in keyboard shortcuts for whatever your preferred interface manager happens to be. On Sway, one implementation might be:
```
bindsym Mod4+XF86MonBrightnessUp           exec "redway tmp+"
bindsym Mod4+XF86MonBrightnessDown         exec "redway tmp-"
bindsym Control+XF86MonBrightnessUp        exec "redway con+"
bindsym Control+XF86MonBrightnessDown      exec "redway con-"
bindsym Mod4+Control+XF86MonBrightnessUp   exec "redway gam-"
bindsym Mod4+Control+XF86MonBrightnessDown exec "redway gam+"
bindsym Mod1+XF86MonBrightnessUp           exec "redway gam1"
bindsym Mod1+XF86MonBrightnessDown         exec "redway con0"
```
Note that increasing gamma generally makes the view darker, so increment/decrement operators are swapped for gamma_modifier.

# AUTHORS
I do not know if any of this code derives from *redshift*, but, as far as I am aware, Jon Lund Steffensen [https://github.com/jonls/redshift] deserves primary credit for creating this entire type of program.

This program was forked from wlsunset as maintained by Kenny Levinsen <contact@kl.wtf> [https://sr.ht/~kennylevinsen/wlsunset], commit f32f6963b9. Many features were removed, including geolocation automation.

The author since the above-mentioned fork, and of this manual, is Jonathan Bakke [https://github.com/jonbakke/redway].
