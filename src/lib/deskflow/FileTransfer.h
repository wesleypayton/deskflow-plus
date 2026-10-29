/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <string>
#include <vector>

//! Copying files between computers
/*!
Files are streamed as a header per file followed by its data in chunks, and the
receiver acknowledges what it has written so the sender never gets more than a
small window ahead. That keeps a large transfer from queueing up in front of
mouse and keyboard messages on the same connection.

Paths on the wire are relative, '/' separated and UTF-8. The receiver treats
them as hostile: anything that could reach outside the destination folder, or
that Windows would quietly rename, is refused.
*/
namespace deskflow::transfer {

//! Bytes of file data per message
inline constexpr size_t kChunkSize = 64 * 1024;

//! Messages the sender may have in flight before it waits for an acknowledgement (about 1 MB)
inline constexpr uint32_t kWindow = 16;

//! The receiver acknowledges after this many messages, and always at the end
inline constexpr uint32_t kAckEvery = 4;

//! Longest relative path accepted from the network
inline constexpr size_t kMaxPathLength = 1024;

//! One file to send
struct FileEntry
{
  std::filesystem::path source; //!< where to read it, on the sender only
  std::string relativePath;     //!< as sent, e.g. "Photos/2026/beach.jpg"
  uint64_t size = 0;
  bool folder = false; //!< an empty folder, sent so it is recreated; folders with files come with them
};

//! Convert between UTF-8 strings and paths without going through the local code page
std::filesystem::path pathFromUtf8(const std::string &text);
std::string pathToUtf8(const std::filesystem::path &path);

//! Expand dropped or copied items into files
/*!
Files are taken as they are and directories are walked, with each file's
relative path starting at the name of the item it came from. Empty folders
are listed too, so they are recreated. Symbolic links and anything that isn't
a regular file or directory are skipped.
*/
std::vector<FileEntry> collectFiles(const std::vector<std::filesystem::path> &items);

//! Turn a relative path from the network into a safe local one, or nothing if it isn't safe
std::optional<std::filesystem::path> safeRelativePath(const std::string &relative);

//! \c wanted if nothing is there yet, otherwise "name (2).ext", "name (3).ext" and so on
std::filesystem::path uniquePath(const std::filesystem::path &wanted);

//! Reads files and produces the messages that carry them
class FileSender
{
public:
  struct Message
  {
    enum class Kind
    {
      Header, //!< a file starts (relativePath and size), or an empty folder (folder set)
      Data,   //!< the next chunk of the current file
      End     //!< every file has been sent
    };

    Kind kind = Kind::End;
    std::string relativePath;
    uint64_t size = 0;
    std::string data;
    bool folder = false;
  };

  explicit FileSender(std::vector<FileEntry> files);

  //! The next message, or nothing if waiting for an acknowledgement or finished
  /*!
  Sets \c error and returns nothing if a file can't be read or has shrunk.
  */
  std::optional<Message> next(std::string &error);

  //! The receiver has handled this many messages in total
  void acknowledge(uint32_t handled);

  //! The end message has been produced
  bool finished() const
  {
    return m_finished;
  }

  uint64_t totalBytes() const;

private:
  std::vector<FileEntry> m_files;
  size_t m_current = 0;
  bool m_headerSent = false;
  uint64_t m_sentOfCurrent = 0;
  std::ifstream m_stream;
  uint32_t m_produced = 0;
  uint32_t m_acknowledged = 0;
  bool m_finished = false;
};

//! Writes incoming files under a destination folder
/*!
Each file is written to "<name>.part" and renamed once complete. The top
level item of every path (a file, or the folder it came in) is given a name
that doesn't clash with anything already there, so nothing is overwritten.
*/
class FileReceiver
{
public:
  FileReceiver(std::filesystem::path destination, uint64_t maxTotalBytes);
  ~FileReceiver();

  FileReceiver(const FileReceiver &) = delete;
  FileReceiver &operator=(const FileReceiver &) = delete;

  bool beginFile(const std::string &relativePath, uint64_t size, std::string &error);

  //! Create an empty folder
  bool makeFolder(const std::string &relativePath, std::string &error);
  bool write(const std::string &data, std::string &error);

  //! The end message arrived; fails if a file is still incomplete
  bool finish(std::string &error);

  //! Stop and remove anything only partly written
  void cancel();

  //! Messages handled so far, for acknowledgements
  uint32_t handled() const
  {
    return m_handled;
  }

  //! Count one more handled message; true when an acknowledgement is due
  bool countMessage();

  //! The files and folders created at the top of the destination
  std::vector<std::filesystem::path> topLevelItems() const;

private:
  bool completeCurrent(std::string &error);

  std::filesystem::path m_destination;
  uint64_t m_maxTotalBytes;
  uint64_t m_totalBytes = 0;
  uint32_t m_handled = 0;

  // the open file and where it ends up
  std::ofstream m_stream;
  std::filesystem::path m_partPath;
  std::filesystem::path m_finalPath;
  uint64_t m_expected = 0;
  uint64_t m_written = 0;
  bool m_open = false;

  // first path component as sent -> the name it was given locally
  std::map<std::filesystem::path, std::filesystem::path> m_topLevel;
  std::vector<std::filesystem::path> m_topLevelOrder;
};

} // namespace deskflow::transfer
