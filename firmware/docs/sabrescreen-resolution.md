# Media resolution

Sabrescreen uses each selected image/video's angular and radial dimensions.
The physical LED list remains in the robot config and is independent of media
resolution. Nearest angular and radial samples are selected for each LED;
the outermost LED on each side maps to that side's outermost radial sample.

Hardcoded arrays keep their existing API. Both dimensions are inferred:

```cpp
const CRGB logo[180][32] = { /* ... */ };
show_still(logo);
```

For contiguous angle-major RGB data with runtime dimensions:

```cpp
show_still(pixels, num_angles, num_radii);
```

Still pixels are copied, so the source can be released after a successful call.
Videos use `SabreVid::numAngles` and `numRadii`, with a mask stride of
`ceil(numAngles * numRadii / 4)` bytes per frame. Existing generated headers
with two-dimensional mask arrays and six-value `SabreVid` initialisers still
work; their array stride is checked when selected. The video object, masks,
and diff arrays must stay alive while the video is selected.

Frame buffers are allocated for the selected dimensions, with a second frame
only for video decoding. Radial maps use 16-bit indices, allowing more than
256 radial samples. Dimensions must be nonzero and fit in 16 bits. Available
heap limits the total pixel count; zero/invalid dimensions or failed allocations
return false, print a serial error, and leave the previous selection unchanged.
Switching requires enough memory for the old and new media during handoff.

Selections may be queued from core 1 before or after screen setup. Allocation,
mapping, initial video decoding, and still-image copying occur outside the
request mutex. Core 0 adopts the prepared frame at its next screen update and
owns all subsequent rendering and video decoding. Flash requests use the same
handoff, so core 1 does not change display buffers while core 0 renders them.
Only pointers/flags are exchanged under the lock; the render loop does not wait
for a busy request lock.

The `NUM_ANGLES`, `NUM_SOURCE_RINGS`, and `MASK_BYTES_PER_FRAME` constants in
config remain legacy exporter defaults. They no longer determine screen buffers
or constrain the resolution of selected media. This change adds rendering
support, not USB media discovery or a binary file loader.
