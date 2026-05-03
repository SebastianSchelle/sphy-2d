#include "save-file-dialog.hpp"

#include <os-helper.hpp>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>

#if defined(_WIN32) || defined(_WIN64)
#include <vector>
#define NOMINMAX
#include <windows.h>
#include <commdlg.h>
#endif

namespace osh
{
namespace
{

/// Parent directory of the last successful save/open (UTF-8), for dialog defaults.
std::string g_last_dialog_dir;

std::filesystem::path dialog_prefs_file()
{
    namespace fs = std::filesystem;
#if defined(_WIN32) || defined(_WIN64)
    const char* appdata = std::getenv("APPDATA");
    if (appdata && appdata[0] != '\0')
    {
        return fs::path(appdata) / "sphy-2d" / "file-dialog-last-dir.txt";
    }
#elif defined(__APPLE__)
    const char* home = std::getenv("HOME");
    if (home && home[0] != '\0')
    {
        return fs::path(home) / "Library" / "Application Support" / "sphy-2d"
               / "file-dialog-last-dir.txt";
    }
#else
    const char* xdg = std::getenv("XDG_CONFIG_HOME");
    if (xdg && xdg[0] != '\0')
    {
        return fs::path(xdg) / "sphy-2d" / "file-dialog-last-dir.txt";
    }
    const char* home = std::getenv("HOME");
    if (home && home[0] != '\0')
    {
        return fs::path(home) / ".config" / "sphy-2d" / "file-dialog-last-dir.txt";
    }
#endif
    return getExecutableDir() / "file-dialog-last-dir.txt";
}

void load_last_dialog_dir_from_disk_once()
{
    static bool loaded = false;
    if (loaded)
    {
        return;
    }
    loaded = true;

    namespace fs = std::filesystem;
    const fs::path f = dialog_prefs_file();
    std::ifstream in(f);
    if (!in)
    {
        return;
    }
    std::string line;
    if (!std::getline(in, line))
    {
        return;
    }
    while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
    {
        line.pop_back();
    }
    if (line.empty())
    {
        return;
    }
    std::error_code ec;
    const fs::path dir(line);
    if (fs::exists(dir, ec) && !ec && fs::is_directory(dir, ec) && !ec)
    {
        g_last_dialog_dir = dir.string();
    }
}

void persist_last_dialog_dir_to_disk()
{
    if (g_last_dialog_dir.empty())
    {
        return;
    }
    namespace fs = std::filesystem;
    const fs::path f = dialog_prefs_file();
    std::error_code ec;
    fs::create_directories(f.parent_path(), ec);
    std::ofstream out(f, std::ios::trunc | std::ios::binary);
    if (!out)
    {
        return;
    }
    out << g_last_dialog_dir << '\n';
}

void remember_dialog_path(const std::filesystem::path& chosen_file)
{
    namespace fs = std::filesystem;
    std::error_code ec;
    const fs::path p = chosen_file.lexically_normal();
    const fs::path parent = p.parent_path();
    if (parent.empty())
    {
        return;
    }
    if (!fs::exists(parent, ec) || ec)
    {
        return;
    }
    if (!fs::is_directory(parent, ec) || ec)
    {
        return;
    }
    g_last_dialog_dir = parent.string();
    persist_last_dialog_dir_to_disk();
}

#if !defined(_WIN32) && !defined(_WIN64)

std::string sh_single_quote(const std::string& s)
{
    std::string out = "'";
    for (char c : s)
    {
        if (c == '\'')
        {
            out += "'\\''";
        }
        else
        {
            out += c;
        }
    }
    out += '\'';
    return out;
}

bool read_first_line(FILE* p, std::string& line)
{
    line.clear();
    std::array<char, 4096> buf{};
    if (!fgets(buf.data(), (int)buf.size(), p))
    {
        return false;
    }
    line.assign(buf.data());
    while (!line.empty()
           && (line.back() == '\n' || line.back() == '\r'))
    {
        line.pop_back();
    }
    return !line.empty();
}

#endif

#if defined(_WIN32) || defined(_WIN64)

std::wstring utf8_to_wide(const std::string& s)
{
    if (s.empty())
    {
        return {};
    }
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    if (n <= 0)
    {
        return {};
    }
    std::wstring w(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), w.data(), n);
    return w;
}

std::string wide_to_utf8(const wchar_t* w)
{
    if (!w || !w[0])
    {
        return {};
    }
    int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    if (n <= 0)
    {
        return {};
    }
    std::string s(static_cast<size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w, -1, s.data(), n, nullptr, nullptr);
    if (!s.empty() && s.back() == '\0')
    {
        s.pop_back();
    }
    return s;
}

#endif

#if !defined(_WIN32) && !defined(_WIN64) && !defined(__APPLE__)

enum class UnixDialogCmd : uint8_t
{
    Unset,
    Zenity,
    Kdialog,
    None,
};

UnixDialogCmd unix_file_dialog_cmd()
{
    static UnixDialogCmd cmd = UnixDialogCmd::Unset;
    if (cmd != UnixDialogCmd::Unset)
    {
        return cmd;
    }
    if (std::system("command -v zenity >/dev/null 2>&1") == 0)
    {
        cmd = UnixDialogCmd::Zenity;
    }
    else if (std::system("command -v kdialog >/dev/null 2>&1") == 0)
    {
        cmd = UnixDialogCmd::Kdialog;
    }
    else
    {
        cmd = UnixDialogCmd::None;
    }
    return cmd;
}

bool try_unix_fork_save_dialog(std::string& out_path,
                               const char* title,
                               const std::filesystem::path& default_path)
{
    const UnixDialogCmd which = unix_file_dialog_cmd();
    if (which == UnixDialogCmd::None)
    {
        return false;
    }

    const std::string t = (title && title[0]) ? title : "Save file";
    std::string cmd;
    if (which == UnixDialogCmd::Zenity)
    {
        cmd = "zenity --file-selection --save --confirm-overwrite --filename="
              + sh_single_quote(default_path.string()) + " --title="
              + sh_single_quote(t) + " 2>/dev/null";
    }
    else
    {
        cmd = "kdialog --getsavefilename " + sh_single_quote(default_path.string())
              + " 2>/dev/null";
    }

    if (FILE* p = popen(cmd.c_str(), "r"))
    {
        std::string line;
        const bool ok = read_first_line(p, line);
        const int st = pclose(p);
        if (ok && st == 0)
        {
            out_path = std::move(line);
            return true;
        }
    }
    return false;
}

bool try_unix_fork_open_dialog(std::string& out_path,
                               const char* title,
                               const std::filesystem::path& start_dir)
{
    const UnixDialogCmd which = unix_file_dialog_cmd();
    if (which == UnixDialogCmd::None)
    {
        return false;
    }

    const std::string t = (title && title[0]) ? title : "Open file";
    std::string cmd;
    if (which == UnixDialogCmd::Zenity)
    {
        std::string dir = start_dir.string();
        if (!dir.empty() && dir.back() != '/')
        {
            dir += '/';
        }
        cmd = "zenity --file-selection --filename=" + sh_single_quote(dir)
              + " --title=" + sh_single_quote(t) + " 2>/dev/null";
    }
    else
    {
        cmd = "kdialog --getopenfilename " + sh_single_quote(start_dir.string())
              + " 2>/dev/null";
    }

    if (FILE* p = popen(cmd.c_str(), "r"))
    {
        std::string line;
        const bool ok = read_first_line(p, line);
        const int st = pclose(p);
        if (ok && st == 0)
        {
            out_path = std::move(line);
            return true;
        }
    }
    return false;
}

#endif

#if defined(__APPLE__)

std::string applescript_escape(const std::string& s)
{
    std::string o;
    o.reserve(s.size());
    for (char c : s)
    {
        if (c == '\\' || c == '"')
        {
            o += '\\';
        }
        o += c;
    }
    return o;
}

bool try_macos_save_dialog(std::string& out_path,
                           const char* title,
                           const std::string& default_file_name,
                           const std::string& default_dir_posix)
{
    const std::string prompt =
        applescript_escape((title && title[0]) ? title : "Save file as");
    const std::string def = applescript_escape(
        default_file_name.empty() ? std::string("hull.yaml") : default_file_name);

    std::string script = "try\nset f to choose file name with prompt \"";
    script += prompt;
    script += "\" default name \"";
    script += def;
    script += "\"";
    if (!default_dir_posix.empty())
    {
        script += " default location (POSIX file \"";
        script += applescript_escape(default_dir_posix);
        if (!default_dir_posix.empty() && default_dir_posix.back() != '/')
        {
            script += '/';
        }
        script += "\")";
    }
    script += "\nreturn POSIX path of f\non error number -128\nreturn \"\""
              "\nend try\n";

    const std::string cmd =
        "printf %s " + sh_single_quote(script) + " | osascript 2>/dev/null";
    if (FILE* p = popen(cmd.c_str(), "r"))
    {
        std::string line;
        const bool ok = read_first_line(p, line);
        (void)pclose(p);
        if (ok && !line.empty())
        {
            out_path = std::move(line);
            return true;
        }
    }
    return false;
}

bool try_macos_open_dialog(std::string& out_path,
                           const char* title,
                           const std::string& default_dir_posix)
{
    const std::string prompt =
        applescript_escape((title && title[0]) ? title : "Open file");
    std::string script = "try\nset f to choose file with prompt \"";
    script += prompt;
    script += "\"";
    if (!default_dir_posix.empty())
    {
        script += " default location (POSIX file \"";
        script += applescript_escape(default_dir_posix);
        if (default_dir_posix.back() != '/')
        {
            script += '/';
        }
        script += "\")";
    }
    script += "\nreturn POSIX path of f\non error number -128\nreturn \"\""
              "\nend try\n";

    const std::string cmd =
        "printf %s " + sh_single_quote(script) + " | osascript 2>/dev/null";
    if (FILE* p = popen(cmd.c_str(), "r"))
    {
        std::string line;
        const bool ok = read_first_line(p, line);
        (void)pclose(p);
        if (ok && !line.empty())
        {
            out_path = std::move(line);
            return true;
        }
    }
    return false;
}

#endif

}  // namespace

bool pick_save_file_path(std::string& out_path,
                         const char* title,
                         const std::string& default_file_name)
{
    out_path.clear();
    load_last_dialog_dir_from_disk_once();
    namespace fs = std::filesystem;

    const fs::path fname =
        default_file_name.empty() ? fs::path("hull.yaml")
                                  : fs::path(default_file_name);
    fs::path def;
    if (fname.is_absolute())
    {
        def = fname;
    }
    else if (fname.has_parent_path())
    {
        def = fs::current_path() / fname;
    }
    else
    {
        std::error_code ec;
        const fs::path last(g_last_dialog_dir);
        if (!g_last_dialog_dir.empty() && fs::exists(last, ec) && !ec
            && fs::is_directory(last, ec) && !ec)
        {
            def = last / fname;
        }
        else
        {
            def = fs::current_path() / fname;
        }
    }

#if defined(_WIN32) || defined(_WIN64)
    thread_local std::vector<wchar_t> file_buf(32768, L'\0');
    std::fill(file_buf.begin(), file_buf.end(), L'\0');

    const std::wstring wdef = utf8_to_wide(def.filename().string());
    if (!wdef.empty() && wdef.size() + 1 < file_buf.size())
    {
        wmemcpy(file_buf.data(), wdef.c_str(), wdef.size());
    }

    static const wchar_t kFilter[] =
        L"YAML (*.yaml)\0*.yaml\0All files (*.*)\0*.*\0\0";

    std::wstring wtitle =
        utf8_to_wide((title && title[0]) ? title : "Save file as");
    std::wstring winitdir = utf8_to_wide(def.parent_path().string());

    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFilter = kFilter;
    ofn.nFilterIndex = 1;
    ofn.lpstrFile = file_buf.data();
    ofn.nMaxFile = (DWORD)file_buf.size();
    ofn.lpstrTitle = wtitle.c_str();
    ofn.Flags = OFN_EXPLORER | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR
                | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    ofn.lpstrDefExt = L"yaml";
    if (!winitdir.empty())
    {
        ofn.lpstrInitialDir = winitdir.c_str();
    }

    if (!GetSaveFileNameW(&ofn))
    {
        return false;
    }
    out_path = wide_to_utf8(file_buf.data());
    if (!out_path.empty())
    {
        remember_dialog_path(out_path);
    }
    return !out_path.empty();

#elif defined(__APPLE__)
    const bool ok = try_macos_save_dialog(
        out_path, title, def.filename().string(), def.parent_path().string());
    if (ok && !out_path.empty())
    {
        remember_dialog_path(out_path);
    }
    return ok;

#else
    const bool ok = try_unix_fork_save_dialog(out_path, title, def);
    if (ok && !out_path.empty())
    {
        remember_dialog_path(out_path);
    }
    return ok;
#endif
}

bool pick_open_file_path(std::string& out_path,
                         const char* title,
                         const std::string& start_path)
{
    out_path.clear();
    load_last_dialog_dir_from_disk_once();
    namespace fs = std::filesystem;

    fs::path startDir = fs::current_path();
    std::error_code ec;
    if (!start_path.empty())
    {
        const fs::path p = fs::path(start_path);
        if (fs::exists(p) && fs::is_regular_file(p))
        {
            startDir = p.parent_path();
        }
        else if (fs::exists(p) && fs::is_directory(p))
        {
            startDir = p;
        }
        else
        {
            const fs::path parent = p.parent_path();
            if (!parent.empty() && parent != p)
            {
                startDir = parent;
            }
        }
    }
    else if (!g_last_dialog_dir.empty())
    {
        const fs::path last(g_last_dialog_dir);
        if (fs::exists(last, ec) && !ec && fs::is_directory(last, ec) && !ec)
        {
            startDir = last;
        }
    }

#if defined(_WIN32) || defined(_WIN64)
    thread_local std::vector<wchar_t> file_buf(32768, L'\0');
    std::fill(file_buf.begin(), file_buf.end(), L'\0');

    static const wchar_t kFilter[] =
        L"YAML (*.yaml;*.yml)\0*.yaml;*.yml\0All files (*.*)\0*.*\0\0";

    std::wstring wtitle =
        utf8_to_wide((title && title[0]) ? title : "Open file");
    std::wstring winitdir = utf8_to_wide(startDir.string());

    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFilter = kFilter;
    ofn.nFilterIndex = 1;
    ofn.lpstrFile = file_buf.data();
    ofn.nMaxFile = (DWORD)file_buf.size();
    ofn.lpstrTitle = wtitle.c_str();
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR
                | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    if (!winitdir.empty())
    {
        ofn.lpstrInitialDir = winitdir.c_str();
    }

    if (!GetOpenFileNameW(&ofn))
    {
        return false;
    }
    out_path = wide_to_utf8(file_buf.data());
    if (!out_path.empty())
    {
        remember_dialog_path(out_path);
    }
    return !out_path.empty();

#elif defined(__APPLE__)
    std::string dirArg = startDir.string();
    if (!dirArg.empty() && dirArg.back() != '/')
    {
        dirArg += '/';
    }
    const bool ok = try_macos_open_dialog(out_path, title, dirArg);
    if (ok && !out_path.empty())
    {
        remember_dialog_path(out_path);
    }
    return ok;

#else
    const bool ok = try_unix_fork_open_dialog(out_path, title, startDir);
    if (ok && !out_path.empty())
    {
        remember_dialog_path(out_path);
    }
    return ok;
#endif
}

}  // namespace osh
