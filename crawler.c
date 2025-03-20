#include <string.h> // For string manipulation
#include <stdio.h> // i/o
#include <curl/curl.h> // libcurl for http requests
#include <errno.h> // error numbers so we don't type messages manually
#include <stdlib.h> // Add mem alloc
#include <string.h> // for error messages
#include <math.h>
#include <stdbool.h>
#include <pthread.h> 

#define ARRAY_LENGTH 100
# define URL_LENGTH 512
#define NUM_THREADS 4


extern int errno; // for error messages

pthread_mutex_t lock; 


// Prototypes
char **parseFile(size_t *links);

// Define a structure for Job
struct Job {
    char *link;                      // Link to make request
    char *contentFilename;          // Filename to store saved content
    int contentFileNameLength;  // Length of the content filename string
    int linkLength;              // Length of the link string
};

struct JobQueueInfo {
    bool hasJob;
    struct Job job;
};

const int IMPORTANT_WORDS_SIZE = 1;
const char * IMPORTANT_WORDS[] = {"Word"};





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



static size_t write_data(void *ptr, size_t size, size_t nmemb, void *stream)
{
    size_t written = fwrite(ptr, size, nmemb, (FILE *)stream);
    return written;
}

void fetchUrlWithFile(char *url, const char * fileName) {
    printf("\n%s", url);

    // printf("Url %s", url);
    CURL *curl_handle;

    FILE * pagefile;

    /* init the curl session */
    curl_handle = curl_easy_init();
    if (!curl_handle) {
        fprintf(stderr, "Failed to initialize CURL\n");
        return;
    }
    
    /* set URL to get here */
    curl_easy_setopt(curl_handle, CURLOPT_URL, url);
    
    /* Switch on full protocol/debug output while testing */
    // curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, 1L);
    
    /* disable progress meter, set to 0L to enable it */
    // curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 1L);
    
    /* send all data to this function  */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_data);
    
    /* open the file */
    pagefile = fopen(fileName, "wb");
    if(pagefile) {
    
        /* Curl setup to write page body to this file handle */
        curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, pagefile);
    
        /* Get page and store in file */
        CURLcode res = curl_easy_perform(curl_handle);

        if (res != CURLE_OK) {
            fprintf(stderr, "Failed to fetch URL: %s\n", curl_easy_strerror(res));
        } else {
            printf("%s content successfully written to %s\n", url, fileName);  // Debug output
        }
    
        /* close the file */
        fclose(pagefile);
    }
    
    /* cleanup curl stuff */
    curl_easy_cleanup(curl_handle);


}



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


int getWorkPerThread(int numLinks, int numThreads) {
    int workPerThread = 0;

    if ((numLinks % numThreads) == 0) {
        workPerThread = numLinks / numThreads;
    } else {
        workPerThread = trunc(numLinks / numThreads) + 1;
    }

    return workPerThread;
}


void getJobs(struct Job jobs[], int numUrl, char **urlArray) {
    
    for (int i = 0; i < numUrl; i++) {
        // Proper initialization of each Job element
        int fileNameLen = 50;

        char *filename = (char *)calloc(fileNameLen, sizeof(char)); // Allocate memory for filename


        // Create the filename as "page<count>.html"
        sprintf(filename, "page%d.html", i + 1);

        // Print the filename
        jobs[i].contentFilename = (char *)calloc(fileNameLen, sizeof(char));
        
        jobs[i] = (struct Job){
            .link = urlArray[i],                      // Assign URL from the array
            .contentFilename = filename,                   
            .contentFileNameLength = strlen(filename),          
            .linkLength = strlen(urlArray[i]),        // Set to length of the URL string
        };
    
        jobs[i].link[strcspn(jobs[i].link, "\n")] = 0;
        jobs[i].link[strcspn(jobs[i].link, "\r")] = 0;


        // Print each URL from urlArray
        // printf("%s\n", urlArray[i]);
    }
    
}


struct JobQueueInfo getJobWithFifo(const struct Job jobs[], int numJobs) {
    pthread_mutex_lock(&lock); 

    static int fifoIndex = 0;
    struct JobQueueInfo info;

    if (fifoIndex < numJobs) {
        info.hasJob = true;
        info.job = jobs[fifoIndex];
        fifoIndex++;
    } else {
        info.hasJob = false;
    }

    pthread_mutex_unlock(&lock); 

    return info;
}


void worker(const struct Job jobs[], int numJobs) {
    for (int count = 0; count < numJobs; count++) {
        struct JobQueueInfo temp = getJobWithFifo(jobs, numJobs);

        if (temp.hasJob) {
            struct Job job = temp.job;

            fetchUrlWithFile(job.link, job.contentFilename);


        } else {
            printf("No job available");
        }
        
    }
}


// main
int main(void)
{
    char **urlArray;
    size_t urlNum;


    curl_global_init(CURL_GLOBAL_ALL);

    // 1. Input Parsing: Read URLs from a file
    urlArray = parseFile(&urlNum);  //we pass in the address with & to allow the function to change urlNum

    // For your convenience (Delete this block later once we understand!)
    struct Job jobs[urlNum]; // Array of 'urlNum' Job structs

    getJobs(jobs, urlNum, urlArray);

    worker(jobs, urlNum);


    for (int i = 0; i < urlNum; i++) {
        printf("Job %d:\n", i + 1);
        printf("  Link: %s\n", jobs[i].link);
        printf("  Link Length: %d\n", jobs[i].linkLength);
        printf("  Content Filename: %s\n", jobs[i].contentFilename);
        printf("  Content Filename Length: %d\n", jobs[i].contentFileNameLength);

        printf("\n");
    }
    

    // 2. Threading
    // a. Use pthreads to assign each URL to a thread for fetching
    // b. Use pthreads to handle multiple web page fetches in parallel







    // **********************DEALLOCATION***********************************
    // Free each individual url
    for (int i = 0; i< urlNum; i++)
        free(urlArray[i]);
    
    free(urlArray);

    curl_global_cleanup();



    return 0;
}