#define _GNU_SOURCE // For getline
#include <string.h> // For string manipulation
#include <stdio.h> // i/o (e.g., printf, fopen, fread)
#include <curl/curl.h> // libcurl for http requests
#include <errno.h> // error numbers so we don't type messages manually
#include <stdlib.h> // Add mem alloc (malloc, realloc, free)
#include <string.h> // for error messages
#include <math.h> // For truncate
#include <stdbool.h> // For Boolean
#include <pthread.h> // For Multithreading
#include <assert.h> // assertions during testing
#include <ctype.h> // character checks (e.g., isalpha, 

#define ARRAY_LENGTH 100 // how many urls you expect at a time (starting point, not a hard limit due to realloc) if we overflow it we realloc
#define URL_LENGTH 512 // max characters per url before reallocation
#define NUM_THREADS 4 // controls how many workers youll spin up

extern int errno; // for error messages (stores the most recent error code set by a system call)

pthread_mutex_t LOCK; // a gate where only one thread can pass at a time, use this to protect shared resources job queue index

// Define a structure for Job, represents a single unit of work: fetching one webpage and saving it to a file
struct Job {
    char *link;                      // Link to make request
    char *contentFilename;          // Filename to store saved content
    int contentFileNameLength;  // Length of the content filename string
    int linkLength;              // Length of the link string
};

// wrapper for safe job queue extraction, "Did I get a job? If yes, here's the job" 
struct JobQueueInfo {
    bool hasJob;
    struct Job job;
};

// this is what you pass to each thread, it tells them "here's the list of jobs" and "here's how many there are"
struct ThreadArgs {
    int numJobs;
    struct Job * jobs;
};
typedef struct {
    const char *word;
    const char *sentence;
    int sentenceLength;
    int localCount; // thread-local count
} ThreadData;

// keywords we count in each HTML file, stored in an array so we can loop it over when counting
const int IMPORTANT_WORDS_SIZE = 3;
const char * IMPORTANT_WORDS[] = {"data", "science", "algorithm"};



// Prototypes

// Reads Urls.txt for URLs to fetch
char **parseFile(size_t *links); 

// Reads contents of stored html files
void parseHTML(char *fileName, const char *url);


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
int countOccurrencesOfWord(const char *word, const char *sentence, int sentenceLength);

// Function to make countOccurrences multithreaded assigns each word to its own thread.
void *countWordOccurrences(void *arg);

// Helper function that returns the index of a target word in an array of words
int findWordIndex(const char **words, int size, const char *target);

// Test function for word occurrences
int testWordOccurrences();

// Function to run all tests
void runtests(void);


// Function to fetch URL using libcurl
void fetch_url(const char *url){ // function to fetch urls by taking url as input and fetch using libcurl
    CURL *curl = curl_easy_init(); // initialize curl session (curl object) -> like opening a browser tab
    if (curl){
        printf("Fetching %s\n", url); // print url being fetched
        curl_easy_setopt(curl, CURLOPT_URL, url); // set url for the request
        CURLcode res = curl_easy_perform(curl); //perform http request, get response

        if(res != CURLE_OK){ // check if request failed and prints error message
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        
            curl_easy_cleanup(curl); // cleanup curl session, close session and free up internal memory
        }
    }
}


// Function to write data to file while fetching URL
static size_t write_data(void *ptr, size_t size, size_t nmemb, void *stream) // pointer to a buffer containing the next chunk of HTML content, size x nmemb bytes (gives number of bytes to write), stream is the file pointer you gave libcurl to write into
{
    size_t written = fwrite(ptr, size, nmemb, (FILE *)stream); // write this data chunk into our file
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
            printf("%s Content successfully written to %s\n", url, fileName);
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
    char **urlBank = malloc(sizeof(char *) * ARRAY_LENGTH);   // We will realloc if we need more lines and chars (list of urls, an array of char*)
    size_t numOfUrls = 0;
    size_t charsRead = 0;                                   // tracks chars read PER LINE
    char c;                                                 // read in each char

    do 
    {
        c = fgetc(file); // read in a character one at a time

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
            urlBank[numOfUrls][charsRead-1] = '\0';                                     // -1 so we don't catch \n (trim off \n with -1)

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
            // Check if we need to increase char limit (expand each string if longer than 512 chars)
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
    // this part trims the array to fit the actual number of urls, set output parameter to *links, returns string array
}


// Function to calculate the maximum number of URLs each thread can work on
int getMaxWorkPerThread(int numLinks, int numThreads) {
    int workPerThread = 0;

    workPerThread = trunc(numLinks / numThreads) + 1; // There can be at most (num jobs / numThreads) + 1 jobs per thread. Trivial to prove

    return workPerThread;
}


// Function to populate the job structs with URLs and filenames
void getJobs(struct Job jobs[], int numUrl, char **urlArray) {
    
    for (int i = 0; i < numUrl; i++) { // loop over each url parsed earlier from urls.txt
        // allocate space for a string like page1.html, calloc() zeros out the buffer to prevent garbage values if i accidentally read uninitialized memory
        int fileNameLen = 50;
        char *filename = (char *)calloc(fileNameLen, sizeof(char)); // Allocate memory for filename

        // Create the filename as "page<count>.html"
        sprintf(filename, "page%d.html", i + 1);

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
    struct JobQueueInfo info; // returns whether there was a hasJob or the actual job if any

    // check to make sure we havent run out of jobs, set hasjob to true, increment fifoIndex so the next thread gets the job
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
// ask job queue for a task, if task exists, fetch the url and save it
void * worker(void * argument) {
    // cast void input into your known ThreadArgs struct, give each thread access to jobs[] list and numJobs (total)
    struct ThreadArgs * args = (struct ThreadArgs *)argument; // Build ThreadArgs for worker using type casting

    int numJobs = args->numJobs;
    struct Job * jobs = args->jobs;

    for (int count = 0; count < numJobs; count++) { // try at most numJobs times to pull work, getJobWithFifo() will return hasJob = false and thread exists, thats why it doesnt loop infinitely
        struct JobQueueInfo temp = getJobWithFifo(jobs, numJobs); // fetches a job safely one at a time across threads using the mutex lock inside getJobWithFifo

        if (temp.hasJob) {
            struct Job job = temp.job;

            fetchUrlToStore(job.link, job.contentFilename);
            
        } else {
            return NULL;
            // printf("\nNo job available\n");
        }
        
    }
}


// Function to count the occurrences of a word in a sentence
int countOccurrencesOfWord(const char * word, const char * sentence, int sentenceLength) {
    // Make substr and temp bigger than word and sentences
    char substr[strlen(word) + 10]; 
    char temp[strlen(sentence) + 10];

    // Correct for additional space (" meow" is valid " meowmeow" is not) this makes " meow" = " meow "
    // strcat(sentence, space);
    // matches whole words, "data" matches "data" and not "metadata"
    sprintf(temp, " %s ", sentence);
    sprintf(substr, " %s ", word);
    
    for (int count = 0; count < strlen(substr); count++) {
        substr[count] = tolower(substr[count]); // make everything lowercase "Data" becomes "data"
    }

    for (int count = 0; count < strlen(temp); count++) {
        if (!isalpha(temp[count])) { // convert symbols punctuations and numbers to spaces, <p> becomes just p
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

void *countWordOccurrences(void *arg) {
    ThreadData *data = (ThreadData *)arg;

    // Count word occurrences in the line
    data->localCount = countOccurrencesOfWord(data->word, data->sentence, data->sentenceLength);

    // Return the data pointer with count set
    return data;
}

int findWordIndex(const char **words, int size, const char *target) {
    // Iterate through the list of words
    for (int i = 0; i < size; i++) {
        // If a match is found, return the index
        if (strcmp(words[i], target) == 0)
            return i;
    }
    return -1;
}

void parseHTML(char *fileName, const char *url)

{
    // Precondition: Input the HTML file name.
    // Postcondition: Print to the console word count results.

    // Array to hold counters for each word in the IMPORTANT_WORDS list
    int wordCounters[IMPORTANT_WORDS_SIZE]; 
    for (int i = 0; i < IMPORTANT_WORDS_SIZE; i++) {
        wordCounters[i] = 0; // Initialize counters to zero
    }

    // Open the HTML file in read-only mode
    FILE* file = fopen(fileName, "r");
    char *line = NULL;          // Create a string to store each line for processing
    size_t length = 0;          // Track the length of the line read

    if (file == NULL) {
        perror("Error message"); // Print error and exit if file can't be opened
        exit(1);
    }

    // Read the file line by line
    while (getline(&line, &length, file) != -1) {
        int i = 0;
        while (i < IMPORTANT_WORDS_SIZE) {
            pthread_t threads[NUM_THREADS];
            ThreadData *threadDataArray[NUM_THREADS];
            int threadCount = 0;

            // Create up to NUM_THREADS threads
            for (; threadCount < NUM_THREADS && i < IMPORTANT_WORDS_SIZE; threadCount++, i++) {
                ThreadData *data = malloc(sizeof(ThreadData));
                data->word = IMPORTANT_WORDS[i];
                data->sentence = line;
                data->sentenceLength = length;
                data->localCount = 0;

                threadDataArray[threadCount] = data;

                if (pthread_create(&threads[threadCount], NULL, countWordOccurrences, (void *)data) != 0) {
                    perror("Failed to create thread");
                    free(data);
                    threadCount--;  // Only count successful threads
                }
            }

            // Join only the threads that were created
            for (int j = 0; j < threadCount; j++) {
                void *ret;
                pthread_join(threads[j], &ret);

                ThreadData *data = (ThreadData *)ret;
                int index = findWordIndex(IMPORTANT_WORDS, IMPORTANT_WORDS_SIZE, data->word);
                if (index != -1) {
                    wordCounters[index] += data->localCount;
                }

                free(data);
            }
        }
    }

    // Output: Display word counts for this HTML file
    printf("Occurrences from %s (URL: %s):\n", fileName, url);
    for (int i = 0; i < IMPORTANT_WORDS_SIZE; i++) {
        printf(" %s: %d\n", IMPORTANT_WORDS[i], wordCounters[i]);
    }
    printf("\n");

    // Close the file and free allocated memory for the line buffer
    fclose(file);
    free(line);
}


// Test function for word occurrences
int testWordOccurrences() {
    char sentence[] = "<p>A <b>frog</b> is any member of a diverse and largely <a href=\"/wiki/Carnivore\" title=\"Carnivore\">carnivorous</a> group of short-bodied, tailless <a href=\"/wiki/Amphibian\" title=\"Amphibian\">amphibians</a> composing the <a href=\"/wiki/Order_(biology)\" title=\"Order (biology)\">order</a> <b>Anura</b><sup id=\"cite_ref-AOTW_1-0\" class=\"reference\"><a href=\"#cite_note-AOTW-1\"><span class=\"cite-bracket\">[</span>1<span class=\"cite-bracket\">]</span></a></sup> (coming from the <a href=\"/wiki/Ancient_Greek\" title=\"Ancient Greek\">Ancient Greek</a> <span title=\"Ancient Greek (to 1453)-language text\"><span lang=\"grc\">ἀνούρα</span></span>, literally 'without tail').</p>";
    int num = countOccurrencesOfWord("frog", sentence, strlen(sentence)); // test if frog appears once, ensure it works with tags and links
    assert(num == 1);

    num = countOccurrencesOfWord("meow", "homeowner", strlen("homeowner")); // test that substrings like meow and homeowner dont match
    assert(num == 0);

    num = countOccurrencesOfWord("Word", "Word", strlen("Word")); // case insensitive match, expect it to be found
    assert(num == 1);  

    num = countOccurrencesOfWord("word", "xyz word abc", strlen("xyz word abc"));
    assert(num == 1);  

    num = countOccurrencesOfWord("Word", "Word", strlen("Word"));
    assert(num == 1);  

    num = countOccurrencesOfWord("Word", "", 0); // empty input test, must gracefully handle
    assert(num == 0);  

    num = countOccurrencesOfWord("Word", "word Word", strlen("word Word"));
    assert(num == 2);  

    num = countOccurrencesOfWord("sub", "substring sub", strlen("substring sub")); // test if only exact word match count, first 'sub' embedded in 'substring' shouldnt be counted
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
    char **urlArray;                    // establish array of strings, pointer to pointer to char
    size_t urlNum;                      // # of urls in said array

    pthread_t tid[NUM_THREADS]; // tid[i] tracks each thread spun up with pthread_create

    curl_global_init(CURL_GLOBAL_ALL); // initialize curl resources

    // 1. Input Parsing: Read URLs from a file
    urlArray = parseFile(&urlNum);  //we pass in the address with & to allow the function to change urlNum read urls.txt line by line, store each URL in urlArray and return the amount of times we found urlNum

    // This Struct will contain for each url: url, html file name, said name's length, url length
    struct Job jobs[urlNum];            // Array of 'urlNum' Job structs (ex. 10 urls is 10 structs)
    getJobs(jobs, urlNum, urlArray);    // Populate the structs with data
    
    struct ThreadArgs args = {urlNum, jobs}; // create an instance of Threadargs
    
    // Use pthreads to handle multiple web page fetches in parallel
    for (int i = 0; i < NUM_THREADS; i++) {
        if(pthread_create(&tid[i], NULL, &worker, &args) != 0) {
            perror("Failed to create thread");
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
        parseHTML(jobs[i].contentFilename, jobs[i].link);
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
