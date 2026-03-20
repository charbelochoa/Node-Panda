

#include "file_watcher.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <algorithm>
#include <unordered_set>
#include <cstdio>

namespace nodepanda {



#ifdef _WIN32


static std::string wideToUtf8(const WCHAR* wide, DWORD byteLength) {
    if (byteLength == 0) return {};

    const int charCount = static_cast<int>(byteLength / sizeof(WCHAR));

    const int sizeNeeded = WideCharToMultiByte(
        CP_UTF8, 0,
        wide, charCount,
        nullptr, 0,
        nullptr, nullptr
    );
    if (sizeNeeded <= 0) return {};

    std::string result(static_cast<size_t>(sizeNeeded), '\0');
    WideCharToMultiByte(
        CP_UTF8, 0,
        wide, charCount,
        result.data(), sizeNeeded,
        nullptr, nullptr
    );
    return result;
}

#endif 


static bool hasMdExtension(const std::string& path) {
    if (path.size() < 3) return false;
    const std::string tail = path.substr(path.size() - 3);
    return (tail[0] == '.' &&
            (tail[1] == 'm' || tail[1] == 'M') &&
            (tail[2] == 'd' || tail[2] == 'D'));
}



FileWatcher::FileWatcher() = default;

FileWatcher::~FileWatcher() {
    stop(); 
}


void FileWatcher::start(const std::filesystem::path& directory) {
    if (m_running.load(std::memory_order_acquire)) {
        stop(); 
    }

    m_directory = directory;
    m_shouldStop.store(false, std::memory_order_release);

    
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        std::queue<FileChange> empty;
        std::swap(m_changedFiles, empty);
    }

    m_running.store(true, std::memory_order_release);
    m_thread = std::thread(&FileWatcher::watchThreadFunc, this);

    fprintf(stderr, "[FileWatcher] Iniciado sobre: %s\n",
            m_directory.string().c_str());
}


void FileWatcher::stop() {
    
    if (!m_running.load(std::memory_order_acquire) && !m_thread.joinable()) return;

    m_shouldStop.store(true, std::memory_order_release);

#ifdef _WIN32
    if (m_directoryHandle && m_directoryHandle != INVALID_HANDLE_VALUE) {
        
        CancelIoEx(static_cast<HANDLE>(m_directoryHandle), nullptr);
    }
#endif

    if (m_thread.joinable()) {
        m_thread.join();
    }

#ifdef _WIN32
    if (m_directoryHandle && m_directoryHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(static_cast<HANDLE>(m_directoryHandle));
        m_directoryHandle = nullptr;
    }
#endif

    m_running.store(false, std::memory_order_release);
    fprintf(stderr, "[FileWatcher] Detenido.\n");
}


void FileWatcher::enqueueChange(FileChange change) {
    std::lock_guard<std::mutex> lock(m_queueMutex);
    m_changedFiles.push(std::move(change));
}


std::vector<FileChange> FileWatcher::pollChanges() {
    std::queue<FileChange> snapshot;
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        std::swap(snapshot, m_changedFiles);
    }

    if (snapshot.empty()) return {};

    std::vector<FileChange> result;
    result.reserve(snapshot.size());

    std::unordered_set<std::string> seenModified;

    while (!snapshot.empty()) {
        FileChange& c = snapshot.front();
        if (c.event == FileEvent::Modified) {
            if (seenModified.insert(c.filename).second) {
                result.push_back(std::move(c));
            }
        } else {
            result.push_back(std::move(c));
        }
        snapshot.pop();
    }

    return result;
}


void FileWatcher::watchThreadFunc() {
#ifdef _WIN32
    m_directoryHandle = CreateFileW(
        m_directory.wstring().c_str(),
        FILE_LIST_DIRECTORY,                          
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 
        nullptr,                                       
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS,                   
        nullptr
    );

    if (m_directoryHandle == INVALID_HANDLE_VALUE) {
        const DWORD err = GetLastError();
        fprintf(stderr, "[FileWatcher] ERROR: No se pudo abrir el directorio (Win32: %lu).\n", err);
        m_running.store(false, std::memory_order_release);
        return;
    }

    
    constexpr DWORD BUFFER_SIZE = 32768;
    alignas(DWORD) BYTE buffer[BUFFER_SIZE];

    
    std::string pendingRenameOld;

    while (!m_shouldStop.load(std::memory_order_acquire)) {
        DWORD bytesReturned = 0;

        const BOOL result = ReadDirectoryChangesW(
            static_cast<HANDLE>(m_directoryHandle),
            buffer,
            BUFFER_SIZE,
            TRUE,          
            
            FILE_NOTIFY_CHANGE_FILE_NAME  |  
            FILE_NOTIFY_CHANGE_LAST_WRITE |  
            FILE_NOTIFY_CHANGE_CREATION,    
            &bytesReturned,
            nullptr,       
            nullptr        
        );

        if (m_shouldStop.load(std::memory_order_acquire)) break;

        if (!result) {
            const DWORD err = GetLastError();
            if (err == ERROR_OPERATION_ABORTED) {
                break;
            }
            fprintf(stderr, "[FileWatcher] ERROR en ReadDirectoryChangesW (Win32: %lu).\n", err);
            break;
        }

        if (bytesReturned == 0) {
            fprintf(stderr, "[FileWatcher] ADVERTENCIA: buffer overflow, rescan necesario.\n");
            enqueueChange({FileEvent::Overflow, "", ""});
            pendingRenameOld.clear(); 
            continue;
        }

        
        const FILE_NOTIFY_INFORMATION* info =
            reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(buffer);

        while (true) {
            std::string filename = wideToUtf8(info->FileName, info->FileNameLength);

            std::replace(filename.begin(), filename.end(), '\\', '/');

            const bool isMd = hasMdExtension(filename);

            switch (info->Action) {

                case FILE_ACTION_MODIFIED:
                    if (isMd) {
                        enqueueChange({FileEvent::Modified, filename, ""});
                    }
                    break;

                case FILE_ACTION_ADDED:
                    if (isMd) {
                        enqueueChange({FileEvent::Added, filename, ""});
                    }
                    break;

                case FILE_ACTION_REMOVED:
                    if (isMd) {
                        enqueueChange({FileEvent::Removed, filename, ""});
                    }
                    break;

                case FILE_ACTION_RENAMED_OLD_NAME:
                    pendingRenameOld = filename;
                    break;

                case FILE_ACTION_RENAMED_NEW_NAME: {
                    
                    const bool oldIsMd = hasMdExtension(pendingRenameOld);
                    if (isMd || oldIsMd) {
                        enqueueChange({FileEvent::Renamed, pendingRenameOld, filename});
                    }
                    pendingRenameOld.clear();
                    break;
                }

                default:
                    break;
            }

            if (info->NextEntryOffset == 0) break;
            info = reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(
                reinterpret_cast<const BYTE*>(info) + info->NextEntryOffset
            );
        }
    }

    m_running.store(false, std::memory_order_release);
    fprintf(stderr, "[FileWatcher] Hilo secundario terminado limpiamente.\n");

#else
    
    fprintf(stderr, "[FileWatcher] ReadDirectoryChangesW no disponible en esta plataforma.\n");
    m_running.store(false, std::memory_order_release);
#endif
}

} 
