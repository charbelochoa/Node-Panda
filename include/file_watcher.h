#pragma once


#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <queue>
#include <atomic>
#include <filesystem>

namespace nodepanda {


enum class FileEvent {
    Modified,   
    Added,      
    Removed,    
    Renamed,    
    Overflow    
};


struct FileChange {
    FileEvent   event;
    std::string filename;     
    std::string newFilename;  
};


class FileWatcher {
public:
    FileWatcher();
    ~FileWatcher();  

    
    void start(const std::filesystem::path& directory);

    
    void stop();

    
    std::vector<FileChange> pollChanges();

    
    bool isRunning() const { return m_running.load(std::memory_order_acquire); }
    const std::filesystem::path& getDirectory() const { return m_directory; }

private:
    void watchThreadFunc();

    void enqueueChange(FileChange change);

    std::filesystem::path m_directory;

    std::thread           m_thread;
    std::atomic<bool>     m_running{false};    
    std::atomic<bool>     m_shouldStop{false}; 

    
    std::mutex            m_queueMutex;
    std::queue<FileChange> m_changedFiles;

    
    void* m_directoryHandle = nullptr; 
};

} 
