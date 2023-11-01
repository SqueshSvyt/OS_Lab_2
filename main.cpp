#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <thread>
#include <chrono>
//#include <mutex>
#include <semaphore.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

class ImageProcessor {
public:


    ImageProcessor(std::string inputFileName, std::string outputFileName, int threadPriority, int threadNumber,
                   int max_work_thread)
            : inputFileName_(std::move(inputFileName)), outputFileName_(std::move(outputFileName)),
            threadPriority_(threadPriority), threadNumber_(threadNumber), max_work_thread_(max_work_thread) {

        //init sync construction
        sem_init(&semaphore, 0, max_work_thread_);
    }

    // Function to convert a segment of the image to grayscale
    static void ConvertToGrayscale(std::vector<unsigned char>& pixelData, size_t start, size_t end) {
        sem_wait(&semaphore);
        for (size_t i = start; i < end; i += 3) {
            unsigned char red = pixelData[i];
            unsigned char green = pixelData[i + 1];
            unsigned char blue = pixelData[i + 2];

            auto grayscale = static_cast<unsigned char>((red + green + blue) / 3);

            pixelData[i] = grayscale;
            pixelData[i + 1] = grayscale;
            pixelData[i + 2] = grayscale;
        }
        sem_post(&semaphore);
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
        int width, height, maxColorValue;

        std::string format;

        std::ifstream imageFile = image_info(width, height, maxColorValue, format);


        pixelData_.resize(width * height * 3);

        imageFile.read(reinterpret_cast<char*>(&pixelData_[0]), (long long)pixelData_.size());

        imageFile.close();

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
        std::ofstream outputFile(outputFileName_, std::ios::binary);

        if (!outputFile.is_open()) {
            std::cerr << "Can't open the output image file!" << std::endl;
            return;
        }

        // Write the header to the output file
        outputFile << format << "\n" << width << " " << height << "\n" << maxColorValue << "\n";

        // Write the modified pixel data to the output file
        outputFile.write(reinterpret_cast<const char*>(&pixelData_[0]), (long long)pixelData_.size());

        outputFile.close();
    }

    ~ImageProcessor(){
        sem_destroy(&semaphore);
        pixelData_.clear();
    }

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
};

sem_t ImageProcessor::semaphore;

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
    ImageProcessor imageProcessor("../test_big_picture.ppm", "../output_bw.ppm", priority, numThreads, max_work_thread);

    imageProcessor.ProcessImage();

    return 0;
}
