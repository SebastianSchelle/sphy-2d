#ifndef SAVE_FILE_DIALOG_HPP
#define SAVE_FILE_DIALOG_HPP

#include <string>

namespace osh
{

/// Native save-file picker where available. Returns false if cancelled or unavailable.
/// For a bare filename (no directory), the initial folder is the last successful
/// save/open directory when set (restored from disk when possible), otherwise the
/// process current directory.
bool pick_save_file_path(std::string& out_path,
                         const char* title,
                         const std::string& default_file_name);

/// Native open-file picker where available. `start_path` may be a file or directory
/// to choose the initial folder; if empty, the last successful dialog directory is
/// used when set (restored from disk when possible), otherwise the process current
/// directory.
bool pick_open_file_path(std::string& out_path,
                         const char* title,
                         const std::string& start_path);

}  // namespace osh

#endif
