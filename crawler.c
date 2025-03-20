#include <string.h> // For string manipulation
#include <stdio.h> // i/o
#include <curl/curl.h> // libcurl for http requests
#include <stdlib.h> // exit() and memory functions

void fetch_url(const char *url){ // function to fetch urls by taking url as input and fetch using libcurl
    CURL *curl = curl_easy_init(); // initialize curl session (curl object)
    if (curl){
        printf("Fetching %s\n", url); // print url being fetched
        curl_easy_setopt(curl, CURLOPT_URL, url); // set url for the request
        CURLcode res = curl_easy_perform(curl); //perform http request

        if(res != CURLE_OK){ // check if request failed and prints error message
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        
        curl_easy_cleanup(curl); // cleanup curl session
        }
    }
}

// main
int main(void)
{
    FILE *file = fopen("urls.txt","r"); // open urls.txt in read mode
    if (file == NULL){ // check if fopen failed
        fprintf(stderr, "Error: Could not open urls.txt \n"); // print error message to standard ouput
        return 1; // exit with error status
    }

    char url[1024]; // buffer to store each line
    while (fgets(url, sizeof(url),file)){ // read each line
        url[strcspn(url, "\n")] = 0; // remove newline character
        fetch_url(url); // call fetch url() to get the webpage
    }
    fclose(file); // close file
    return 0; // exit successfully

    // 1. Input Parsing: Read URLs from a file

    // 2. Threading
    // a. Use pthreads to assign each URL to a thread for fetching
    // b. Use pthreads to handle multiple web page fetches in parallel

    return 0;
}