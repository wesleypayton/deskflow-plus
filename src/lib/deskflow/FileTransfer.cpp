/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "deskflow/FileTransfer.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <string_view>

namespace deskflow::transfer {

namespace {

// device names Windows reserves in every folder, with or without an extension
bool isReservedName(const std::string &component)
{
  auto base = component.substr(0, component.find('.'));
  std::ranges::transform(base, base.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });

  static const std::array<std::string_view, 4> plain = {"CON", "PRN", "AUX", "NUL"};
  if (std::ranges::find(plain, base) != plain.end())
    return true;

  return base.size() == 4 && (base.starts_with("COM") || base.starts_with("LPT")) &&
         std::isdigit(static_cast<unsigned char>(base[3]));
}

bool isSafeComponent(const std::string &component)
{
  if (component.empty() || component == "." || component == "..")
    return false;

  // Windows drops trailing dots and spaces, which would let two names land on one file
  if (component.back() == '.' || component.back() == ' ')
    return false;

  for (const auto c : component) {
    const auto byte = static_cast<unsigned char>(c);
    if (byte < 0x20 || std::string_view("<>:\"\\|?*").find(c) != std::string_view::npos)
      return false;
  }

  return !isReservedName(component);
}

} // namespace

std::filesystem::path pathFromUtf8(const std::string &text)
{
  return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t *>(text.data()), text.size()));
}

std::string pathToUtf8(const std::filesystem::path &path)
{
  const auto text = path.generic_u8string();
  return std::string(reinterpret_cast<const char *>(text.data()), text.size());
}

std::vector<FileEntry> collectFiles(const std::vector<std::filesystem::path> &items)
{
  namespace fs = std::filesystem;

  std::vector<FileEntry> files;
  for (const auto &item : items) {
    std::error_code ec;
    const auto status = fs::symlink_status(item, ec);
    if (ec)
      continue;

    if (fs::is_regular_file(status)) {
      files.push_back({item, pathToUtf8(item.filename()), fs::file_size(item, ec)});
      continue;
    }

    if (!fs::is_directory(status))
      continue;

    if (fs::is_empty(item, ec)) {
      files.push_back({item, pathToUtf8(item.filename()), 0, true});
      continue;
    }

    for (auto it = fs::recursive_directory_iterator(item, fs::directory_options::skip_permission_denied, ec);
         !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
      const auto entryStatus = it->symlink_status(ec);
      if (ec)
        continue;

      const auto relative = item.filename() / it->path().lexically_relative(item);
      if (fs::is_regular_file(entryStatus)) {
        files.push_back({it->path(), pathToUtf8(relative), it->file_size(ec)});
      } else if (fs::is_directory(entryStatus) && fs::is_empty(it->path(), ec)) {
        // folders with files in are recreated along with them; empty ones need saying
        files.push_back({it->path(), pathToUtf8(relative), 0, true});
      }
    }
  }
  return files;
}

std::optional<std::filesystem::path> safeRelativePath(const std::string &relative)
{
  if (relative.empty() || relative.size() > kMaxPathLength)
    return std::nullopt;

  std::filesystem::path result;
  size_t start = 0;
  while (start <= relative.size()) {
    const auto end = std::min(relative.find('/', start), relative.size());
    const auto component = relative.substr(start, end - start);
    if (!isSafeComponent(component))
      return std::nullopt;

    result /= pathFromUtf8(component);
    start = end + 1;
  }
  return result;
}

std::filesystem::path uniquePath(const std::filesystem::path &wanted)
{
  std::error_code ec;
  if (!std::filesystem::exists(wanted, ec))
    return wanted;

  const auto parent = wanted.parent_path();
  const auto stem = wanted.stem().native();
  const auto extension = wanted.extension().native();
  for (int n = 2;; ++n) {
    auto candidate = stem;
    candidate += std::filesystem::path(" (" + std::to_string(n) + ")").native();
    candidate += extension;
    const auto path = parent / candidate;
    if (!std::filesystem::exists(path, ec))
      return path;
  }
}

//
// FileSender
//

FileSender::FileSender(std::vector<FileEntry> files) : m_files(std::move(files))
{
  // nothing to do
}

std::optional<FileSender::Message> FileSender::next(std::string &error)
{
  if (m_finished || m_produced - m_acknowledged >= kWindow)
    return std::nullopt;

  using enum Message::Kind;
  while (true) {
    if (m_current >= m_files.size()) {
      m_finished = true;
      ++m_produced;
      return Message{End, {}, 0, {}};
    }

    const auto &file = m_files[m_current];
    if (file.folder) {
      ++m_current;
      ++m_produced;
      return Message{Header, file.relativePath, 0, {}, true};
    }

    if (!m_headerSent) {
      m_stream = std::ifstream(file.source, std::ios::binary);
      if (!m_stream) {
        error = "can't read " + pathToUtf8(file.source);
        return std::nullopt;
      }
      m_headerSent = true;
      m_sentOfCurrent = 0;
      ++m_produced;
      return Message{Header, file.relativePath, file.size, {}};
    }

    if (m_sentOfCurrent >= file.size) {
      m_stream.close();
      m_headerSent = false;
      ++m_current;
      continue;
    }

    const auto length = static_cast<size_t>(std::min<uint64_t>(kChunkSize, file.size - m_sentOfCurrent));
    std::string data(length, '\0');
    m_stream.read(data.data(), static_cast<std::streamsize>(length));
    if (static_cast<size_t>(m_stream.gcount()) != length) {
      error = pathToUtf8(file.source) + " changed while it was being sent";
      return std::nullopt;
    }

    m_sentOfCurrent += length;
    ++m_produced;
    return Message{Data, {}, 0, std::move(data)};
  }
}

void FileSender::acknowledge(uint32_t handled)
{
  m_acknowledged = std::max(m_acknowledged, std::min(handled, m_produced));
}

uint64_t FileSender::totalBytes() const
{
  uint64_t total = 0;
  for (const auto &file : m_files)
    total += file.size;
  return total;
}

//
// FileReceiver
//

FileReceiver::FileReceiver(std::filesystem::path destination, uint64_t maxTotalBytes)
    : m_destination(std::move(destination)),
      m_maxTotalBytes(maxTotalBytes)
{
  // nothing to do
}

FileReceiver::~FileReceiver()
{
  cancel();
}

bool FileReceiver::beginFile(const std::string &relativePath, uint64_t size, std::string &error)
{
  namespace fs = std::filesystem;

  if (m_open && !completeCurrent(error))
    return false;

  const auto relative = safeRelativePath(relativePath);
  if (!relative) {
    error = "refused unsafe file name \"" + relativePath + "\"";
    return false;
  }

  if (size > m_maxTotalBytes || m_totalBytes > m_maxTotalBytes - size) {
    error = "the files are larger than the transfer limit";
    return false;
  }
  m_totalBytes += size;

  // a file on its own gets a fresh name; files in a folder share the folder's name
  const auto first = *relative->begin();
  fs::path local;
  if (std::distance(relative->begin(), relative->end()) == 1) {
    local = uniquePath(m_destination / first);
  } else {
    auto folder = m_topLevel.find(first);
    if (folder == m_topLevel.end()) {
      const auto name = uniquePath(m_destination / first).filename();
      folder = m_topLevel.emplace(first, name).first;
      m_topLevelOrder.push_back(m_destination / name);
    }
    local = m_destination / folder->second / relative->lexically_relative(first);
  }

  std::error_code ec;
  fs::create_directories(local.parent_path(), ec);
  if (ec) {
    error = "can't create " + pathToUtf8(local.parent_path()) + ": " + ec.message();
    return false;
  }

  m_finalPath = uniquePath(local);
  m_partPath = uniquePath(fs::path(m_finalPath).concat(".part"));
  m_stream = std::ofstream(m_partPath, std::ios::binary | std::ios::trunc);
  if (!m_stream) {
    error = "can't write " + pathToUtf8(m_partPath);
    return false;
  }

  if (std::distance(relative->begin(), relative->end()) == 1)
    m_topLevelOrder.push_back(m_finalPath);

  m_expected = size;
  m_written = 0;
  m_open = true;
  return size > 0 || completeCurrent(error);
}

bool FileReceiver::makeFolder(const std::string &relativePath, std::string &error)
{
  namespace fs = std::filesystem;

  if (m_open && !completeCurrent(error))
    return false;

  const auto relative = safeRelativePath(relativePath);
  if (!relative) {
    error = "refused unsafe folder name \"" + relativePath + "\"";
    return false;
  }

  // share the top level folder's name with any files that came in it
  const auto first = *relative->begin();
  auto folder = m_topLevel.find(first);
  if (folder == m_topLevel.end()) {
    const auto name = uniquePath(m_destination / first).filename();
    folder = m_topLevel.emplace(first, name).first;
    m_topLevelOrder.push_back(m_destination / name);
  }

  std::error_code ec;
  const auto local = (m_destination / folder->second / relative->lexically_relative(first)).lexically_normal();
  fs::create_directories(local, ec);
  if (ec) {
    error = "can't create " + pathToUtf8(local) + ": " + ec.message();
    return false;
  }
  return true;
}

bool FileReceiver::write(const std::string &data, std::string &error)
{
  if (!m_open) {
    error = "file data arrived before its header";
    return false;
  }
  if (data.size() > m_expected - m_written) {
    error = "more data arrived than the file's size";
    return false;
  }

  m_stream.write(data.data(), static_cast<std::streamsize>(data.size()));
  if (!m_stream) {
    error = "can't write " + pathToUtf8(m_partPath) + ", is the disk full?";
    return false;
  }

  m_written += data.size();
  return m_written < m_expected || completeCurrent(error);
}

bool FileReceiver::finish(std::string &error)
{
  return completeCurrent(error);
}

void FileReceiver::cancel()
{
  if (!m_open)
    return;

  m_stream.close();
  std::error_code ec;
  std::filesystem::remove(m_partPath, ec);
  m_open = false;
}

bool FileReceiver::countMessage()
{
  ++m_handled;
  return m_handled % kAckEvery == 0;
}

std::vector<std::filesystem::path> FileReceiver::topLevelItems() const
{
  return m_topLevelOrder;
}

bool FileReceiver::completeCurrent(std::string &error)
{
  if (!m_open)
    return true;

  if (m_written != m_expected) {
    error = "the connection ended part way through " + pathToUtf8(m_finalPath.filename());
    return false;
  }

  m_stream.close();
  m_open = false;

  // something may have taken the name since the file was opened
  const auto target = uniquePath(m_finalPath);
  std::error_code ec;
  std::filesystem::rename(m_partPath, target, ec);
  if (ec) {
    error = "can't finish " + pathToUtf8(target) + ": " + ec.message();
    return false;
  }

  if (target != m_finalPath)
    std::ranges::replace(m_topLevelOrder, m_finalPath, target);
  return true;
}

} // namespace deskflow::transfer
