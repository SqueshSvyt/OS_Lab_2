#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <thread>

// Function to convert a segment of the image to grayscale
void convertToGrayscale(std::vector<unsigned char>& pixelData, size_t start, size_t end) {
    for (size_t i = start; i < end; i += 3) {
        unsigned char red = pixelData[i];
        unsigned char green = pixelData[i + 1];
        unsigned char blue = pixelData[i + 2];

        unsigned char grayscale = (unsigned char)((red + green + blue) / 3);

        pixelData[i] = grayscale;
        pixelData[i + 1] = grayscale;
        pixelData[i + 2] = grayscale;
    }
}

int main() {
    // Open the binary PPM image file
    std::ifstream imageFile("../robloxscreenshot20230915-195938636.ppm", std::ios::binary);

    if (!imageFile.is_open()) {
        std::cerr << "Can't open the input image file!" << std::endl;
        return 1;
    }

    std::string format;
    int width, height, maxColorValue;

    // Read the header of the binary PPM file
    imageFile >> format >> width >> height >> maxColorValue;

    if (format != "P6") {
        std::cerr << "Bad format" << std::endl;
        return 1;
    }

    std::vector<unsigned char> pixelData(width * height * 3);

    imageFile.read(reinterpret_cast<char*>(&pixelData[0]), pixelData.size());

    // Set the number of threads to use (adjust as needed)
    const size_t numThreads = 2;
    std::vector<std::thread> threads;

    // Divide the image into segments for parallel processing
    const size_t segmentSize = pixelData.size() / numThreads;
    size_t start = 0;

    for (size_t i = 0; i < numThreads - 1; ++i) {
        size_t end = start + segmentSize;
        threads.emplace_back(convertToGrayscale, std::ref(pixelData), start, end);
        start = end;
    }

    // Process the last segment in the main thread
    convertToGrayscale(pixelData, start, pixelData.size());

    // Join the threads
    for (std::thread& thread : threads) {
        thread.join();
    }

    // Open the output image file
    std::ofstream outputFile("../output_bw.ppm", std::ios::binary);

    if (!outputFile.is_open()) {
        std::cerr << "Can't open the output image file!" << std::endl;
        return 1;
    }

    // Write the header to the output file
    outputFile << format << "\n" << width << " " << height << "\n" << maxColorValue << "\n";

    // Write the modified pixel data to the output file
    outputFile.write(reinterpret_cast<const char*>(&pixelData[0]), pixelData.size());

    // Close the output image file
    outputFile.close();
    imageFile.close();

    return 0;
}
