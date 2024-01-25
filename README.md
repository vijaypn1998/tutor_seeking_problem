Project Description - Seeking Tutor Problem

Overview:
The computer science department operates a mentoring center (csmc) to aid undergraduate students with programming assignments. The center comprises a coordinator, tutors, and a waiting area with chairs. Students seek help, and tutors assist based on priority. The project involves implementing a solution using POSIX threads, mutex locks, and semaphores to synchronize the activities of the coordinator, tutors, and students.

Specifications:

    Students:
        Students arrive at the center, sit in an empty chair, and wait for tutoring.
        Priority is based on the number of times a student seeks help, with first-time visitors having the highest priority.
        Once a student reaches the specified number of tutor sessions, the student thread terminates.

    Coordinator:
        Queues students based on priority and notifies an idle tutor.
        Outputs information about student additions to the queue.

    Tutors:
        Wake up when notified by the coordinator, assist the highest-priority student, and wait for the next student.
        Outputs information about tutoring sessions.

Command Line Arguments:

    Passed as command line arguments to the executable csmc: csmc #students #tutors #chairs #help
        Example: csmc 10 3 4 5 or csmc 2000 10 20 4

Output:

    Student Thread Output:
        S: Student x takes a seat. Empty chairs = <# of empty chairs after student x took a seat>.
        S: Student x found no empty chair. Will try again later.
        S: Student x received help from Tutor y.

    Coordinator Thread Output:
        C: Student x with priority p added to the queue. Waiting students now = <# students waiting>. Total requests = <total # requests for tutoring>.

    Tutor Thread Output:
        T: Student x tutored by Tutor y. Students tutored now = <# students receiving help now>. Total sessions tutored = <total sessions tutored>.

Notes:

    Memory allocation for data structures is dynamic based on input parameters.
    Check and modify ~/.bashrc to adjust thread creation limits.
    Redirect output to a file for testing with a large number of threads.
    Print error messages on stderr.

Note:
Ensure proper error handling and adherence to project-specific guidelines.
