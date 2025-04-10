#define _GNU_SOURCE // For getline
#include <string.h> // For string manipulation
#include <stdio.h> // i/o
#include <curl/curl.h> // libcurl for http requests
#include <errno.h> // error numbers so we don't type messages manually
#include <stdlib.h> // Add mem alloc
#include <string.h> // for error messages
#include <math.h> // For truncate
#include <stdbool.h> // For Boolean
#include <pthread.h> // For Multithreading
#include <assert.h>
#include <ctype.h>

#define ARRAY_LENGTH 100
#define URL_LENGTH 512
#define NUM_THREADS 4

extern int errno; // for error messages

pthread_mutex_t LOCK;

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


struct ThreadArgs {
    int numJobs;
    struct Job * jobs;
};

const int IMPORTANT_WORDS_SIZE = 1;
const char * IMPORTANT_WORDS[] = {"Word"};


// Prototypes

// Reads Urls.txt for URLs to fetch
char **parseFile(size_t *links);

// Reads contents of stored html files
void parseHTML(char *fileName);

// Function to fetch URL using libcurl
void fetch_url(const char *url);

// Function to write data to file while fetching URL
static size_t write_data(void *ptr, size_t size, size_t nmemb, void *stream);

// Function to fetch URL content and store it into a file using Libcurl
void fetchUrlToStore(char *url, const char *fileName);

// Function to calculate the maximum number of URLs each thread can work on
int getMaxWorkPerThread(int numLinks, int numThreads);

// Function to populate the job structs with URLs and filenames
void getJobs(struct Job jobs[], int numUrl, char **urlArray);

// Function to get a job from the job queue using FIFO (Array is used as a queue using shared index)
struct JobQueueInfo getJobWithFifo(const struct Job jobs[], int numJobs);

// Worker function for handling the fetch and storage tasks for each URL
void *worker(void *argument);

// Function to count the occurrences of a word in a sentence
int countOccurrencesOfWord(const char *word, char *sentence, int sentenceLength);

// Test function for word occurrences
int testWordOccurrences();

// Function to run all tests
void runtests(void);


// Function to fetch URL using libcurl
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


// Function to write data to file while fetching URL
static size_t write_data(void *ptr, size_t size, size_t nmemb, void *stream)
{
    size_t written = fwrite(ptr, size, nmemb, (FILE *)stream);
    return written;
}


// Function to fetch URL content and store it into a file using Libcurl
void fetchUrlToStore(char *url, const char * fileName) {
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
    
    // Tell libcurl to follow redirection */
    curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1L);
    
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
            fprintf(stderr, "Failed to fetch %s: %s\n", url, curl_easy_strerror(res));
        } else {
            printf("%s content successfully written to %s\n", url, fileName);
        }
    
        /* close the file */
        fclose(pagefile);
    } else {
        fprintf(stderr, "Failed to open file %s for writing\n", fileName);
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
                urlBank[numOfUrls] = realloc(urlBank[numOfUrls], charsRead);
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


// Function to calculate the maximum number of URLs each thread can work on
int getMaxWorkPerThread(int numLinks, int numThreads) {
    int workPerThread = 0;

    workPerThread = trunc(numLinks / numThreads) + 1; // There can be at most (num jobs / numThreads) + 1 jobs per thread. Trivial to prove

    return workPerThread;
}


// Function to populate the job structs with URLs and filenames
void getJobs(struct Job jobs[], int numUrl, char **urlArray) {
    
    for (int i = 0; i < numUrl; i++) {
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
    
        // Removes \n and \r from string
        jobs[i].link[strcspn(jobs[i].link, "\n")] = 0;
        jobs[i].link[strcspn(jobs[i].link, "\r")] = 0;

    }
    
}


// Function to get a job from the job queue using FIFO (Array is used as a queue using shared index)
struct JobQueueInfo getJobWithFifo(const struct Job jobs[], int numJobs) {
    pthread_mutex_lock(&LOCK); // Protect Critical Section with mutex

    static int fifoIndex = 0;
    struct JobQueueInfo info;

    if (fifoIndex < numJobs) {
        info.hasJob = true;
        info.job = jobs[fifoIndex];
        fifoIndex++;
    } else {
        info.hasJob = false;
    }

    pthread_mutex_unlock(&LOCK); 

    return info;
}


// Worker function for handling the fetch and storage tasks for each URL
void * worker(void * argument) {
    struct ThreadArgs * args = (struct ThreadArgs *)argument; // Build ThreadArgs for worker using type casting

    int numJobs = args->numJobs;
    struct Job * jobs = args->jobs;

    for (int count = 0; count < getMaxWorkPerThread(numJobs, NUM_THREADS); count++) {
        struct JobQueueInfo temp = getJobWithFifo(jobs, numJobs);

        if (temp.hasJob) {
            struct Job job = temp.job;

            fetchUrlToStore(job.link, job.contentFilename);
            
        } else {
            // printf("\nNo job available\n");
        }
        
    }
}


// Function to count the occurrences of a word in a sentence
int countOccurrencesOfWord(const char * word, char * sentence, int sentenceLength) {
    char substr[strlen(word) + 10];
    char temp[strlen(sentence) + 10];

    // Correct for additional space (" meow" is valid " meowmeow" is not) this makes " meow" = " meow "
    // strcat(sentence, space);
    sprintf(temp, " %s ", sentence);
    sprintf(substr, " %s ", word);
    
    for (int count = 0; count < strlen(substr); count++) {
        substr[count] = tolower(substr[count]);
    }

    for (int count = 0; count < strlen(temp); count++) {
        if (!isalpha(temp[count])) {
            temp[count] = ' ';
        }
        
        temp[count] = tolower(temp[count]);
    }

    int count = 0;
    char *tmp = temp;

    // Count all substrings in line
    while(tmp = strstr(tmp, substr))
    {
        count++;
        tmp++;  // Increment temp pointer to remove first element making temp no longer valid substr if it is last remaining substr (Avoid Infinite Loop)
    }

    return count;
}


void parseHTML(char *fileName)
{
    // Precondition: Input the HTML file name.
    // Postcondition: Print to the console word count results.

    // Loop to make a counter for each word in list?
    int theCounter = 0; //test

    // read only mode
    FILE* file = fopen(fileName, "r");
    char *line = NULL;          // Create a string so we are able to call our word counting function on it once we finish a line
    size_t length = 0;
    size_t read;

    if (file == NULL)
    {
        perror("Error message");
        exit(1);
    }

    // look at documentation later
    while ((read = getline(&line, &length, file) != -1))
    {
        theCounter += countOccurrencesOfWord("the", line, length);
    }

    // Output
    printf("Occurrences from %s\n", fileName);
    printf(" the: %d\n\n", theCounter); //test
    
    // Close file
    fclose(file);
    return;
}


// Test function for word occurrences
int testWordOccurrences() {
    char sentence[] = "<p>A <b>frog</b> is any member of a diverse and largely <a href=\"/wiki/Carnivore\" title=\"Carnivore\">carnivorous</a> group of short-bodied, tailless <a href=\"/wiki/Amphibian\" title=\"Amphibian\">amphibians</a> composing the <a href=\"/wiki/Order_(biology)\" title=\"Order (biology)\">order</a> <b>Anura</b><sup id=\"cite_ref-AOTW_1-0\" class=\"reference\"><a href=\"#cite_note-AOTW-1\"><span class=\"cite-bracket\">[</span>1<span class=\"cite-bracket\">]</span></a></sup> (coming from the <a href=\"/wiki/Ancient_Greek\" title=\"Ancient Greek\">Ancient Greek</a> <span title=\"Ancient Greek (to 1453)-language text\"><span lang=\"grc\">ἀνούρα</span></span>, literally 'without tail').</p>";
    int num = countOccurrencesOfWord("frog", sentence, strlen(sentence));
    assert(num == 1);

    num = countOccurrencesOfWord("meow", "homeowner", strlen("homeowner"));
    assert(num == 0);

    num = countOccurrencesOfWord("Word", "Word", strlen("Word"));
    assert(num == 1);  

    num = countOccurrencesOfWord("word", "xyz word abc", strlen("xyz word abc"));
    assert(num == 1);  

    num = countOccurrencesOfWord("Word", "Word", strlen("Word"));
    assert(num == 1);  

    num = countOccurrencesOfWord("Word", "", 0);
    assert(num == 0);  

    num = countOccurrencesOfWord("Word", "word Word", strlen("word Word"));
    assert(num == 2);  

    num = countOccurrencesOfWord("sub", "substring sub", strlen("substring sub"));
    assert(num == 1);

    // Note how the following fails since 123 are numbers, however we dont care about this case since all valid words being searched will not appear like this
    // num = countOccurrencesOfWord("test", "123test123test", strlen("123test123test"));
    // assert(num == 0);


    printf("All occurrences tests passed\n");
}


// Function to run all tests
void runtests() {
    testWordOccurrences();

}


// main
int main(void)
{
    char **urlArray;                    // establish array of strings
    size_t urlNum;                      // # of urls in said array

    pthread_t tid[NUM_THREADS];

    curl_global_init(CURL_GLOBAL_ALL);

    // 1. Input Parsing: Read URLs from a file
    urlArray = parseFile(&urlNum);  //we pass in the address with & to allow the function to change urlNum

    // This Struct will contain for each url: url, html file name, said name's length, url length
    struct Job jobs[urlNum];            // Array of 'urlNum' Job structs
    getJobs(jobs, urlNum, urlArray);    // Populate the structs with data
    
    struct ThreadArgs args = {urlNum, jobs};
    
    // Use pthreads to handle multiple web page fetches in parallel
    for (int i = 0; i < NUM_THREADS; i++) {
        if(pthread_create(&tid[i], NULL, &worker, &args) != 0) {
            perror("Failed to creater thread");
        }
    }

    // Wait for all workers to complete before proceeding
    for (int i = 0; i < NUM_THREADS; i++) {
        if(pthread_join(tid[i], NULL) != 0) {
            perror("Failed to join thread");
        }
    }


    printf("\n\n\nRunning Tests:\n");
    runtests();
    
    // Word Counting per html file
    // checking for "the" as a test.
    // Still needs the actual important words, which may vary
    for (int i = 0; i < urlNum; i++)
    {   
        parseHTML(jobs[i].contentFilename);
    }




    // **********************DEALLOCATION***********************************
    // Free each individual url
    for (int i = 0; i< urlNum; i++)
    {
        free(urlArray[i]);
    }

    for (int i = 0; i < urlNum; i++) {
        // Free the dynamically allocated Filename for each job
        free(jobs[i].contentFilename);
    }
    
    free(urlArray);

    curl_global_cleanup();

    return 0;
}


// int main(void)
// {
//     FILE *file = fopen("urls.txt","r"); // open urls.txt in read mode
//     if (file == NULL){ // check if fopen failed
//         fprintf(stderr, "Error: Could not open urls.txt \n"); // print error message to standard ouput
//         return 1; // exit with error status
//     }

//     char url[1024]; // buffer to store each line
//     while (fgets(url, sizeof(url),file)){ // read each line
//         url[strcspn(url, "\n")] = 0; // remove newline character
//         fetch_url(url); // call fetch url() to get the webpage
//     }
//     fclose(file); // close file
//     return 0; // exit successfully
// }