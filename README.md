# redway

Color temperature adjustment for Wayland compositors supporting `wlr-gamma-control-unstable-v1`.

# Why?

`redway` facilitates manual control of color temperature, contrast, and gamma. This allows rapid accommodation of changing content and environment.

For example, you might want an extremely dim, extremely red display for use in a completely dark room; then move to a mildly warm artificial light; then have a deep red sunrise come in through the window; followed by a strongly blue overcast as clouds roll in.

Or, you might want very low contrast for working with text, then normal contrast for watching a video, then increase shadow contrast to quickly see what's going on in that too-dark photo someone just sent you.

# How to build and install

```
meson build
ninja -C build
sudo ninja -C build install
```

# How to use

`redway` accepts input upon initial invocation, from another invocation, from `SIGUSR1` and `SIGUSR2` (to increase and decrease temperatures), and from a FIFO that is typically located at `/run/user/{UID}/redway/in`.

Associating these commands with keyboard shorcuts is highly recommended.

See the man page for full details. The unprocessed `redway.1.sct` is fairly legible if you want to review it without downloading and building.

## Examples

```
# Start with a slightly warm color.
redway 5700

# Make the color bluer.
kill -SIGUSR1 $(pgrep redway)

# Make the color redder.
kill -SIGUSR2 $(pgrep redway)
```

See the man page (redway.1.sct) for more examples, including my Sway configuration.

# Status
Very, very buggy. Not at all technically precise or color-safe. It does what I want better than anything else I've seen, though, including my previous attempts at doing effectively the same thing.

# Application note: viewing comfort
`redway` is intended to help make displays be more comfortable to look at for extended periods of time. This note discusses a few details that might help you use this tool more effectively.

## Conceptual overview

High contrast is not always desirable. Strong contrasts -- bright whites, deep blacks -- are pleasing because they allow the mind to quickly form an overview understanding of the thing being looked at. If you ever want someone to like the look of something within their first few milliseconds of looking at it, use high contrast.

However, if you will be looking at something for longer than that, such as looking at computer displays for most of the day, high contrast causes strain. Specifically: bright views excite your optical sensors, which can grow tired from the exertion. To reduce the level of stimulation, your retina reduces in size to block out part of the light. This also decreases the brightness of the dark parts of your view, which makes shadows more difficult to distinguish, which also causes strain.

The ideal contrast range for long-term viewing is, therefore, one where whites are dim enough to look at directly while the blacks remain easy to distinguish. This is why some people prefer low-contrast color themes, such as [Solarized](https://en.wikipedia.org/wiki/Solarized).

An often-overlooked problem is that "contrast range" applies to a person's entire field of view, not just the portion of it to which they are paying attention. If you have a bright window behind a dark monitor, or a bright monitor in a dark room, your eyes will eventually grow tired. The effect is slight and may only be noticed as a growing aversion to maintaining the same field of view.

A subtler problem is that "contrast" applies cumulatively to both dark/light contrast and color contrast. Opposites on the color wheel make each other more apparent: for example, the green (which has little red and blue) leaves of a flower make its magenta (which has red and blue, but not green) appear much stronger. This is relevant when your computer display does not have a compatible sense of "white" as does the rest of your environment. The resulting color contrast seems to create mental subject-differentiation stress much like the physical tension inherent in dark/light contrast.

## How other tools can help (try these first)

Identify your environment's overall brightness and color.

If you have a bright environment, your display should be about as bright -- change the brightness setting. Note that modern external monitors often respond to DDC controls, which can be set by the computer so you don't have to reach for those awful buttons; `redway` does not have this feature, but `ddcutil` and `ddccontrol` are two examples of such programs.

Similarly, if you are a dim or outright dark environment, your display should also be about as dark yet bright enough to be easy to read. While `redway` can dim a display (contrast < 0), this is not recommended. If another program writes the same settings as `redway`, the display will revert to full brightness. If your display has a backlight, it will still be consuming that full amount of power. If your pixels have gaps between them -- and they almost always do -- the backlight will still shine through.

Finally, if your preferred applications support themes, use a "dark" theme in dim environments and a "light" theme in bright environments. By convention, the "dark" and "light" refer to the background color. You can minimize useless contrast by setting your display background to match your environment background.

## How `redway` can help

While general settings can arrive at vaguely correct results, they are often limited to one application or environment at a time. `redway` changes the entire Wayland color look-up-table, which might be implemented by changing kernel's LUT, which might change not only Wayland but also the virtual consoles.

If your display's brightness controls do not range dark enough, `redway` can further dim the display by applying contrast < 0, which dims the white point.

If your environment is of a different color than the graphic design rooms of some print publications (which target 6500 degrees), `redway` can adjust to a different temperature.

If your monitor or the content you are working with is too light or too dark, but the problem is not the white point or black point, a gamma modification may help. For example, if you are watching a movie in a dark room with minimum brightness, but the movie has so many dark scenes it is difficult to tell what's happening, setting gamma < 1.0 will brighten the shadows.

## How to choose a color temperature or contrast

On your display, open a white window with black text or other high-contrast content. Make it large, even full-screen. Look at the corner of the display. Now, look past it and at the things behind it. Your goal will be for the overall color and contrast on your display to match the environment behind it.

An exact match is likely impossible, but get close and return to what you were doing. If you notice that the color or contrast is no longer satisfying -- which can happen with changes in your environment, with changes in your eye, and/or with changes in how your mind is processing its visual input -- go back to trying new changes.

Keyboard shortcuts are immensely useful because they allow you to customize your view while looking at what you want to change, without leaving its graphic context.

I prefer for my display's color temperature to be warmer than my room's, and for my contrast to be rather low, similar to newspaper or paperback-book print. I generally do not modify gamma unless I am working with graphical material, in which case the nature of that material determines what adjustments I make.

# Application note: graphics

Modifying the display's gamma will make it easier to inspect the shadows (gamma < 1.0) and highlights (gamma > 1.0) of an image without changing the image itself.

Lifting shadows (contrast > 0) may provide a basic preview of how an image will appear on matte paper.

However, this program is not intended to be visually "correct." The colors displayed at a given color temperature may have little to do with a physical object at that temperature. In no way are colors managed for accuracy or even reliable transformation; transformations are not guaranteed from one version to another, even across minor releases. Please do not use this program in any color-critical application.

