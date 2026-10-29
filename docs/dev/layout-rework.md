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

* Done: Deskflow Plus computers find each other without changing the protocol
  version, which stays at upstream's 1.8. The server adds a `DPLF` option with
  its feature bits to the options it sends every client (stock clients ignore
  unknown options) and a Deskflow Plus client answers with `DPLS`. Only then
  does either side use the new messages, so stock peers never see one.
* Done: `DMON`, the client's monitor rectangles, follows every `DINF` once the
  handshake is done (so also whenever displays change).
  `IComputer::getMonitors()` defaults to the whole shape; Windows enumerates
  real monitors. Stock clients are treated as one rectangle.
* Done: the server sends the GUI `computerShape=<name>|x,y,w,h;...` over the
  core IPC when a client connects and whenever its monitors change. The GUI
  saves them with the computer, so offline computers keep their last known
  shape. A computer that grows is anchored on whichever corner keeps it clear
  of its neighbours. The server's own monitors are read by the GUI directly.
* `layout` entries gain optional per-monitor rectangles. The server detects
  *exposed* monitor edges (edges inside the bounding box, e.g. a short monitor
  next to a tall one) using hook deltas when the cursor is pinned at a monitor
  edge, and links from those edges too.

### 3. GUI canvas

Done:

* `ComputerLayoutView` (a `QGraphicsView`) replaces the `ComputerSetupView`
  grid; `ComputerSetupModel` stays as the owner of the computer list.
* Each computer is drawn as its monitors, in its own colour from a fixed
  palette that works in light and dark themes (the server is always blue and
  marked "This computer"). Several monitors share a tinted outline, are
  numbered, and carry the name on the largest. It is dragged as one piece. Edges and
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
* The server's own monitors come from `EnumDisplayMonitors` in the GUI. A
  client is 1920x1080 until it first connects and the server reports its shape.
* `ServerConfig` writes `section: layout` instead of `section: links`, leaving
  out (with a warning) any computer that overlaps one already written so the
  server still starts.

Both the core and the GUI are `QApplication`s, which Qt makes per-monitor DPI
aware on Windows, so monitor sizes from either are in the same real pixels.

### 4. File transfer (Windows)

Done:

* Transfer engine (`deskflow/FileTransfer`): files stream from disk as a header
  per file and 64 KB chunks, with at most 16 messages unacknowledged so file
  data never queues up in front of input. The receiver writes `.part` files and
  renames them when complete, never overwrites (it picks "name (2).ext"),
  enforces a size limit, and refuses relative paths that could leave its folder
  or that Windows would quietly rename.
* Messages `DFOF` offer, `DFRQ` request, `DFST` start (with the total size),
  `DFHD` header (a file, or an empty folder), `DFDT` data, `DFEN` end, `DFAK`
  ack, `DFCN` cancel and `DFPB` probe, used once the Plus handshake is done. Every file goes
  source -> server -> destination. Each computer's `TransferAgent` offers files
  and only ever sends its own offers, referenced by id, so the server can't ask
  a client for arbitrary paths. The server's `TransferRouter` only relays a
  message from the right end of a transfer.
* Copy/paste: copying files in Explorer offers them; copying anything else
  withdraws the offer. When the cursor enters another computer, the clipboard
  owner's files (up to the "Copied files limit" setting, 512 MB by default) are copied to `%TEMP%\Deskflow\Clipboard\...`
  there and put on its clipboard, marked as Deskflow's so the sync doesn't bounce
  them back.

* Drag across an edge: switching is normally blocked while a button is held.
  When a held button is pushed against an edge that leads somewhere, an almost
  invisible drop target is put under the cursor and the cursor nudged so the
  drag loop reports what it carries. Files are offered, the drag is ended on the
  drop target with a release of our own (it accepts nothing, so the source
  neither moves nor copies), and the cursor crosses with the button held.
  Releasing it on another computer drops the files into the Explorer folder or
  desktop under the cursor, otherwise into `Downloads\Deskflow`, which opens
  with them selected. Drags that start on a client use the `DFPB` probe message.

Verified live on one PC (server and client cores on loopback): copying three
items client to server, and dragging them server to client into the fallback
folder. Not yet verified live: dropping into an open Explorer folder or onto the
desktop, drags starting on a client, and two real machines.

Since then:

* Empty folders are copied, the copied files limit is a setting, and transfers
  report progress and errors to the GUI (status bar and tray notifications).
* A running server reloads its config (new `reloadConfig` core IPC command) when
  a client's reported monitors change the layout, and reported shapes survive
  the server settings dialog being saved.
* Drops onto a Windows 11 Explorer window go into its visible tab.

Still to do:

* Two-way control: a client's own mouse and keyboard crossing to the server
  and on to other computers, as in Mouse Without Borders. Needs input capture
  that can be switched on and off in a client process, and messages carrying
  input from client to server.
* Crossing between individual monitors rather than each computer's outline.
