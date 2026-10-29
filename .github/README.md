# Deskflow Plus

**Deskflow Plus** is a fork of [Deskflow](https://github.com/deskflow/deskflow), the free and open source
keyboard and mouse sharing app. Use the keyboard and mouse of one computer to control the computers next
to it, as if their screens were one big desktop: a software KVM, without the video.

Deskflow Plus keeps everything Deskflow does and adds a free-form screen layout and file transfer
between computers, in the spirit of ShareMouse and Mouse Without Borders.

> [!NOTE]
> Deskflow Plus is an independent fork. It is not made or supported by the Deskflow project, so please
> report problems with it [here](https://github.com/wesleypayton/deskflow-plus/issues), not to Deskflow.

## What Deskflow does

- Share one keyboard and mouse between computers on your network, moving across by pushing the cursor
  off the edge of a screen
- Share the clipboard between computers
- Hotkeys, screen locking and switching options
- TLS encryption, on by default
- Windows, macOS, Linux (including Wayland) and BSD

## What Deskflow Plus adds

- **Free-form layout.** The fixed grid of screens is replaced by a canvas where you drag computers anywhere.
  They snap together, and green lines show exactly where the cursor will cross. Computers no longer have
  to line up: a laptop can sit against the lower half of a tall monitor, and the cursor crosses only
  where they actually touch.
- **Real monitor arrangements.** Each computer is drawn as its own monitors, arranged as Windows has them,
  portrait screens included, in its own colour so you can tell computers apart.
- **Copy and paste files.** Copy files or folders in Explorer, move to another computer and paste them there.
- **Drag files across.** Drag files in Explorer to the edge of the screen and keep holding: the cursor
  crosses to the next computer, and releasing drops the files into the folder or desktop under the cursor.

Files are sent through the server, encrypted with the rest of the connection, and are never written
outside the folder they're dropped in. More detail is in the [design notes](../docs/dev/layout-rework.md).

## Status

The new features are **Windows only** for now, and are early: they have been tested on one PC but not yet
across many setups. Expect rough edges, and please report what you find.

- The layout is set up on the server, so it works with any client. Copying and dragging files, and
  showing a client's real monitors, need Deskflow Plus on both computers.
- Deskflow Plus still works with stock Deskflow, Input Leap and Barrier computers for mouse, keyboard and
  clipboard sharing.
- Copied files are limited to 512 MB at a time.

## Download

Get the latest build from [Releases](https://github.com/wesleypayton/deskflow-plus/releases):

- **`...-setup.exe`** installs Deskflow Plus, along with the Microsoft Visual C++ runtime if it's missing.
  It installs as its own app, so it won't replace an official Deskflow install.
- **`...-portable.exe`** unpacks a portable copy into a folder you choose, with its settings kept in that
  folder. The portable copy can't control the login screen or administrator (UAC) prompts.

The files aren't code signed, so Windows SmartScreen will warn the first time: choose **More info**, then
**Run anyway**. Deskflow Plus and Deskflow share their settings and network port, so don't run both at once.

## Building

Build it the same way as Deskflow: see [docs/dev/build.md](../docs/dev/build.md). After building a Release
configuration on Windows, [`deploy/windows/make-release.ps1`](../deploy/windows/make-release.ps1) makes the
setup and portable `.exe` files.

## Credits

Deskflow Plus is built on [Deskflow](https://github.com/deskflow/deskflow) and owes everything else to it
and its contributors. Deskflow began as Synergy, created by Chris Schoeneman in 2001; read its
[history](https://github.com/deskflow/deskflow/wiki/History).

- Deskflow: [github.com/deskflow/deskflow](https://github.com/deskflow/deskflow)
- Deskflow contributors: [contributors](https://github.com/deskflow/deskflow/graphs/contributors)
- Similar projects: [Input Leap](https://github.com/input-leap/input-leap), [Lan Mouse](https://github.com/feschber/lan-mouse), [Synergy](https://symless.com/synergy)

## License

Like Deskflow, this project is licensed under [GPL-2.0](../LICENSE) with an
[OpenSSL exception](../LICENSES/LicenseRef-OpenSSL-Exception.txt).
