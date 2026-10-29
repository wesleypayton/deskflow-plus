# Free-form layout and file transfer rework

This fork replaces the fixed 5x3 computer grid with a free-moving, snap-based
layout (similar to ShareMouse) and adds Mouse Without Borders style file
transfer. Windows is the only supported platform for the new features.

## Goals

* Computers are placed anywhere on a canvas, drawn as their real monitor
  arrangement, and dragged as a group. Edges and centres snap to neighbours.
* Cursor transitions follow geometry: wherever two computers touch, the cursor
  crosses 1:1 along the shared span.
* Files dragged across an edge are dropped on the destination computer, and
  files copied with Ctrl+C can be pasted with Ctrl+V on another computer.
* Stock Deskflow clients keep working (no monitor shapes, no file transfer).

## Phases

### 1. Core layout model

A new `layout` section in the server config gives each computer a rectangle in
one shared coordinate space:

```
section: layout
	desk   = 0,0,1920,1080
	laptop = 1920,180,1440,900
end
```

When the config is loaded, `Config::applyLayout()` turns every pair of touching
rectangles into fractional links (the same links the `links` section produces),
so `Server` needs no changes for this phase. A config may contain `layout` or
`links`, not both. Rectangles may not overlap.

The rectangle sizes are the sizes the user saw when arranging the layout. If a
client's real resolution differs, the topology stays as designed and the
fractional mapping scales onto the real shape.

### 2. Monitor geometry over the wire

* Done: protocol 1.9 adds `DMON`, the client's monitor rectangles, sent
  straight after every `DINF` (so also whenever displays change).
  `IComputer::getMonitors()` defaults to the whole shape; Windows enumerates
  real monitors. Old clients negotiate 1.8 and are treated as one rectangle.
  Risk: if upstream Deskflow ever ships its own 1.9, the two would disagree,
  and the version number would need to move.
* Server publishes known computer shapes and monitor rectangles to the GUI over
  IPC; the GUI caches the last known shapes in settings for offline computers.
* `layout` entries gain optional per-monitor rectangles. The server detects
  *exposed* monitor edges (edges inside the bounding box, e.g. a short monitor
  next to a tall one) using hook deltas when the cursor is pinned at a monitor
  edge, and links from those edges too.

### 3. GUI canvas

Done:

* `ComputerLayoutView` (a `QGraphicsView`) replaces the `ComputerSetupView`
  grid; `ComputerSetupModel` stays as the owner of the computer list.
* Each computer is drawn as its monitors and dragged as one piece. Edges and
  centres snap to other computers within 12 screen pixels, a drop that overlaps
  another computer springs back, and the edges the cursor can cross are drawn
  in green, live while dragging.
* The view zooms to fit. Double click or the context menu edits a computer;
  Delete, the context menu, or dragging it off the canvas onto the trash removes
  it; dropping the new computer tile adds one where it lands.
* Each computer's layout rectangle and monitors are stored with its
  `internalConfig/screens` entry. Settings from before the canvas are converted
  once from the grid, pushing cells toward the server so its grid neighbours
  touch it; computers added later are placed touching the server.
* The server's own monitors come from `EnumDisplayMonitors` in the GUI. Clients
  are 1920x1080 until phase 2's IPC tells the GUI their real shapes.
* `ServerConfig` writes `section: layout` instead of `section: links`, leaving
  out (with a warning) any computer that overlaps one already written so the
  server still starts.

Open: the core isn't DPI aware, so on a scaled display it sees a uniformly
scaled desktop while the GUI sees real pixels. Links are proportional so this
doesn't break crossing, but sizes drawn for clients (reported by the core) and
for the server (read by the GUI) won't match until both use the same units.

### 4. File transfer (Windows)

* Revive the deprecated `DDRG`/`DFTR` messages on protocol 1.9, sent over a
  separate chunked stream so large files don't stall input.
* Drag: when the cursor leaves the server during an OLE drag, a hidden drop
  target window at the edge captures the `CF_HDROP` list; on button release on
  the client the files are streamed and written to a "Received files" folder,
  which is then opened (as Mouse Without Borders does).
* Copy/paste: `CF_HDROP` on the clipboard is announced as a file list; on paste
  the destination requests the data and materialises the files, then places a
  `CF_HDROP` for them on its own clipboard.
* Size limit and destination folder are settings.
