#include <string.h>
#include <stdio.h>
#include <curl/curl.h>


int main(void)
{
    CURL *curl = curl_easy_init();
    if(curl) {
        CURLcode res;
        curl_easy_setopt(curl, CURLOPT_URL, "https://example.com");
        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
    }

    // 1. Input Parsing: Read URLs from a file

    // 2. Threading
    // a. Use pthreads to assign each URL to a thread for fetching
    // b. Use pthreads to handle multiple web page fetches in parallel

    return 0;
}