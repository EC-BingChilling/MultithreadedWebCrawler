#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>

#define NUM_THREADS 4


struct FileInfo {
    int lineCount;
    int maxLength;
};

// struct work {
//     // link 
// };

struct FileInfo getFileInfo() {
    FILE *fp;
    int count = 0, maxLineLen = 0; 
    char c; // To store a character read from file
    struct FileInfo info;
 
    // Open the file
    fp = fopen("urls.txt", "r");
 
    // Check if file exists
    if (fp == NULL) {
        printf("Could not open file");
        exit(0);
    }
 
    int lineLen = 0;
    // Extract characters from file and store in character c
    for (c = getc(fp); c != EOF; c = getc(fp)) {
        lineLen = lineLen + 1;
        
        if (c == '\n') // Increment count if this character is newline
        {
            count = count + 1;

            if (lineLen > maxLineLen) {
                maxLineLen = lineLen;
            }

            lineLen = 0;
        }
            
    }

    // Account for last line that ends with EOF
    if (count > 1) {
        count = count + 1;
    }

    // Close the file
    fclose(fp);

    
    info.lineCount = count;
    info.maxLength = maxLineLen;

 
    return info;
}





// void * worker(void *arg) {

// }



int main(void)
{
    struct FileInfo info = getFileInfo();
    int numLinks = info.lineCount;
    int maxLength = info.maxLength;
    int numThreads = 0;

    if (numLinks < numThreads) {
        numThreads = numLinks;
    } else {
        numThreads = NUM_THREADS;
    }
    
    char arr[numLinks][maxLength];

    int work_per_thread = 0;

    if ((numLinks % numThreads) == 0) {
        work_per_thread = numLinks / numThreads;
    } else {
        work_per_thread = trunc(numLinks / numThreads) + 1;
    }

    char partitionedData[work_per_thread][maxLength];

    
    
    printf("The file has %d lines\n", numLinks);
    printf("The max len is: %d\n", maxLength);

    FILE * file = fopen("urls.txt", "r");

    if (file != NULL) {
        int i = 0;
        while (fgets(arr[i], maxLength, file) && i < numLinks) {
            printf("%s", arr[i]);
            i++;
        }

        fclose(file);

    } else {
        fprintf(stderr, "Unable to open file!\n");
    }

    // Partition Links
    // int count = 0;
    while (count < numLinks) {

    }

    
    


    // CURL *curl = curl_easy_init();
    // if(curl) {
    //     CURLcode res;
    //     curl_easy_setopt(curl, CURLOPT_URL, "https://example.com");
    //     res = curl_easy_perform(curl);
    //     curl_easy_cleanup(curl);
    // }

    // 1. Input Parsing: Read URLs from a file

    // 2. Threading
    // a. Use pthreads to assign each URL to a thread for fetching
    // b. Use pthreads to handle multiple web page fetches in parallel

    return 0;
}