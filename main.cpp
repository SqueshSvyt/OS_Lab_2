#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#include <mutex>
#include <semaphore.h>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#include <cstring>

#else
#include <pthread>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#endif

class ImageProcessor {
public:

    ImageProcessor(std::string inputFileName, std::string outputFileName, int threadPriority, int threadNumber,
                   int max_work_thread)
            : inputFileName_(std::move(inputFileName)), outputFileName_(std::move(outputFileName)),
            threadPriority_(threadPriority), threadNumber_(threadNumber), max_work_thread_(max_work_thread) {

        //init sync construction
#ifdef _WIN32
        semaphore = CreateSemaphore(NULL, max_work_thread, max_work_thread, NULL);
        mutex_win = CreateMutex(NULL, FALSE, NULL);
#else
        sem_init(&semaphore, 0, max_work_thread_);
#endif
    }

    // Function to convert a segment of the image to grayscale
    static void ConvertToGrayscale(std::vector<unsigned char>& pixelData, size_t start, size_t end) {
        auto start_time = std::chrono::steady_clock::now();


#ifdef _WIN32
        WaitForSingleObject(semaphore, INFINITE);
#else
        sem_init(&semaphore, 0, max_work_thread_);
#endif
        for (size_t i = start; i < end; i += 3) {
            //std::unique_lock<std::mutex> lock(mtx);

            unsigned char red = pixelData[i];
            unsigned char green = pixelData[i + 1];
            unsigned char blue = pixelData[i + 2];

            auto grayscale = static_cast<unsigned char>((red + green + blue) / 3);

            std::this_thread::sleep_for(std::chrono::milliseconds(10));

            //mtx.lock();
            pixelData[i] = grayscale;
            pixelData[i + 1] = grayscale;
            pixelData[i + 2] = grayscale;
            //mtx.unlock();

            auto current_time = std::chrono::steady_clock::now();
            auto elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - start_time).count();
            if (elapsed_time >= 20 || i > end - 4) {

#ifdef _WIN32
                WaitForSingleObject(mutex_win, INFINITE);
#else
                mtx.lock();
#endif
                std::cout << "Progress of thread " << std::this_thread::get_id() << ": " << i - start << " out of " << end - start << " processed." << std::endl;
#ifdef _WIN32
                ReleaseMutex(mutex_win);
#else
                mtx.lock();
#endif

                start_time = current_time;
            }
        }
#ifdef _WIN32
        ReleaseSemaphore(semaphore, 1, NULL);
#else
        sem_post(&semaphore);
#endif
    }

    // Set thread priority (platform-specific)
    void SetThreadPriority(std::thread& thread) const {

#ifdef _WIN32

        int priority = THREAD_PRIORITY_NORMAL;
        if (threadPriority_ == 1) {
            priority = HIGH_PRIORITY_CLASS ;
        }
        else if (threadPriority_ == -1) {
            priority = THREAD_PRIORITY_LOWEST;
        }

        ::SetThreadPriority(reinterpret_cast<HANDLE>(thread.native_handle()), priority);

#else
        int policy = SCHED_OTHER;
        struct sched_param params;
        params.sched_priority = 0;

        if (threadPriority_ == 1) {
            policy = SCHED_FIFO;
        }
        else if (threadPriority_ == -1) {
            policy = SCHED_BATCH;
        }

        pthread_setschedparam(thread.native_handle(), policy, &params);
#endif

    }

    void ProcessImage() {
        int width=0 , height=0, maxColorValue=0;

        std::string format;

        //std::ifstream imageFile = image_info(width, height, maxColorValue, format);
#ifdef _WIN32
        HANDLE file = CreateFile(inputFileName_.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

        LARGE_INTEGER fileSize;
        GetFileSizeEx(file, &fileSize);

        HANDLE file_mapping = CreateFileMapping(file, nullptr, PAGE_READONLY, 0, fileSize.QuadPart, nullptr);
        void* mapped_data = MapViewOfFile(file_mapping, FILE_MAP_READ, 0, 0, 0);

        const char* charData = static_cast<const char*>(mapped_data);
        std::vector<unsigned char> byteVector(charData, charData + strlen(charData));

        pixelData_ = byteVector;

        UnmapViewOfFile(mapped_data);
        CloseHandle(file_mapping);
        CloseHandle(file);
#else
        // Use memory mapping on Linux
        int fd = open(inputFileName_.c_str(), O_RDONLY);
        void* mapped_data = mmap(nullptr, pixelData_.size(), PROT_READ, MAP_SHARED, fd, 0);

        const char* charData = static_cast<const char*>(mapped_data);
        std::vector<unsigned char> byteVector(charData, charData + strlen(charData));

        pixelData_ = byteVector;

        munmap(mapped_data, pixelData_.size());
        close(fd);
#endif

        //imageFile.close();
        std::reverse(pixelData_.begin(), pixelData_.end());

        while (pixelData_[pixelData_.size() - 1] != '\n') {
            format += pixelData_[pixelData_.size() - 1];
            pixelData_.pop_back();
        }
        pixelData_.pop_back();
        while (pixelData_[pixelData_.size() - 1] != ' ') {
            width = width * 10 + (pixelData_[pixelData_.size() - 1] - '0');
            pixelData_.pop_back();
        }
        pixelData_.pop_back();
        while (pixelData_[pixelData_.size() - 1] != '\n') {
            height = height * 10 + (pixelData_[pixelData_.size() - 1] - '0');
            pixelData_.pop_back();
        }
        pixelData_.pop_back();
        while (pixelData_[pixelData_.size() - 1] != '\n') {
            maxColorValue = maxColorValue * 10 + (pixelData_[pixelData_.size() - 1] - '0');
            pixelData_.pop_back();
        }
        pixelData_.pop_back();

        const size_t numThreads = threadNumber_;
        std::vector<std::thread> threads;

        // Divide the image into segments for parallel processing
        const size_t segmentSize = pixelData_.size() / numThreads;
        size_t start = 0;
        //size_t end = pixelData_.size();

        auto start_time = std::chrono::high_resolution_clock::now();


        for (size_t i = 0; i < numThreads - 1; ++i) {
            size_t end = start + segmentSize;


            threads.emplace_back(ConvertToGrayscale, std::ref(pixelData_), start, end);

            // Set thread priority
            SetThreadPriority(threads.back());

            //Replace work zone for thread
            start = end;
        }

        // Process the last segment in the main thread
        ConvertToGrayscale(pixelData_, start, pixelData_.size());
        // Join the threads
        for (std::thread& thread : threads) {
            thread.join();
        }

        auto stop_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(stop_time - start_time);

        //Work time
        std::cout << "Execution time: " << duration.count() << " microseconds" << std::endl;

        // Write the processed image to the output file
#ifdef _WIN32
        HANDLE file_out = CreateFile(outputFileName_.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        HANDLE file_mapping_out = CreateFileMapping(file_out, nullptr, PAGE_READWRITE, 0, fileSize.QuadPart, nullptr);
        LPVOID mapped_data_out = MapViewOfFile(file_mapping_out, FILE_MAP_WRITE, 0, 0, fileSize.QuadPart);

        char* charDataOut = static_cast<char*>(mapped_data_out);
#else
        // Use memory mapping on Linux for writing the output file
        int fd_out = open(outputFileName_.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);

        ftruncate(fd_out, pixelData_.size());

        void* mapped_data_out = mmap(nullptr, pixelData_.size(), PROT_READ | PROT_WRITE, MAP_SHARED, fd_out, 0);


        char* charDataOut = static_cast<char*>(mapped_data_out);
#endif


        const char* temp = reinterpret_cast<const char *>(pixelData_.data());
        std::string res = format + '\n' + std::to_string(width) + ' ' +
                std::to_string(height) + '\n' + std::to_string(maxColorValue) +
                '\n' + temp;
        // Write the processed image data to the output file
        std::copy(res.data(), res.data() + res.size(), charDataOut);


#ifdef _WIN32
        UnmapViewOfFile(mapped_data_out);
        CloseHandle(file_mapping_out);
        CloseHandle(file_out);
#else
        // Unmap the memory and close the file descriptor on Linux
        munmap(mapped_data_out, pixelData_.size());
        close(fd_out);
#endif
    }

    ~ImageProcessor(){
#ifdef _WIN32
        CloseHandle(mutex_win);
        CloseHandle(semaphore_win);
#else
        sem_destroy(&semaphore);
#endif
        pixelData_.clear();
    }

    static std::mutex mtx;
    static sem_t semaphore;

private:

    std::ifstream image_info(int& width,int& height, int& maxColorValue, std::string& format){
        std::ifstream imageFile(inputFileName_, std::ios::binary);

        if (!imageFile.is_open()) {
            std::cerr << "Can't open the input image file!" << std::endl;
        }

        // Read the header of the binary PPM file
        imageFile >> format >> width >> height >> maxColorValue;

        if (format != "P6") {
            std::cerr << "Bad format" << std::endl;
        }

        return imageFile;
    }

    std::string inputFileName_;
    std::string outputFileName_;
    int threadPriority_;
    int threadNumber_;
    int max_work_thread_;
    std::vector<unsigned char> pixelData_;

#ifdef _WIN32
    static HANDLE semaphore_win;
    static HANDLE mutex_win;
#endif
};



sem_t ImageProcessor::semaphore;
std::mutex ImageProcessor::mtx;

#ifdef _WIN32
    HANDLE ImageProcessor::semaphore_win;
    HANDLE ImageProcessor::mutex_win;
#endif

int main() {

    int numThreads, priority, max_work_thread;
    std::cout << "Enter num of threads: ";
    std::cin >> numThreads;
    std::cout << std::endl;

    std::cout << "Enter work thread: ";
    std::cin >> max_work_thread;
    std::cout << std::endl;

    std::cout << "Enter priority level: ";
    std::cin >> priority;
    std::cout << std::endl;

    //Set threadPriority to 1 for higher priority, -1 for lower priority, 0 for normal priority.
    ImageProcessor imageProcessor("../Gojo.ppm", "../output_bw.ppm", priority, numThreads, max_work_thread);

    imageProcessor.ProcessImage();

    return 0;
}
