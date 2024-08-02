#include "ECConfig.h"

void *Malloc(size_t size)
{
    void *ptr = malloc(size);
    if (ptr == NULL)
        ERROR_RETURN_NULL("Memory allocation failed");
    return ptr;
}
void *Calloc(size_t nmemb, size_t size)
{
    void *ptr = calloc(nmemb, size);
    if (ptr == NULL)
        ERROR_RETURN_NULL("Memory allocation failed");
    return ptr;
}

void pthreadCreate(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg)
{
    int ret = pthread_create(thread, attr, start_routine, arg);
    if (ret != 0)
        ERROR_RETURN_NULL("Failed to create thread");
    return;
}

void pthreadJoin(pthread_t thread, void **retval)
{
    int ret = pthread_join(thread, retval);
    if (ret != 0)
        ERROR_RETURN_NULL("Failed to join thread");
    return;
}

int replaceFileNameSuffix(char *filename, int suffix)
{
    // Find the position of the last '-' character
    char *dash_pos = strrchr(filename, '_');
    if (dash_pos == NULL)
        ERROR_RETURN_VALUE("Invalid filename");

    // Find the position of the character to replace
    int pos = dash_pos - filename + 1;

    // Replace the number with the new number
    char new_num_str[12];
    sprintf(new_num_str, "%d", suffix);
    strcpy(&filename[pos], new_num_str);

    return EC_OK;
}

// Initialize the stack
void stackInit(stack_t *stack, int capacity)
{
    stack->data = (int *)Malloc(capacity * sizeof(int));
    stack->top = -1;            // Initialize the top pointer to -1, indicating an empty stack
    stack->capacity = capacity; // Set the maximum capacity of the stack
    pthread_mutex_init(&stack->lock, NULL);
}

// Check if the stack is empty
int stackIsEmpty(stack_t *stack)
{
    return stack->top == -1;
}

// Check if the stack is full
int stackIsFull(stack_t *stack)
{
    return stack->top == stack->capacity - 1;
}

// Push operation
void stackPush(stack_t *stack, int value)
{
    if (stackIsFull(stack))
        ERROR_RETURN_NULL("stack is full, cannot push element.");
    pthread_mutex_lock(&stack->lock);
    stack->data[++stack->top] = value;
    pthread_mutex_lock(&stack->lock);
}

// Pop operation
int stackPop(stack_t *stack)
{
    if (stackIsEmpty(stack))
        ERROR_RETURN_NULL("stack is empty, cannot pop element.");
    pthread_mutex_lock(&stack->lock);
    return stack->data[stack->top--];
    pthread_mutex_unlock(&stack->lock);
}

// Peek operation to get the top element
int stackPeek(stack_t *stack)
{
    if (stackIsEmpty(stack))
        ERROR_RETURN_NULL("stack is empty, cannot peek element.");

    return stack->data[stack->top];
}

// Free memory allocated for the stack
void stackFree(stack_t *stack)
{
    pthread_mutex_lock(&stack->lock);
    free(stack->data);
    pthread_mutex_unlock(&stack->lock);
}

// Initialize the queue
void queueInit(queue_t *queue, int capacity)
{
    queue->data = (int *)Malloc(capacity * sizeof(int));
    queue->front = 0;           // Initialize the front pointer to 0, indicating an empty queue
    queue->rear = -1;           // Initialize the rear pointer to -1, indicating an empty queue
    queue->size = 0;            // Initialize the size of the queue to 0
    queue->capacity = capacity; // Set the maximum capacity of the queue
    pthread_mutex_init(&queue->lock, NULL);
}

// Check if the queue is empty
int queueIsEmpty(queue_t *queue)
{
    return queue->size == 0;
}

// Check if the queue is full
int queueIsFull(queue_t *queue)
{
    return queue->size == queue->capacity;
}

// Enqueue operation
void enqueue(queue_t *queue, int value)
{
    if (queueIsFull(queue))
        ERROR_RETURN_NULL("Queue is full, cannot enqueue element.");
    pthread_mutex_lock(&queue->lock);
    queue->rear = (queue->rear + 1) % queue->capacity; // Circular increment of rear pointer
    queue->data[queue->rear] = value;
    queue->size++;
    pthread_mutex_unlock(&queue->lock);
}

// Dequeue operation
int dequeue(queue_t *queue)
{
    if (queueIsEmpty(queue))
        ERROR_RETURN_NULL("Queue is empty, cannot dequeue element.");
    pthread_mutex_lock(&queue->lock);
    int value = queue->data[queue->front];
    queue->front = (queue->front + 1) % queue->capacity; // Circular increment of front pointer
    queue->size--;
    pthread_mutex_unlock(&queue->lock);
    return value;
}

// Peek operation to get the front element
int queuePeek(queue_t *queue)
{
    if (queueIsEmpty(queue))
        ERROR_RETURN_NULL("Queue is empty, cannot peek element.");
    return queue->data[queue->front];
}

// Free memory allocated for the queue
void queueFree(queue_t *queue)
{
    free(queue->data);
    pthread_mutex_destroy(&queue->lock);
}

int sumArr(int arr[], int size)
{
    int sum = 0;
    for (int i = 0; i < size; i++)
        sum += arr[i];
    return sum;
}

void swap(int *a, int *b)
{
    int temp = *a;
    *a = *b;
    *b = temp;
}

void sortMaxBubble(int arr[], int n)
{
    for (int i = 0; i < n - 1; i++)
        for (int j = 0; j < n - i - 1; j++)
            if (arr[j] < arr[j + 1])
                swap(&arr[j], &arr[j + 1]);
}

void sortMinBubble(int arr[], int n)
{
    for (int i = 0; i < n - 1; i++)
        for (int j = 0; j < n - i - 1; j++)
            if (arr[j] > arr[j + 1])
                swap(&arr[j], &arr[j + 1]);
}

void sortMin2ArrBubble(int arr1[], int arr2[], int n)
{
    for (int i = 0; i < n - 1; i++)
        for (int j = 0; j < n - i - 1; j++)
            if (arr1[j] > arr1[j + 1])
            {
                swap(&arr1[j], &arr1[j + 1]);
                swap(&arr2[j], &arr2[j + 1]);
            }
}

int getArrMaxValue(int arr[], int size)
{
    int max = arr[0];
    for (int i = 0; i < size; i++)
        if (arr[i] > max)
            max = arr[i];
    return max;
}

int getOneArrMinValue(int arr[], int size)
{
    int min = arr[0];
    for (int i = 0; i < size; i++)
        if (arr[i] < min)
            min = arr[i];
    return min;
}

int get2ArrMaxValue(int row_num, int col_num, int arr[row_num][col_num])
{
    int max = arr[0][0];
    for (int i = 0; i < row_num; i++)
        for (int j = 0; j < col_num; j++)
            if (arr[i][j] > max)
                max = arr[i][j];
    return max;
}

int get2ArrMinValue(int row_num, int col_num, int arr[row_num][col_num])
{
    int min = arr[0][0];
    for (int i = 0; i < row_num; i++)
        for (int j = 0; j < col_num; j++)
            if (arr[i][j] < min)
                min = arr[i][j];
    return min;
}

void getSortedMaxIndices(int arr[], int indices[], int size)
{
    for (int i = 0; i < size; i++)
        indices[i] = i;
    for (int i = 0; i < size - 1; i++)
        for (int j = 0; j < size - i - 1; j++)
            if (arr[indices[j]] < arr[indices[j + 1]])
                swap(&indices[j], &indices[j + 1]);
}

void getSortedMinIndices(int arr[], int indices[], int size)
{
    for (int i = 0; i < size; i++)
        indices[i] = i;
    for (int i = 0; i < size - 1; i++)
        for (int j = 0; j < size - i - 1; j++)
            if (arr[indices[j]] > arr[indices[j + 1]])
                swap(&indices[j], &indices[j + 1]);
}

void getSortedMinIndicesPointer(int *arr, int *indices, int size)
{
    for (int i = 0; i < size; i++)
        indices[i] = i;
    for (int i = 0; i < size - 1; i++)
        for (int j = 0; j < size - i - 1; j++)
            if (arr[indices[j]] > arr[indices[j + 1]])
                swap(&indices[j], &indices[j + 1]);
}

void printArrINT(int size, int *arr, char *msg)
{
    printf("%s:\t", msg);
    for (int i = 0; i < size; i++)
        printf("%d\t", arr[i]);
    printf("\n");
    PRINT_FLUSH;
}

void printArrDOUBLE(int size, double *arr, char *msg)
{
    printf("%s:\t", msg);
    for (int i = 0; i < size; i++)
        printf("%0.4f\t", arr[i]);
    printf("\n");
    PRINT_FLUSH;
}

int getNumDigits(int num)
{
    int count = 0;
    while (num != 0)
    {
        num /= 10;
        count++;
    }
    return count;
}

void print2ArrINT(int row_num, int col_num, int arr[row_num][col_num], char *msg)
{

    printf("%s:\n", msg);
    int max_Value = get2ArrMaxValue(row_num, col_num, arr);
    int field_Width = getNumDigits(max_Value) + 2;
    for (int i = 0; i < row_num; i++)
    {
        printf("[%d]:\t", i);
        for (int j = 0; j < col_num; j++)
            printf("%*d ", field_Width, arr[i][j]);
        printf("\n");
    }
    PRINT_FLUSH;
}

void print2ArrDOUBLE(int row_num, int col_num, double arr[row_num][col_num], char *msg)
{
    printf("%s:\n", msg);
    for (int i = 0; i < row_num; i++)
    {
        printf("[%d]:\t", i);
        for (int j = 0; j < col_num; j++)
            printf("%0.4f\t", arr[i][j]);
        printf("\n");
    }
    PRINT_FLUSH;
}

void print2PointerArrINT(int row_num, int col_num, int **arr, char *msg)
{

    printf("%s:\n", msg);
    for (int i = 0; i < row_num; i++)
    {
        printf("[%d]:\t", i);
        for (int j = 0; j < col_num; j++)
            printf("%d ", arr[i][j]);
        printf("\n");
    }
    PRINT_FLUSH;
}

void print3ArrINT(int first_num, int row_num, int col_num, int arr[first_num][row_num][col_num], char *msg)
{

    printf("%s:\n", msg);
    for (int k = 0; k < first_num; k++)
    {
        int max_Value = get2ArrMaxValue(row_num, col_num, arr[k]);
        int field_Width = getNumDigits(max_Value) + 2;
        for (int i = 0; i < row_num; i++)
        {
            printf("[%d-%d]:\t", k, i);
            for (int j = 0; j < col_num; j++)
                printf("%*d ", field_Width, arr[k][i][j]);
            printf("\n");
        }
    }
    PRINT_FLUSH;
}

int getArrIndex(int *arr, int size, int value)
{
    for (int i = 0; i < size; i++)
        if (arr[i] == value)
            return i;
    return EC_ERROR;
}

void memcpyMove(void *dst, void *src, size_t size, int *p_cur_offset)
{
    memcpy(dst, src, size);
    (*p_cur_offset) += size;
}

void mergeFail(int *arr1, int *arr2, int *result, int a)
{
    int *copy1 = (int *)Malloc(a * sizeof(int));
    int *copy2 = (int *)Malloc(a * sizeof(int));
    memcpy(copy1, arr1, a * sizeof(int));
    memcpy(copy2, arr2, a * sizeof(int));

    int i = 0, j = 0, k = 0;

    while (i < a && copy1[i] != -1)
        i++;
    int b = i;
    while (j < a && copy2[j] != -1)
        j++;
    int c = j;

    i = 0;
    j = 0;
    while (i < b && j < c && k < a)
    {
        if (copy1[i] < copy2[j])
        {
            if (k == 0 || result[k - 1] != copy1[i])
                result[k++] = copy1[i];
            i++;
        }
        else if (copy1[i] > copy2[j])
        {
            if (k == 0 || result[k - 1] != copy2[j])
                result[k++] = copy2[j];
            j++;
        }
        else
        { // copy1[i] == copy2[j]
            if (k == 0 || result[k - 1] != copy1[i])
                result[k++] = copy1[i];
            i++;
            j++;
        }
    }

    while (i < b && k < a)
    {
        if (k == 0 || result[k - 1] != copy1[i])
            result[k++] = copy1[i];
        i++;
    }

    while (j < c && k < a)
    {
        if (k == 0 || result[k - 1] != copy2[j])
            result[k++] = copy2[j];
        j++;
    }

    while (k < a)
        result[k++] = -1;

    free(copy1);
    free(copy2);
}

void create2PointerArrINT(int **bufP, int ***arrP, size_t rows, size_t cols)
{
    (*bufP) = (int *)Malloc((size_t)rows * cols * sizeof(int));
    (*arrP) = (int **)Malloc((size_t)rows * sizeof(int *));
    for (size_t i = 0; i < rows; i++)
        (*arrP)[i] = (*bufP) + i * cols;
}

void delete2ArrINT(int *buf, int **arr)
{
    free(buf);
    free(arr);
}

void create2PointerArrCHAR(char **bufP, char ***arrP, size_t rows, size_t cols)
{
    (*bufP) = (char *)Malloc((size_t)rows * cols * sizeof(char));
    (*arrP) = (char **)Malloc((size_t)rows * sizeof(char *));
    for (size_t i = 0; i < rows; i++)
        (*arrP)[i] = (*bufP) + i * cols;
}

void delete2ArrCHAR(char *buf, char **arr)
{
    free(buf);
    free(arr);
}

void create2PointerArrUC(unsigned char **bufP, unsigned char ***arrP, size_t rows, size_t cols)
{
    (*bufP) = (unsigned char *)Malloc((size_t)rows * cols * sizeof(unsigned char));
    (*arrP) = (unsigned char **)Malloc((size_t)rows * sizeof(unsigned char *));
    for (size_t i = 0; i < rows; i++)
        (*arrP)[i] = (*bufP) + i * cols;
}

void delete2ArrUC(unsigned char *buf, unsigned char **arr)
{
    free(buf);
    free(arr);
}

void create3PointerArrINT(int **bufP, int ***arrP, int ****twoArrP, size_t firstNum, size_t rows, size_t cols)
{
    (*bufP) = (int *)Malloc((size_t)firstNum * rows * cols * sizeof(int));
    (*arrP) = (int **)Malloc((size_t)firstNum * rows * sizeof(int *));
    (*twoArrP) = (int ***)Malloc((size_t)firstNum * sizeof(int **));
    for (size_t i = 0; i < firstNum * rows; i++)
        (*arrP)[i] = (*bufP) + i * cols;
    for (size_t i = 0; i < firstNum; i++)
        (*twoArrP)[i] = (*arrP) + i * rows;
}

void delete3ArrINT(int *buf, int **arr, int ***twoArr)
{
    free(buf);
    free(arr);
    free(twoArr);
}

void create3PointerArrCHAR(char **bufP, char ***arrP, char ****twoArrP, size_t firstNum, size_t rows, size_t cols)
{
    (*bufP) = (char *)Malloc((size_t)firstNum * rows * cols * sizeof(char));
    (*arrP) = (char **)Malloc((size_t)firstNum * rows * sizeof(char *));
    (*twoArrP) = (char ***)Malloc((size_t)firstNum * sizeof(char **));
    for (size_t i = 0; i < firstNum * rows; i++)
        (*arrP)[i] = (*bufP) + i * cols;
    for (size_t i = 0; i < firstNum; i++)
        (*twoArrP)[i] = (*arrP) + i * rows;
}

void delete3ArrCHAR(char *buf, char **arr, char ***twoArr)
{
    free(buf);
    free(arr);
    free(twoArr);
}

void create3PointerArrUC(unsigned char **bufP, unsigned char ***arrP, unsigned char ****twoArrP, size_t firstNum, size_t rows, size_t cols)
{
    (*bufP) = (unsigned char *)Malloc((size_t)firstNum * rows * cols * sizeof(unsigned char));
    (*arrP) = (unsigned char **)Malloc((size_t)firstNum * rows * sizeof(unsigned char *));
    (*twoArrP) = (unsigned char ***)Malloc((size_t)firstNum * sizeof(unsigned char **));
    for (size_t i = 0; i < firstNum * rows; i++)
        (*arrP)[i] = (*bufP) + i * cols;
    for (size_t i = 0; i < firstNum; i++)
        (*twoArrP)[i] = (*arrP) + i * rows;
}

void delete3ArrUC(unsigned char *buf, unsigned char **arr, unsigned char ***twoArr)
{
    free(buf);
    free(arr);
    free(twoArr);
}

void create4PointerArrUC(unsigned char **bufP, unsigned char ***arrP, unsigned char ****twoArrP,
                         unsigned char *****threeArrP, size_t firstNum, size_t secondNum, size_t rows, size_t cols)
{
    (*bufP) = (unsigned char *)Malloc((size_t)firstNum * secondNum * rows * cols * sizeof(unsigned char));
    (*arrP) = (unsigned char **)Malloc((size_t)firstNum * secondNum * rows * sizeof(unsigned char *));
    (*twoArrP) = (unsigned char ***)Malloc((size_t)firstNum * secondNum * sizeof(unsigned char **));
    (*threeArrP) = (unsigned char ****)Malloc((size_t)firstNum * sizeof(unsigned char ***));
    for (size_t i = 0; i < firstNum * secondNum * rows; i++)
        (*arrP)[i] = (*bufP) + i * cols;
    for (size_t i = 0; i < firstNum * secondNum; i++)
        (*twoArrP)[i] = (*arrP) + i * rows;
    for (size_t i = 0; i < firstNum; i++)
        (*threeArrP)[i] = (*twoArrP) + i * secondNum;
}

void delete4ArrUC(unsigned char *buf, unsigned char **arr, unsigned char ***twoArr, unsigned char ****threeArr)
{
    free(buf);
    free(arr);
    free(twoArr);
    free(threeArr);
}

void free2(void *one, void *two)
{
    free(one);
    free(two);
}

void free3(void *one, void *two, void *three)
{
    free(one);
    free(two);
    free(three);
}

void free4(void *one, void *two, void *three, void *four)
{
    free(one);
    free(two);
    free(three);
    free(four);
}

void shutdown2Arr(int **arr, int rows, int cols)
{
    for (int i = 0; i < rows; i++)
        for (int j = 0; j < cols; j++)
            shutdown(arr[i][j], SHUT_RDWR);
}