#include <string.h> // For string manipulation
#include <stdio.h> // i/o
#include <curl/curl.h> // libcurl for http requests

// main
int main(void)
{
    CURL *curl = curl_easy_init(); // initialize curl handle
    if(curl) { // check if initialization successful
        CURLcode res; // variable to store result of request
        curl_easy_setopt(curl, CURLOPT_URL, "https://example.com"); // setup url for request
        res = curl_easy_perform(curl); // perform http request and store the result
        curl_easy_cleanup(curl); // cleanup and free curl handle after use
    }

    // 1. Input Parsing: Read URLs from a file

    // 2. Threading
    // a. Use pthreads to assign each URL to a thread for fetching
    // b. Use pthreads to handle multiple web page fetches in parallel

    return 0;
}