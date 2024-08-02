#ifndef SHAREFUNC_H
#define SHAREFUNC_H

typedef struct stack_s
{
    int *data;    // Pointer to dynamically allocated arr to store data
    int top;      // Pointer to the top of the stack
    int capacity; // Maximum capacity of the stack
    pthread_mutex_t lock;
} stack_t;

// Define the queue structure
typedef struct queue_s
{
    int *data;    // Array to store queue elements
    int front;    // Index of the front element
    int rear;     // Index of the rear element
    int size;     // Current number of elements in the queue
    int capacity; // Maximum capacity of the queue
    pthread_mutex_t lock;
} queue_t;

void *Malloc(size_t size);
void *Calloc(size_t nmemb, size_t size);
void pthreadCreate(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg);
void pthreadJoin(pthread_t thread, void **retval);

int replaceFileNameSuffix(char *filename, int suffix);

// Initialize the stack
void stackInit(stack_t *stack, int capacity);
int stackIsEmpty(stack_t *stack);
int stackIsFull(stack_t *stack);
void stackPush(stack_t *stack, int value);
int stackPop(stack_t *stack);
int stackPeek(stack_t *stack);
void stackFree(stack_t *stack);

// Function prototypes
void queueInit(queue_t *queue, int capacity);
int queueIsEmpty(queue_t *queue);
int queueIsFull(queue_t *queue);
void enqueue(queue_t *queue, int value);
int dequeue(queue_t *queue);
int queuePeek(queue_t *queue);
void queueFree(queue_t *queue);

int sumArr(int arr[], int size);
void swap(int *a, int *b);
int getArrMaxValue(int arr[], int size);
int getOneArrMinValue(int arr[], int size);
int get2ArrMaxValue(int row_num, int col_num, int arr[row_num][col_num]);
int get2ArrMinValue(int row_num, int col_num, int arr[row_num][col_num]);

void sortMaxBubble(int arr[], int n);
void sortMinBubble(int arr[], int n);
void sortMin2ArrBubble(int arr1[], int arr2[], int n);
void getSortedMaxIndices(int arr[], int indices[], int size);
void getSortedMinIndices(int arr[], int indices[], int size);
void getSortedMinIndicesPointer(int *arr, int *indices, int size);

void printArrINT(int size, int *arr, char *msg);
void printArrDOUBLE(int size, double *arr, char *msg);
void print2ArrINT(int row_num, int col_num, int arr[row_num][col_num], char *msg);
void print2ArrDOUBLE(int row_num, int col_num, double arr[row_num][col_num], char *msg);
void print2PointerArrINT(int row_num, int col_num, int **arr, char *msg);
void print3ArrINT(int first_num, int row_num, int col_num, int arr[first_num][row_num][col_num], char *msg);

int getArrIndex(int *arr, int size, int value);

void memcpyMove(void *dst, void *src, size_t size, int *p_cur_offset);

void mergeFail(int *arr1, int *arr2, int *result, int a);

void create2PointerArrINT(int **bufP, int ***arrP, size_t rows, size_t cols);
void delete2ArrINT(int *buf, int **arr);
void create2PointerArrCHAR(char **bufP, char ***arrP, size_t rows, size_t cols);
void delete2ArrCHAR(char *buf, char **arr);
void create2PointerArrUC(unsigned char **bufP, unsigned char ***arrP, size_t rows, size_t cols);
void delete2ArrUC(unsigned char *buf, unsigned char **arr);
void create3PointerArrINT(int **bufP, int ***arrP, int ****twoArrP, size_t firstNum, size_t rows, size_t cols);
void delete3ArrINT(int *buf, int **arr, int ***twoArr);
void create3PointerArrCHAR(char **bufP, char ***arrP, char ****twoArrP, size_t firstNum, size_t rows, size_t cols);
void delete3ArrCHAR(char *buf, char **arr, char ***twoArr);
void create3PointerArrUC(unsigned char **bufP, unsigned char ***arrP, unsigned char ****twoArrP, size_t firstNum, size_t rows, size_t cols);
void delete3ArrUC(unsigned char *buf, unsigned char **arr, unsigned char ***twoArr);
void create4PointerArrUC(unsigned char **bufP, unsigned char ***arrP, unsigned char ****twoArrP,
                         unsigned char *****threeArrP, size_t firstNum, size_t secondNum, size_t rows, size_t cols);
void delete4ArrUC(unsigned char *buf, unsigned char **arr, unsigned char ***twoArr, unsigned char ****threeArr);


void free2(void *one, void *two);
void free3(void *one, void *two, void *three);
void free4(void *one, void *two, void *three, void *four);

void shutdown2Arr(int **arr, int rows, int cols);

#endif // SHARE_FUNC_H
