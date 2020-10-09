#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
// #include "synch.h"
#include "pintos_thread.h"


// Basic info of the station
struct station {
    int total_seats_of_current;     // Total number of seats of the current arrival train
    int num_already_aboard;         // The number of passengers that has been aboard
    int num_already_seated;         // The number of passengers that has been seated
    int train_arrival;            // 0: No train arrives    1: Train already arrived
    int waiting_passenger;          // Number of passenger waiting at station
    struct lock action_lock;        // the only lock
    struct condition can_board_condition;      // condition indecates passenger can get on board
    struct condition can_leave_condition;       // condition indecates train can leave the station
    struct condition able_to_board_condition;       // condition indecates passenger has been seated
};

void station_init(struct station *station) {
    // init station
    station->total_seats_of_current = -1;
    station->num_already_aboard = -1;
    station->num_already_seated = -1;
    station->train_arrival = 0;
    station->waiting_passenger = 0;
    lock_init(&station->action_lock);
    cond_init(&station->can_board_condition);
    cond_init(&station->can_leave_condition);
    cond_init(&station->able_to_board_condition);
}

// Will be invoked when a train arrives
void station_load_train(struct station *station, int count) {
    printf("\n");
    printf("*** New Train Arrive ***\n");
    printf("*** %d Seats Available ***\n", count);

    // No passenger waiting
    if (station->waiting_passenger==0) {
        printf("*** Train Leaves ***\n\n");
        return ;
    }

    // No seats available
    if (count==0) {
        printf("*** Train Leaves ***\n\n");
        return ;
    }

    station->total_seats_of_current = count;
    station->num_already_aboard = 0;        // Empty train
    station->num_already_seated = 0;
    station->train_arrival = 1;         // Mark train arrive

    lock_acquire(&station->action_lock);

    // Call the passengers to get aboard
    cond_broadcast(&station->able_to_board_condition, &station->action_lock);

    // Waiting to leave station
    while(station->waiting_passenger!=0 && station->total_seats_of_current-station->num_already_seated>0) {
        cond_wait(&station->can_leave_condition, &station->action_lock);
    }

    // Leaving the station
    station->total_seats_of_current = -1;
    station->num_already_aboard = -1;
    station->num_already_seated = -1;
    station->train_arrival = 0;
    printf("*** Train Leaves ***\n");
    printf("*** %d passenger(s) still waiting ***\n\n", station->waiting_passenger);
    lock_release(&station->action_lock);
}

// Will be called when a passenger is seated
void station_on_board(struct station *station) {
    lock_acquire(&station->action_lock);

    station->num_already_seated++;
    station->waiting_passenger--;
    printf("New passenger on board  Current: %d / %d seated\n", station->num_already_seated, station->total_seats_of_current);

    // Call the train to check whether able to leave
    cond_signal(&station->can_leave_condition, &station->action_lock);

    lock_release(&station->action_lock);
}

// Will be invoked when a passenger robot arrives
void station_wait_for_train(struct station *station) {
    // New passenger arrive
    lock_acquire(&station->action_lock);
    station->waiting_passenger++;
    
    // Waiting until able to board
    while(station->train_arrival!=1 || station->total_seats_of_current<=station->num_already_aboard) {
        cond_wait(&station->able_to_board_condition, &station->action_lock);
    }

    // Getting seated
    station->num_already_aboard++;

    // Got onto the train, leaving function

    lock_release(&station->action_lock);
}

