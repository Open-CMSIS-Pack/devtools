#ifndef RteFsUtils_H
#define RteFsUtils_H
/******************************************************************************/
/* RTE  -  CMSIS Run-Time Environment                                          */
/******************************************************************************/
/** @file  RteFsUtils.h
  * @brief CMSIS RTE Data Model
*/
/******************************************************************************/
/*
 * Copyright (c) 2020-2024 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/******************************************************************************/

#include "RteUtils.h"

#if defined(_MSC_VER) && (_MSC_VER < 1920)
  #include <experimental/filesystem>
  namespace fs = std::experimental::filesystem;
#else
  #include <filesystem>
  namespace fs = std::filesystem;
#endif

// Utility class for file and directory operations
class RteFsUtils
{
private:
  RteFsUtils() {};

public:
  typedef std::vector<fs::path> PathVec;
  /**
   * @brief make path canonical
   * @param path given path to be canonicalized
   * @return canonicalized path
  */
  static std::string MakePathCanonical(const std::string& path);
  /**
   * @brief back up file given by 'src' with new extension '.<4-digit number>', e.g. '.0001'
   * @param src file to be backed up
   * @param deleteExisting true if file to be backed up exists
   * @return backed up file name
  */
  static std::string BackupFile(const std::string& src, bool deleteExisting = false);
  /**
   * @brief Move file 'existing' into 'newFile'
   * @param existing existing file
   * @param newFile new file
   * @return true if file is successfully moved
  */
  static bool MoveExistingFile(const std::string& existing, const std::string& newFile);
  /**
   * @brief copy file 'src' into 'dst', create destination directory if necessary
   * @param src source file name
   * @param dst destination file name
   * @param backup true if destination file is to be backed up
   * @return true if file is copied
  */
  static bool CopyCheckFile(const std::string& src, const std::string& dst, bool backup);
  /**
   * @brief create file and directories if necessary
   * @param file name of file which is to be created
   * @param content string to be stored in the created file
   * @return true if file is created
  */
  static bool CreateTextFile(const std::string& file, const std::string& content);
  /**
   * @brief copy string to file in binary mode. Previous content of file is destroyed.
   * @param fileName name of file
   * @param fileBuffer string to be saved in file
   * @param backup true if file is to be backed up
   * @return true if buffer is copied to file
  */
  static bool CopyBufferToFile(const std::string& fileName, const std::string& fileBuffer, bool backup);
  /**
   * @brief replace occurrences of "%Instance%" in content of file 'src' with 'nInstance' and copy content to destination file 'dst'
   * @param src source file
   * @param dst destination file
   * @param nInstance number to replace occurrences of "%Instance%"
   * @param backup true to back up destination file
   * @return true if copying file is processed
  */
  static bool CopyMergeFile(const std::string& src, const std::string& dst, int nInstance, bool backup);
  /**
   * @brief compare file content with given string 'buffer'
   * @param fileName name of file to be compared
   * @param buffer contain string to be compared
   * @return true if file content is equal to given string
  */
  static bool CmpFileMem(const std::string& fileName, const std::string& buffer);
  /**
   * @brief replace occurrences of "%Instance%" in file content with 'nInst' and store content in string 'buffer'
   * @param fileName name of file to be processed
   * @param nInst number to replace "%Instance%"
   * @param buffer contain processed file content
   * @return true if file content is processed
  */
  static bool ExpandFile(const std::string& fileName, int nInst, std::string& buffer);
  /**
   * @brief store file content in string 'buffer'
   * @param fileName name of file to be read
   * @param buffer string containing file content
   * @return true if file content can be read
  */
  static bool ReadFile(const std::string& fileName, std::string& buffer);
  /**
   * @brief set file read-only permission for owner, group and others
   * @param path file path
   * @param bReadOnly true to set read-only otherwise all permissions are granted to owner, group and others
   * @return true if file access permissions are set
  */
  static bool SetFileReadOnly(const std::string& path, bool bReadOnly);
  /**
   * @brief set read-only permission for directory including subdirectories
   * @param path directory
   * @param bReadOnly true to set read-only otherwise all permissions are granted to owner, group and others
   * @return true if file access permissions for directory including subdirectories are set
  */
  static bool SetTreeReadOnly(const std::string& path, bool bReadOnly = true);
  /**
   * @brief delete file with retries and delay between retries
   * @param path file to be deleted
   * @param retries number of retries
   * @param delay time lag in milliseconds between retries
   * @return true if file is deleted
  */
  static bool DeleteFileAutoRetry(const std::string& path, unsigned int retries = 5, unsigned int delay = 1);
  /**
   * @brief remove directory with retries and delay between retries
   * @param path directory to be removed
   * @param retries number of retries
   * @param delay time lag in milliseconds between retries
   * @return true if directory is removed
  */
  static bool RemoveDirectoryAutoRetry(const std::string& path, int retries = 5, int delay = 1);
  /**
   * @brief rename file with retries and delay between retries, create destination directory if necessary
   * @param existingFileName name of source file
   * @param newFileName name of destination file
   * @param retries number of retries
   * @param delay time lag in milliseconds between retries
   * @return true if file is renamed
  */
  static bool MoveFileExAutoRetry(const std::string& existingFileName, const std::string& newFileName, unsigned int retries = 5, unsigned int delay = 1);
  /**
   * @brief copy file with retries and delay between retries, create destination directory if necessary
   * @param existingFileName name of source file
   * @param newFileName name of destination file
   * @param retries number of retries
   * @param delay time lag in milliseconds between retries
   * @return true if file is copied
  */
  static bool CopyFileExAutoRetry(const std::string& existingFileName, const std::string& newFileName, unsigned int retries = 5, unsigned int delay = 1);
  /**
   * @brief copy source file/directory include subdirectories to destination one
   * @param src source file/directory
   * @param dst destination file/directory
   * @return true if file or directory is copied
  */
  static bool CopyTree(const std::string& src, const std::string& dst);
  /**
   * @brief delete a directory including subdirectories
   * @param path name of directory
   * @return true if directory is deleted
  */
  static bool DeleteTree(const std::string& path);
  /**
   * @brief remove a file or an empty directory
   * @param file name of file or empty directory
   * @return true if file or empty directory is deleted
  */
  static bool RemoveFile(const std::string& file);
  /**
   * @brief remove files and directory including subdirectories
   * @param dir name of directory
   * @return true if files and directories are removed
  */
  static bool RemoveDir(const std::string& dir);
  /**
   * @brief check if a given file or directory exists
   * @param path name of file or directory to be examined
   * @return true if file or directory exists
  */
  static bool Exists(const std::string& path);
  /**
   * @brief check if given path is a directory
   * @param path given path
   * @return true if given path is a directory
  */
  static bool IsDirectory(const std::string& path);
  /**
   * @brief check if given path is a regular file
   * @param path name of given path
   * @return true if given path is a regular file
  */
  static bool IsRegularFile(const std::string& path);
  /**
   * @brief check if given path is a regular file with execute permissions
   * @param path path to be checked
   * @return true if path is an executable
  */
  static bool IsExecutableFile(const std::string& path);
  /**
   * @brief check if given path is relative or absolute
   * @param path path to be checked
   * @return true if path is relative, false if path is absolute
  */
  static bool IsRelative(const std::string& path);

  /**
   * @brief make path to absolute one
   * @param path path to be processed
   * @return filesystem::path containing the absolute path or an empty one in case error has happened
  */
  static fs::path AbsolutePath(const std::string& path);
  /**
   * @brief get parent path
   * @param path path to be processed
   * @return string containing the parent path
  */
  static std::string ParentPath(const std::string& path);

  /**
   * @brief get lexically normalized path
   * @param path path to be processed
   * @return string containing the lexically normalized path
  */
  static std::string LexicallyNormal(const std::string& path);

  /**
   * @brief determine relative path in respect to base directory
   * @param path path to be processed
   * @param base base directory
   * @param withHeadingDot true if returned value should contain heading dot '.'
   * @return relative path in respect to base directory
  */
  static std::string RelativePath(const std::string& path, const std::string& base, bool withHeadingDot = false);

  /**
   * @brief determine absolute path of the current working directory
   * @param withTrailingSlash true if returned value should contain trailing slash '/'
   * @return absolute path of the current working directory
  */
  static std::string GetCurrentFolder(bool withTrailingSlash = true);

  /**
   * @brief change current working directory to path
   * @param path path to use as current working directory
  */
  static void SetCurrentFolder(const std::string &path);

  /**
   * @brief determine first file owning the given extension in given folder
   * @param folder name of path where first file with given extension is to be found
   * @param extension extension of file to be found
   * @return string containing name of first file without path found with given extension
  */
  static std::string FindFirstFileWithExt(const std::string & folder, const char *extension);
  /**
   * @brief look for all files in given folder, ordered by version comparator (applicable for CMSIS packs)
   * @param folder directory to look for files
   * @param files contain all files in folder ordered by version comparator (applicable for CMSIS packs)
  */
  static void GetFilesSorted(const std::string &folder, std::set<std::string, VersionCmp::Greater> &files);
  /**
   * @brief look for installed pack version which matches the given version range
   * @param path folder where installed pack version is to be searched
   * @param versionRange given version range
   * @return string containing installed pack version
  */
  static std::string GetInstalledPackVersion(const std::string &path, const std::string &versionRange);
  /**
   * @brief ensure that directory including subdirectories exists,  creating them if necessary
   * @param filePath directory to be ensured
   * @return true if directory exists
  */
  static bool MakeSureFilePath(const std::string &filePath);
  /**
   * @brief create directory including subdirectories
   * @param path directory to be created
   * @return true if directory is successfully created
  */
  static bool CreateDirectories(const std::string &path);
  /**
   * @brief normalize path by ensuring that it's absolute and canonical
   * @param path folder to be normalized
   * @param base root folder to form absolute
  */
  static void NormalizePath(std::string& path, const std::string& base = RteUtils::EMPTY_STRING);
  /**
   * @brief scan given directory for regular files with given extension
   * @param path directory to look in
   * @param typeExt file extension to look for
   * @return a vector of filesystem::path representing files with given extension
  */
  static PathVec FindFiles(const std::string& path, const std::string& typeExt);

  /**
   * @brief scan given directory for regular files matching given pattern
   * @param dir directory to look in
   * @param wildCardPattern wild card pattern to match
   * @return a vector of filesystem::path representing files with given extension
  */
  static PathVec GrepFiles(const std::string& dir, const std::string& wildCardPattern);

  /**
  * @brief scan given directory for regular files matching given pattern
  * @param fileNames list of strings to receive filenames
  * @param dir directory to look in
  * @param wildCardPattern wild card pattern to match
  */
  static void GrepFileNames(std::list<std::string>& fileNames, const std::string& dir, const std::string& wildCardPattern);

  /**
   * @brief determine number of regular files found in given folder
   * @param folder directory to look for
   * @return number of regular files in given folder
  */
  static int CountFilesInFolder(const std::string& folder);
  /**
   * @brief scan directory including subdirectories for regular files depending on certain criteria
   * @param files result containing regular files matching extension
   * @param extension file extension, must be supplied with leading dot, e.g. ".pdsc"
   * @param path directory to look for
   * @param depth directory depth to look for
   * @param bAlwaysSearchSubfolders true to consider subdirectories
  */
  static void GetMatchingFiles(std::list<std::string>& files, const std::string& extension, const std::string& path, int depth, bool bAlwaysSearchSubfolders);
  /**
   * @brief scan directory including subdirectories for package description files *.pdsc
   * @param files result containing package description files *.pdsc
   * @param path directory to look for
   * @param depth directory depth to look for
  */
  static void GetPackageDescriptionFiles(std::list<std::string>& files, const std::string& path, int depth);
  /**
   * @brief scan directory including subdirectories for package files *.pack
   * @param files result containing package files *.pack
   * @param path directory to look for
   * @param depth directory depth to look for
  */
  static void GetPackageFiles(std::list<std::string>& files, const std::string& path, int depth);
  /**
   * @brief create a string according to the syntax <path>_<extPrefix>_<index> as file name. The postfix index is increased to ensure that file does not exist
   * @param path path to be included
   * @param extPrefix prefix of extension to be included
   * @return string according to the syntax <path>_<extPrefix>_<index>
  */
  static std::string CreateExtendedName(const std::string& path, const std::string& extPrefix);  // creates a name "<path>_<extPrefix>_<index>

  /**
   * @brief find file using regular expression
   * @param search paths
   * @param regular expression
   * @param path to the file found
   * @return true if file is found successfully, false otherwise
  */
  static bool FindFileRegEx(const std::vector<std::string>& searchPaths, const std::string& regEx, std::string& file);

  /**
   * @brief get absolute file path from local url
   * @param url starting with the pattern 'file://localhost/' or 'file:///' or 'file:/'
   * @return string containing the absolute path
  */
  static std::string GetAbsPathFromLocalUrl(const std::string& url);

  /**
   * @brief finds file in directories relative to the base path
   * @param fileName file name to search for
   * @param baseDir path to look in
   * @param relSearchOrder vector of relative directories to look in
   * @return absolute file name if found, empty string otherwise
  */
  static std::string FindFile(const std::string& fileName, const std::string& baseDir,
    const std::vector<std::string>& relSearchOrder);

  /**
   * @brief finds file in  ./, ../etc/, and ../../etc directories relative to the base
   * @param fileName file name to search for
   * @param baseDir path to look in
   * @return absolute file name if found, empty string otherwise
  */
  static std::string FindFileInEtc(const std::string& fileName, const std::string& baseDir);

   /**
   * @brief get file category according to file extension
   * @param filename with extension
   * @return string category
  */
  static const std::string& FileCategoryFromExtension(const std::string& file);

  /**
   * @brief find file using regular expression (non recursively)
   * @param search path
   * @param regular expression
   * @param path to the file found
   * @return true if file is found successfully, false otherwise
  */
  static bool FindFileWithPattern(const std::string& searchPath, const std::string& pattern, std::string& file);
};

#endif // RteFsUtils_H
