/******************************************************************************/
/* RTE  -  CMSIS Run-Time Environment                                          */
/******************************************************************************/
/** @file  RteFsUtils.cpp
  * @brief CMSIS RTE Data model
*/
/******************************************************************************/
/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/******************************************************************************/

#include "RteFsUtils.h"

#include "CrossPlatformUtils.h"
#include "RteUtils.h"
#include "WildCards.h"

#include <chrono>
#include <sstream>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <regex>
#include <thread>

using namespace std;

string RteFsUtils::MakePathCanonical(const string& path)
{
  error_code ec;
  string canonical = fs::weakly_canonical(path, ec).generic_string();
  if (ec) {
    // canonical call failed, e.g. path does not exist
    return path;
  }
  return canonical;
}

bool RteFsUtils::MoveExistingFile(const string& existing, const string& newFile) {
  error_code ec;

  // Check if file exists
  if (!fs::exists(existing, ec)) {
    return false;
  }

  // Create directories in the destination path
  fs::create_directories(fs::path(newFile).parent_path().generic_string(), ec);
  if (ec) {
    return false;
  }

  // Move file
  fs::rename(existing, newFile, ec);
  if (ec) {
    return false;
  }

  // Set destination file permissions
  return SetFileReadOnly(newFile, false);
}

bool RteFsUtils::CopyCheckFile(const string& src, const string& dst, bool backup) {
  // Backup file
  if (backup) {
    if (BackupFile(dst) == RteUtils::ERROR_STRING) {
      return false;
    }
  }

  // Create directories in the destination path
  std::error_code ec;
  fs::create_directories(fs::path(dst).parent_path().generic_string(), ec);
  if (ec) {
    return false;
  }

  // Copy file
  fs::copy_file(src, dst, fs::copy_options::overwrite_existing, ec);
  if (ec) {
    return false;
  }

  // Set destination file permissions
  return SetFileReadOnly(dst, false);
}

bool RteFsUtils::ReadFile(const string& fileName, string& buffer) {
  // Open file
  ifstream file(fileName, std::ios::binary);
  if (!file.is_open()) {
    return false;
  }
  // read content and close
  stringstream fileStream;
  fileStream << file.rdbuf();
  buffer = fileStream.str();
  file.close();

  return true;
}


bool RteFsUtils::CmpFileMem(const string& fileName, const string& buffer) {

  // Open file and read content
  string fileBuffer;
  if(!ReadFile(fileName, fileBuffer)) {
    return false;
  }
  // Compare file and buffer contents
  return buffer == fileBuffer;
}

bool RteFsUtils::ExpandFile(const string& fileName, int nInst, string& buffer) {
  // Open file and read content
  string fileBuffer;
  if(!ReadFile(fileName, fileBuffer)) {
    return false;
  }

  // Expand template by replacing %Instance% occurrences
  string instanceString = std::to_string(nInst);
  size_t position = 0;
  bool expanded = false;
  while ((position = fileBuffer.find("%Instance%", position)) != string::npos) {
    fileBuffer.replace(position, 10, instanceString);
    expanded = true;
  }

  // No expansion
  if (!expanded) {
    return false;
  }

  // Set output buffer
  buffer = fileBuffer;
  return true;
}

/*
 * Backup the file 'fileName'
 */
string RteFsUtils::BackupFile(const string& fileName, bool bDeleteExisting) {
  error_code ec;

  // Nothing to backup
  if (!fs::exists(fileName, ec)) {
    return RteUtils::EMPTY_STRING;
  }

  // Iterate over backup suffixes
  string lastBackup;
  for (int i = 0; i < 512; ++i) {
    ostringstream suffix;
    suffix << "." << setfill('0') << setw(4) << i;
    string backupName = fileName + suffix.str();

    // File with new extension exist
    if (fs::exists(backupName, ec)) {
      lastBackup = backupName;
      continue;
    }

    // Check if previous backup is identical
    if (!lastBackup.empty()) {
      ifstream lastBackupFile(lastBackup);
      if (!lastBackupFile.is_open()) {
        return RteUtils::ERROR_STRING;
      }
      stringstream lastBackupStream;
      lastBackupStream << lastBackupFile.rdbuf();
      string lastBackupBuffer = lastBackupStream.str();
      lastBackupFile.close();
      if (CmpFileMem(fileName, lastBackupBuffer)) {
        backupName = lastBackup;
      }
    }

    // Do the backup if needed
    if (backupName != lastBackup) {
      fs::copy_file(fileName, backupName, fs::copy_options::overwrite_existing, ec);
      if (ec) {
        return RteUtils::ERROR_STRING;
      }
    }

    // Delete file if requested
    if (bDeleteExisting) {
      fs::remove(fileName, ec);
      if (ec) {
        return RteUtils::ERROR_STRING;
      }
    }

    // Return backup file name
    return backupName;
  }

  return RteUtils::ERROR_STRING;
}

bool RteFsUtils::CreateTextFile(const string& file, const string& content) {
  // Create file and directories
  error_code ec;
  fs::create_directories(fs::path(file).parent_path(), ec);
  if (ec) {
    return false;
  }
  ofstream fileStream(file, std::ios::binary);
  if (!fileStream.is_open()) {
    return false;
  }
  fileStream << content;
  fileStream.flush();
  fileStream.close();
  return true;
}

bool RteFsUtils::CopyBufferToFile(const string& fileName, const string& buffer, bool backup) {
  // Compare buffer against file contents
  if (CmpFileMem(fileName, buffer)) {
    return true;
  }

  // Backup
  if (backup) {
    if (BackupFile(fileName) == RteUtils::ERROR_STRING) {
      return false;
    }
  }

  return CreateTextFile(fileName, buffer);
}

bool RteFsUtils::CopyMergeFile(const string& src, const string& dst, int nInstance, bool backup) {
  if (nInstance < 0) {
    nInstance = 0;
  }
  string buffer;
  if (ExpandFile(src, nInstance, buffer)) {
    return CopyBufferToFile(dst, buffer, backup);
  }
  else {
    return CopyCheckFile(src, dst, backup);
  }
}

bool RteFsUtils::SetFileReadOnly(const string& path, bool bReadOnly) {
/*
 * Set file read-only permission
 * std:experimental::filesystem "Some permission bits may be ignored on some systems,
 * and changing some bits may automatically change others"
 */

  error_code ec;
  constexpr fs::perms write_mask = fs::perms::owner_write | fs::perms::group_write | fs::perms::others_write;

  if (bReadOnly) {
    // Remove write bits
    fs::permissions(fs::path(path), write_mask, fs::perm_options::remove | fs::perm_options::nofollow, ec);
  } else {
    // Inherit write bits from parent directory
    fs::perms perms = fs::status(fs::path(path).parent_path(), ec).permissions() & write_mask;
    if (ec) {
      return false;
    }

    // If parent directory is not writable, lets rely on current umask
    if (perms == fs::perms::none) {
      perms = (~CrossPlatformUtils::GetCurrentUmask()) & write_mask;
    }

    // Unable to find proper write permission to add, so let's error out.
    if (perms == fs::perms::none) {
      return false;
    }

    // Add write bits
    fs::permissions(fs::path(path), perms, fs::perm_options::add | fs::perm_options::nofollow, ec);
  }

  if (ec) {
    return false;
  }

  // Permissions updated
  return true;
}

bool RteFsUtils::SetTreeReadOnly(const string& path, bool bReadOnly) {
/*
 * Set file tree read-only permission
 * std:experimental::filesystem "Some permission bits may be ignored on some systems,
 * and changing some bits may automatically change others"
 */
  error_code ec;

  // Path does not exist
  if (!fs::exists(path, ec)) {
    return false;
  }

  // Set permission on directory
  if (!SetFileReadOnly(path, bReadOnly)) {
    return false;
  }

  // Iterate over tree and change permission of files and directories
  for(auto& p: fs::recursive_directory_iterator(path, ec)) {
    if (!SetFileReadOnly(p.path().generic_string(), bReadOnly)) {
      return false;
    }
  }

  // Permissions updated
  return true;
}

bool RteFsUtils::DeleteFileAutoRetry(const string& path, unsigned int retries, unsigned int delay) {
  error_code ec;

  // File does not exist, nothing to do
  if (!fs::exists(path, ec)) {
    return true;
  }

  // It is a directory
  if (fs::is_directory(path, ec)) {
    return false;
  }

  // Set permissions
  SetFileReadOnly(path, false);

  for ( unsigned int r = 0; r < retries ; r++ ) {
    if(fs::remove(path, ec)) {
      return true;
    }

    if (delay > 0) {
      this_thread::sleep_for(std::chrono::milliseconds(delay));
    }
  }
  return false;
}

bool RteFsUtils::RemoveDirectoryAutoRetry(const string& path, int retries, int delay) {
  error_code ec;

  // Path does not exist, nothing to do
  if (!fs::exists(path, ec)) {
    return true;
  }

  // It is not a directory
  if (!fs::is_directory(path, ec)) {
    return false;
  }

  // folder is not empty
  if (!fs::is_empty(path, ec)) {
    return false;
  }

  for (int r = 0; r < retries; r++) {
    if (fs::remove(path, ec)) {
      return true;
    }

    if (delay > 0) {
      this_thread::sleep_for(std::chrono::milliseconds(delay));
    }
  }
  return false;
}

bool RteFsUtils::MoveFileExAutoRetry(const string& existingFileName, const string& newFileName, unsigned int retries, unsigned int delay) {
  // Check if file exists
  if (!Exists(existingFileName)) {
    return false;
  }
  for ( unsigned int r = 0; r < retries ; r++ ) {
    if(MoveExistingFile(existingFileName, newFileName)) {
      return true;
    }
    if (delay > 0) {
      this_thread::sleep_for(std::chrono::milliseconds(delay));
    }
  }
  return false;
}

bool RteFsUtils::CopyFileExAutoRetry(const string& existingFileName, const string& newFileName, unsigned int retries, unsigned int delay) {
  // Check if file exists
  if (!Exists(existingFileName)) {
    return false;
  }
  for (unsigned int r = 0; r < retries; r++) {
    if (CopyCheckFile(existingFileName, newFileName, false)) {
      return true;
    }
    if (delay > 0) {
      this_thread::sleep_for(std::chrono::milliseconds(delay));
    }
  }
  return false;
}


bool RteFsUtils::CopyTree(const string& src, const string& dst) {
  error_code ec;

  // File does not exist
  if (!fs::exists(src, ec)) {
    return false;
  }

  // It is not a directory
  if (!fs::is_directory(src, ec)) {
    return false;
  }

  // Create destination directories
  fs::create_directories(dst, ec);
  if (ec) {
    return false;
  }

  // Directory: copy all files recursively
  fs::copy(src, dst, fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
  if (ec) {
    return false;
  }

  // File tree was copied
  return true;
}

bool RteFsUtils::DeleteTree(const string& path) {
  error_code ec;

  // Path does not exist, nothing to do
  if (!fs::exists(path, ec)) {
    return true;
  }

  // It is not a directory
  if (!fs::is_directory(path, ec)) {
    return false;
  }
  SetFileReadOnly(path, false);
  // Remove files
  for (auto& p : fs::recursive_directory_iterator(path, ec)) {
    if (fs::is_regular_file(p, ec)) {
      SetFileReadOnly(p.path().generic_string(), false);
      fs::remove(p, ec);
      if (ec) {
        return false;
      }
    }
  }

  // Remove empty directories
  fs::remove_all(path, ec);
  if (ec) {
    return false;
  }

  // File tree was deleted
  return true;
}

bool RteFsUtils::RemoveFile(const string& file) {
  // Remove file
  error_code ec;
  if (fs::exists(file, ec) && fs::is_regular_file(file, ec)) {
    SetFileReadOnly(file, false);
    fs::remove(file, ec);
    if (ec) {
      return false;
    }
  }
  return true;
}

bool RteFsUtils::RemoveDir(const string& dir) {
  error_code ec;
  if (fs::exists(dir, ec)) {
    // Remove files
    for (auto& p : fs::recursive_directory_iterator(dir, ec)) {
      SetFileReadOnly(p.path().generic_string(), false);
      if (fs::is_regular_file(p, ec)) {
        fs::remove(p, ec);
      }
      if (ec) {
        return false;
      }
    }
    // Remove empty directories
    SetFileReadOnly(dir, false);
    fs::remove_all(dir, ec);
    if (ec) {
      return false;
    }
  }
  return true;
}

bool RteFsUtils::Exists(const string& path)
{
  error_code ec;
  return fs::exists(path, ec);
}

bool RteFsUtils::IsDirectory(const string& path)
{
  error_code ec;
  return fs::is_directory(path, ec);
}

bool RteFsUtils::IsRegularFile(const string& path)
{
  error_code ec;
  return fs::is_regular_file(path, ec);
}

bool RteFsUtils::IsExecutableFile(const string& path)
{
  if (!IsRegularFile(path)) {
    return false;
  }
  return CrossPlatformUtils::CanExecute(path);
}


bool RteFsUtils::IsRelative(const string& path)
{
  return fs::path(path).is_relative();
}


fs::path RteFsUtils::AbsolutePath(const std::string& path) {
  if (!path.empty()) {
    try {
      return fs::absolute(path);
    }
    catch (fs::filesystem_error const& e) {
      std::cout << "runtime_error: " << e.what() << std::endl;
    }
    catch (...) {
      std::cout << "non-standard exception occurred" << std::endl;
    }
  }
  return fs::path();
}

string RteFsUtils::ParentPath(const string& path) {
  return fs::path(path).parent_path().generic_string();
}

string RteFsUtils::LexicallyNormal(const string& path) {
  string lexicallyNormal = fs::path(path).lexically_normal().generic_string();
  if ((lexicallyNormal.length() > 1) && (lexicallyNormal.back() == '/')) {
    // remove trailing separator for alignment with canonical behaviour
    lexicallyNormal.pop_back();
  }
  return lexicallyNormal;
}

string RteFsUtils::RelativePath(const string& path, const string& base, bool withHeadingDot) {
  if (path.empty() || base.empty()) {
    return "";
  }
  error_code ec;
  string relativePath = fs::relative(path, base, ec).generic_string();
  if (withHeadingDot) {
    if (!relativePath.empty() && relativePath.find("./") != 0 && relativePath.find("../") != 0) {
      relativePath.insert(0, "./");
    }
  }
  return relativePath;
}

string RteFsUtils::GetCurrentFolder(bool withTrailingSlash) {
  error_code ec;
  string f = fs::current_path(ec).generic_string();
  if (withTrailingSlash) {
     f += "/"; // returns path also with forward slash
  }
  return f;
}

void RteFsUtils::SetCurrentFolder(const string& path) {
  error_code ec;
  fs::current_path(path, ec);
}

bool RteFsUtils::MakeSureFilePath(const string &filePath) {
  fs::path path(filePath);
  path.remove_filename();
  return CreateDirectories(path.generic_string());
}

bool RteFsUtils::CreateDirectories(const string &_path) {
  fs::path path(_path);
  /*
  The documentation for std::experimental::create_directories() does not define
  the return value for the function, and the std::filesystem implementation is
  ambiguous. On the Mac, the function (in experimental) always returns false, even
  when the directory exists after the call. Hence the subsequent check for exists().
  Note that create_directories() does nothing if the path already exists.
  */
  error_code ec;
  fs::create_directories(path, ec);
  return fs::exists(path, ec);
}

void RteFsUtils::NormalizePath(string& path, const string& base) {
  path = (fs::path(base) / fs::path(path)).lexically_normal().generic_string();
  if (path.size() > 1 && path.back() == '/')
    path.erase(path.size() - 1);
}

string RteFsUtils::FindFirstFileWithExt(const std::string &folder, const char *extension) {
  if (!extension)
    return "";
  set<string, VersionCmp::Greater> files;
  GetFilesSorted(folder, files);
  for (auto entry : files) {
    if (fs::path(entry).extension() == extension) {
      return entry;
    }
  }
  return "";
}

void RteFsUtils::GetFilesSorted(const string &folder, set<string, VersionCmp::Greater> &files) {
  error_code ec;
  // Path does not exist, nothing to do
  if (folder.empty() || !fs::exists(folder, ec) || !fs::is_directory(folder, ec)) {
    return;
  }
  for (auto entry : fs::directory_iterator(folder, ec)) {
    files.insert(entry.path().filename().generic_string());
  }
}

string RteFsUtils::GetInstalledPackVersion(const string &path, const string &versionRange) {
  set<string, VersionCmp::Greater> files;

  RteFsUtils::GetFilesSorted(path, files);

  if ((versionRange.empty()) && (!files.empty()))
    return *files.begin();

  for (auto version : files) {
    if (VersionCmp::RangeCompare(version, versionRange) == 0)
      return version;
  }

  return RteUtils::EMPTY_STRING;
}


RteFsUtils::PathVec RteFsUtils::FindFiles(const string& path, const string& typeExt) {
  RteFsUtils::PathVec result;
  error_code ec;
  for (auto& item : fs::recursive_directory_iterator(path, ec)) {
    if (!fs::is_regular_file(item.path()) || item.path().extension() != typeExt)
      continue;
    result.push_back(item.path());
  }
  return result;
}

RteFsUtils::PathVec RteFsUtils::GrepFiles(const string& dir, const string& wildCardPattern) {
  RteFsUtils::PathVec result;
  error_code ec;
  for (auto& item : fs::directory_iterator(dir, ec)) {
    auto& path = item.path();
    string name = item.path().filename().generic_string();
    if (fs::is_regular_file(path) &&
      WildCards::Match(wildCardPattern, path.generic_string())) {
      result.push_back(path);
    }
  }
  return result;
}

void RteFsUtils::GrepFileNames(list<std::string>& fileNames, const string& dir, const string& wildCardPattern) {
  error_code ec;
  for (auto& item : fs::directory_iterator(dir, ec)) {
    if (fs::is_regular_file(item.path())) {
      string name = item.path().filename().generic_string();
        if (WildCards::Match(wildCardPattern, name)) {
            NormalizePath(name, dir + '/');
            fileNames.push_back(name);
        }
    }
  }
}


int RteFsUtils::CountFilesInFolder(const string& folder)
{
  error_code ec;
  // Path does not exist, nothing to do
  if (folder.empty() || !fs::exists(folder, ec) || !fs::is_directory(folder, ec)) {
    return 0;
  }
  int count = 0;
  for (const auto & entry : fs::recursive_directory_iterator(folder)) {
    if (fs::is_regular_file(entry, ec)) {
      count++;
    }
  }
  return count;
}

void RteFsUtils::GetMatchingFiles(list<string>& files, const string& extension, const string& path, int depth, bool bAlwaysSearchSubfolders)
{
  error_code ec;
  RteFsUtils::PathVec dirs;
  bool bFound = false; // flag indicating that a file is found in this directory
  fs::path folder = RteFsUtils::AbsolutePath(path);
  if (!fs::exists(folder, ec) || !fs::is_directory(folder, ec))
    return;

  for (auto& entry : fs::directory_iterator(folder, ec)) {
    const fs::path& p = entry.path();
    string filename = p.filename().generic_string();
    if (fs::is_regular_file(p)) {
      auto pos = filename.rfind(extension);
      if (pos != string::npos && pos == (filename.size() - extension.size())) {
        files.push_back(p.generic_string()); // insert full absolute path
        bFound = true;
      }
    } else if (depth > 0 && fs::is_directory(p) && filename.find('.') != 0) { // ignore .web, .download directories
      dirs.push_back(p.generic_string());
    }
  }
  if (depth <= 0 || dirs.empty()) {
    return; // max depth is reached
  }

  if (bFound && !bAlwaysSearchSubfolders) {
    return; // e.g.: for pdsc files do not search subdirectories, because they cannot contain other pdsc files
  }
  depth--;
  for (auto& p : dirs) {
    GetMatchingFiles(files, extension, p.generic_string(), depth, bAlwaysSearchSubfolders);
  }
}

void RteFsUtils::GetPackageDescriptionFiles(list<string>& files, const string& path, int depth)
{
  GetMatchingFiles(files, ".pdsc", path, depth, false);
}


void RteFsUtils::GetPackageFiles(list<string>& files, const string& path, int depth)
{
  GetMatchingFiles(files, ".pack", path, depth, true);
}

string RteFsUtils::CreateExtendedName(const string& path, const string& extPrefix)
{
  string backup;
  error_code ec;
  for (int i = 0; i >= 0; i++) { // normal loop abort is break in loop
    backup = path + string("_") + extPrefix + string("_") + to_string(i);
    if (!fs::exists(backup, ec)) {
      break;
    }
  }
  return backup;
}

bool RteFsUtils::FindFileRegEx(const vector<string>& searchPaths, const string& regEx, string& file) {
  error_code ec;
  for (const auto& searchPath : searchPaths) {
    const auto& files = fs::directory_iterator(searchPath, ec);
    vector<string> findings;
    for (const auto& p : files) {
      const string& path = p.path().generic_string();
      try {
        if (regex_match(path, regex(regEx))) {
          findings.push_back(path);
        }
      } catch (regex_error const&) {
        return false;
      }
    }
    if (findings.size() == 1) {
      file = findings[0];
      return true;
    }
    if (findings.size() > 1) {
      file = searchPath;
      return false;
    }
  }
  return false;
}

string RteFsUtils::GetAbsPathFromLocalUrl(const string& url) {
  string filepath = url;
  // File URI scheme allows the literal 'localhost' to be omitted entirely
  // or may contain an empty hostname
  static const string&& fileScheme = "file:/";
  if (filepath.find(fileScheme) == 0) {
    filepath.erase(0, fileScheme.length());
    static const string&& localhost = "/localhost/";
    if (filepath.find(localhost) == 0) {
      filepath.erase(0, localhost.length());
    } else if (filepath.find("//") == 0) {
      filepath.erase(0, 2);
    }
    const string& host = CrossPlatformUtils::GetHostType();
    if ((host == "linux") || (host == "mac")) {
      filepath = fs::path("/").append(filepath).generic_string();
    }
  }
  return filepath;
}

std::string RteFsUtils::FindFile(const std::string& fileName, const std::string& baseDir,
  const std::vector<std::string>& relSearchOrder)
{
  for(auto& relPath : relSearchOrder) {
    string file = baseDir + relPath + fileName;
    if(RteFsUtils::Exists(file)) {
      return MakePathCanonical(file);
    }
  }
  return RteUtils::EMPTY_STRING;
}

std::string RteFsUtils::FindFileInEtc(const std::string& fileName, const std::string& baseDir)
{
  static const std::vector<std::string>& relSearchOrder = { "./", "../etc/", "../../etc/" };
  return FindFile(fileName, baseDir, relSearchOrder);
}

const string& RteFsUtils::FileCategoryFromExtension(const string& file) {
  static const string& OTHER = "other";
  static const map<string, vector<string>> CATEGORIES = {
    {"sourceC", {".c", ".C"}},
    {"sourceCpp", {".cpp", ".c++", ".C++", ".cxx", ".cc", ".CC"}},
    {"sourceAsm", {".asm", ".s", ".S"}},
    {"header", {".h", ".hpp"}},
    {"library", {".a", ".lib"}},
    {"object", {".o"}},
    {"linkerScript", {".sct", ".scf", ".ld", ".icf", ".src"}},
    {"doc", {".txt", ".md", ".pdf", ".htm", ".html"}},
  };
  fs::path ext((fs::path(file)).extension());
  for (const auto& category : CATEGORIES) {
    if (find(category.second.begin(), category.second.end(), ext) != category.second.end()) {
      return category.first;
    }
  }
  return OTHER;
}

bool RteFsUtils::FindFileWithPattern(const std::string& searchDir,
  const std::string& pattern, std::string& file)
{
  try {
    std::regex fileRegex(pattern);

    // Iterate through the directory (not recursively)
    for (const auto& entry : fs::directory_iterator(searchDir)) {
      // Check only regular files
      if (entry.is_regular_file()) {
        const std::string fileName = entry.path().filename().string();
        if (std::regex_match(fileName, fileRegex)) {
          file = fileName;
          return true; // Return on match found
        }
      }
    }
  }
  catch (const std::exception& e) {
    std::cout << "error: " << e.what() << std::endl;
  }
  return false; // No matching file found
}

// End of RteFsUtils.cpp
