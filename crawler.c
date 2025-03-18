#include <string.h> // For string manipulation
#include <stdio.h> // i/o
#include <curl/curl.h> // libcurl for http requests
#include <errno.h> // error numbers so we don't type messages manually
#include <string.h> // for error messages

#define ARRAY_LENGTH 100
# define URL_LENGTH 512

extern int errno; // for error messages

// Prototypes
char **parseFile(size_t *links);


char **parseFile(size_t *links)
{
    // Precondition: String array and variable to hold length created in main.
    // Postcondition: Return a parsed urls.txt array and updated length variable.

    // Open the file in read mode
    // FILE* is a file pointer
    // fopen returns a file pointer upon success, NULL if not opened or found
    FILE* file = fopen("urls.txt", "r");

    // Fail fast!
    if (file == NULL)
    {
        //printf("Error message: %s\n", strerror(errno));
        perror("Error message");    // does the same thing as above
        exit(1);
    }

    // Count the number of lines
    char **urlBank = malloc(sizeof(char *) * ARRAY_LENGTH);   // We will realloc if we need more lines and chars
    size_t numOfUrls = 0;
    size_t charsRead = 0;                                   // tracks chars read PER LINE
    char c;                                                 // read in each char

    do 
    {
        c = fgetc(file);

        if (ferror(file))
        {
            perror("Error message");                        // thanks to errno.h, string.h, and extern int errno,
            fclose(file);                                   // we don't have to type our own messages.
            exit(1);
        }

        // End of File reached
        if (c == EOF)
        {
            // In case the last URL doesn't end in a \n
            if (charsRead != 0)
            {
                // Constrain
                urlBank[charsRead] = realloc(urlBank[numOfUrls], charsRead);
                urlBank[numOfUrls][charsRead] = '\0';
                numOfUrls++;
            }

            break;
        }
        

        // If current charsRead=0, it is the beginning of a line and we allocate memory for it
        if (charsRead == 0 )    
            urlBank[numOfUrls] = malloc(URL_LENGTH);

        // Store char in 2D array one at a time 
        urlBank[numOfUrls][charsRead] = c;
        charsRead++;

        if (c == '\n') // end of url
        {
            // Constrain the length of the url to also contain the null terminator
            urlBank[numOfUrls] = realloc(urlBank[numOfUrls], charsRead);
            urlBank[numOfUrls][charsRead-1] = '\0';                                     // -1 so we don't catch \n

            // Prepare for next url
            numOfUrls++;
            charsRead = 0;

            // Check if we need more URL space
            if (numOfUrls % ARRAY_LENGTH == 0)
            {
                size_t biggerSize = numOfUrls + ARRAY_LENGTH;
                urlBank = realloc(urlBank,  sizeof(char *) * biggerSize);
            }
              
                
        }
        else // Not the end of the line, still reading chars
        {
            // Check if we need to increase char limit
            if (charsRead % URL_LENGTH == 0)
            {
                size_t moreChars = charsRead + URL_LENGTH;
                urlBank[numOfUrls] = realloc(urlBank[numOfUrls], moreChars);
            }
        }
        
    }while (1==1);

    // Constrain the number of urls to only the amount that we have
    urlBank = realloc(urlBank, sizeof(char *) * numOfUrls);

    fclose(file);

    // "return" the number of urls by directly changing the value (from main) via reference
    *links = numOfUrls;
    return urlBank;
    
    // Don't forget to deallocate the array in main at the end.
    
}




// main
int main(void)
{
    char **urlArray;
    size_t urlNum;

    CURL *curl = curl_easy_init(); // initialize curl handle
    if(curl) { // check if initialization successful
        CURLcode res; // variable to store result of request
        curl_easy_setopt(curl, CURLOPT_URL, "https://example.com"); // setup url for request
        res = curl_easy_perform(curl); // perform http request and store the result
        curl_easy_cleanup(curl); // cleanup and free curl handle after use
    }

    // 1. Input Parsing: Read URLs from a file
    urlArray = parseFile(&urlNum);  //we pass in the address with & to allow the function to change urlNum

    // For your convenience (Delete this block later once we understand!)
    for (int i = 0; i < urlNum; i++)
    {
        printf("%s\n", urlArray[i]);
    }

    // 2. Threading
    // a. Use pthreads to assign each URL to a thread for fetching
    // b. Use pthreads to handle multiple web page fetches in parallel







    // **********************DEALLOCATION***********************************
    // Free each individual url
    for (int i = 0; i< urlNum; i++)
        free(urlArray[i]);
    
    free(urlArray);

    return 0;
}