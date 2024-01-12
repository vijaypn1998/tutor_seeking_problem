#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <assert.h>
#include <pthread.h>
#include <sys/time.h>
#include <semaphore.h> 

int num_students; // Program argument.
int num_tutors; // Program argument.
int chairs_available; //Program argument.
int num_help; //Program argument.
int total_chairs; //Used to calculate number of students in queue(chairs occupied).
int *tutors_close; //Set when tutor threads need to close.
int total_help = 0; //total number of students helped by tutors.
int current_help = 0; //current number of students getting help.

sem_t chair_lock; //for occupying chair.
sem_t list_lock; // for adding to waiting list.
sem_t queue_lock; // for getting in queue.
sem_t total_help_lock; // for total helps.
sem_t current_help_lock; // for current helps.

sem_t *wait_for_tutor; //used by student/tutor.
sem_t *wait_for_coordinator; // used by tutor/co-ordinate.
sem_t wait_for_student; // used by co-ordinate/student.

//keeps track of students' info.
struct student_node {
    long s_id;
    int s_help;
    long t_id;
};

//for queuing.
struct queue_node {
    long s_id;
    int s_help;
    struct queue_node *next;
};

//for waiting in line.
struct list_node {
    long s_id;
    int s_help;
    struct list_node *prev;
    struct list_node *next;
};

struct student_node *student_array;
struct queue_node *queue_head = NULL;
struct list_node *list_head = NULL;
struct list_node *list_tail = NULL;

struct list_node *new_list_node(long s_id, int s_help); //to get a new node for occupying chair.
void push(struct list_node **head_ref, struct list_node **tail_ref, long s_id, int s_help); //occupying a chair.
struct list_node *pop(struct list_node **head_ref, struct list_node **tail_ref); // to remove from waiting list.
struct queue_node *new_queue_node(long s_id, int s_help); // to get a new node for queue.
void enqueue(struct queue_node **head_ref, long s_id, int s_help); // inserting a student into a queue based on his priority.
struct queue_node *dequeue(struct queue_node **head_ref); //getting the highest priority student for tutor.
static void *student_thread(void *arg); //emulates a student.
static void *coordinator_thread(void *arg); // emulates a co-ordinator.
static void *tutor_thread(void *arg);// emulates a tutor.
int random_number(int low, int up); // getting a random number, required to simulate programming time by student.

struct list_node *new_list_node(long s_id, int s_help){

    struct list_node *new_node = (struct list_node *)malloc(sizeof(struct list_node));
    new_node->s_id = s_id;
    new_node->s_help = s_help;
    new_node->next = NULL;
    new_node->prev = NULL;
    return new_node;
}

void push(struct list_node **head_ref, struct list_node **tail_ref, long s_id, int s_help){

    struct list_node *new_node = new_list_node(s_id, s_help);
    if((*head_ref) == NULL && (*tail_ref) == NULL){
        (*head_ref) = new_node;
        (*tail_ref) = new_node;
    }
    else{
        new_node->next = *head_ref;
        (*head_ref)->prev = new_node;
        (*head_ref) = new_node;
    }
    return;
}

struct list_node *pop(struct list_node **head_ref, struct list_node **tail_ref){

    struct list_node *temp = (*tail_ref);
    if((*head_ref) == (*tail_ref)){
        (*head_ref) = NULL;
        (*tail_ref) = NULL;
    }
    else{
        (*tail_ref)->prev->next = NULL;
        (*tail_ref) = (*tail_ref)->prev;
    }
    return temp;
}

struct queue_node *new_queue_node(long s_id, int s_help){

    struct queue_node *new_node = (struct queue_node *)malloc(sizeof(struct queue_node));
    new_node->s_id = s_id;
    new_node->s_help = s_help;
    new_node->next = NULL;
    return new_node;
}

void enqueue(struct queue_node **head_ref, long s_id, int s_help){

    struct queue_node *temp = *head_ref;
    struct queue_node *new_node = new_queue_node(s_id, s_help);
    if(temp == NULL){
        *head_ref = new_node;
        return;
    }
    while(temp->next != NULL && new_node->s_help >= temp->s_help ){ //Student with lowest priority goes to the end of the queue.
        temp = temp->next;
    }
    if(temp->next == NULL){
        temp->next = new_node;
    }
    else{
        new_node->next = temp->next;
        temp->next = new_node;
    }
    return;
}

struct queue_node *dequeue(struct queue_node **head_ref){

    struct queue_node *temp = *head_ref; //head will have student with highest priority.
    (*head_ref) = (*head_ref)->next;
    return temp;
}

static void *student_thread(void *arg){

    long s_id = (long) arg;
    int time_to_sleep;
    while(1){
        //Check if the student has received enough help.
        if(student_array[s_id].s_help > num_help)
            return NULL;

        //Programming Time.
        time_to_sleep = random_number(0, 2000);
        time_to_sleep = time_to_sleep/1000000;
        sleep(time_to_sleep);

        //Check for availability of chair.
        sem_wait(&chair_lock);
        if(chairs_available <= 0){
            printf("S: Student %ld found no empty chair. Will try again later.\n", s_id);
            sem_post(&chair_lock);
            continue;
        }
        chairs_available--;
        printf("S: Student %ld takes a seat. Empty chairs = %d.\n", s_id, chairs_available);
        sem_post(&chair_lock);
    
        //Add the student to waiting list.
        sem_wait(&list_lock);
        push(&list_head, &list_tail, s_id, student_array[s_id].s_help);
        sem_post(&list_lock);

        //Notify Co-ordinator.
        sem_post(&wait_for_student);

        //Wait for tutor to start helping.
        sem_wait(&wait_for_tutor[s_id]);

        //Increase the chair count.
        sem_wait(&chair_lock);
        chairs_available++;
        sem_post(&chair_lock);

        //Time spent for tutoring the student.
        sleep(0.0002);

        printf("S: Student %ld received help from Tutor %ld.\n", s_id, student_array[s_id].t_id);

        //Increase the help count.
        student_array[s_id].s_help++;
    }
}

static void *coordinator_thread(void *arg){

    int tutor_index = 0, total_req = 0, i = 0;
    struct list_node *l_node;
    while(1){
        //Check if all requests are queued.
        if(total_req == num_help * num_students){
            while(1){ //No more requests to queue, wait here for all tutors to complete
                if(total_req == total_help){ //once all students are done taking help, signal all tutors to close
                    for(i = 0; i < num_tutors; i++){
                        tutors_close[i] = 1;
                        sem_post(&wait_for_coordinator[i]);
                    }
                    return NULL;
                }
                sleep(1);
            }
        }

        //Wait for student.
        sem_wait(&wait_for_student);

        //Get the student from waiting list into the queue.
        sem_wait(&list_lock);
        sem_wait(&queue_lock);
        l_node = pop(&list_head, &list_tail);
        enqueue(&queue_head, l_node->s_id, l_node->s_help);
        total_req++;
        printf("C: Student %ld with priority %d added to the queue. Waiting students now = %d. Total requests = %d.\n", l_node->s_id, l_node->s_help, total_chairs - chairs_available, total_req);
        free(l_node);
        sem_post(&queue_lock);
        sem_post(&list_lock);

        //Notify a tutor.
        sem_post(&wait_for_coordinator[tutor_index]);
        tutor_index = (tutor_index + 1) % num_tutors;
    }
}

static void *tutor_thread(void *arg){

    long t_id = (long) arg;
    struct queue_node *q_node;
    while(1){
        //Wait for co-ordinator.
        sem_wait(&wait_for_coordinator[t_id]);

        //Check if all students have received enough help.
        if(tutors_close[t_id] == 1) //Set by co-ordinator if all helps are done.
            return NULL;

        //Get the highest priority student from the queue.
        sem_wait(&queue_lock);
        q_node = dequeue(&queue_head);
        sem_post(&queue_lock);

        student_array[q_node->s_id].t_id = t_id;//Used by student to check which tutor helped.

        //Increase current help count.
        sem_wait(&current_help_lock);
        current_help++;
        sem_post(&current_help_lock);

        //Increase total help count.
        sem_wait(&total_help_lock);
        total_help++;
        sem_post(&total_help_lock);

        //Notify the student
        sem_post(&wait_for_tutor[q_node->s_id]);

        //Time spent for tutoring the student.
        sleep(0.0002);

        printf("T: Student %ld tutored by Tutor %ld. Students tutored now = %d. Total sessions tutored = %d.\n", q_node->s_id, t_id, current_help, total_help);
        free(q_node);
    
        //Decrease current help count.
        sem_wait(&current_help_lock);
        current_help--;
        sem_post(&current_help_lock);
    }
}

int random_number(int min_num, int max_num){

    int result = 0, low_num = 0, hi_num = 0;
    if (min_num < max_num){
        low_num = min_num;
        hi_num = max_num + 1;
    }
    else{
        low_num = max_num + 1;
        hi_num = min_num;
    }
    result = (rand() % (hi_num - low_num)) + low_num;
    return result;
}

int main(int argc, char **argv){

    if(argc != 5){
        printf("Usage : csmc #students #tutors #chairs #help\n");
        return 1;
    }

    //seed for random number.
    srand(time(0));

    //Store the arguments from commandline.
    num_students = atoi(argv[1]);
    num_tutors = atoi(argv[2]);
    chairs_available = atoi(argv[3]);
    num_help = atoi(argv[4]);

    if(num_students < 1 || num_tutors < 1 || chairs_available < 1 || num_help < 1){
        printf("Students, tutors, chairs and help should be atleast 1\n");
        return 1;
    }

    total_chairs = chairs_available;
    long i,j,k;
    
    //Allocate memory based on commandline arguments.
    pthread_t *s_thread_id = (pthread_t *)malloc(num_students * sizeof(pthread_t));
    pthread_t *t_thread_id = (pthread_t *)malloc(num_tutors * sizeof(pthread_t));
    pthread_t c_thread_id;

    tutors_close = (int *) malloc(num_tutors * sizeof(int));
    student_array = (struct student_node *) malloc(num_students * sizeof(struct student_node));
    wait_for_tutor = (sem_t *) malloc(num_students * sizeof(sem_t));
    wait_for_coordinator = (sem_t *) malloc(num_tutors * sizeof(sem_t));

    //Initialize help of all students to 1.
    for(i = 0; i < num_students; i++){
        student_array[i].s_id = i;
        student_array[i].s_help = 1;
    }

    //Initialize all locks to 1.
    sem_init(&chair_lock, 0, 1);
    sem_init(&list_lock, 0, 1);
    sem_init(&queue_lock, 0, 1);
    sem_init(&total_help_lock, 0, 1);
    sem_init(&current_help_lock, 0, 1);
  
    //Initialize all waits to 0.
    sem_init(&wait_for_student, 0, 0);
    for(j = 0; j < num_students; j++){
        sem_init(&wait_for_tutor[j], 0, 0);
    }
    for(k = 0; k < num_tutors; k++){
        sem_init(&wait_for_coordinator[k], 0, 0);
    }

    //Creation of students,co-ordinate  and tutors' threads.
    for(i = 0; i < num_students; i++){
        assert(pthread_create(&s_thread_id[i], NULL, student_thread, (void *) i) == 0);
    }
    assert(pthread_create(&c_thread_id, NULL, coordinator_thread, NULL) == 0);
    for(k = 0; k < num_tutors; k++){
        assert(pthread_create(&t_thread_id[k], NULL, tutor_thread, (void *) k) == 0);
    }

    //Wait for all threads to complete.
    for(k = 0; k < num_tutors; k++) {
        pthread_join(t_thread_id[k], NULL);
    }
    pthread_join(c_thread_id, NULL);
    for(i = 0; i < num_students; i++) {
        pthread_join(s_thread_id[i], NULL);
    }

    //Free dynamically allocated memory.
    free(tutors_close);
    free(student_array);
    free(wait_for_tutor);
    free(wait_for_coordinator);
    free(s_thread_id);
    free(t_thread_id);

    return 0;
}