#include "pintos_thread.h"

struct station
{
    int seats;
    int upload_p;
    int wait_p;
    struct condition closed;
    struct condition open;
    struct lock lock;
};

//initialize the station object
void station_init(struct station *station)
{
    station->upload_p = 0;
    station->wait_p = 0;
    cond_init(&station->closed);
    cond_init(&station->open);
    lock_init(&station->lock);
}

/*
When a train arrives in the station and has opened its doors, it invokes the function
where count indicates how many seats are available on the train. 
The function must not return until the train is satisfactorily loaded
 (all passengers are in their seats, and either the train is full or all waiting passengers have boarded).
*/

void station_load_train(struct station *station, int count)
{
    lock_acquire(&station->lock);
    station->seats = count;

    while ((station->seats > 0 && station->wait_p > 0) || station->upload_p > 0)
    {
        cond_broadcast(&station->open, &station->lock);
        cond_wait(&station->closed, &station->lock);
    }

    station->seats = 0;
    lock_release(&station->lock);
}

/*
When a passenger robot arrives in a station, it first invokes the function
This function must not return until a train is in the station (i.e., a call to station_load_train is in progress) 
and there are enough free seats on the train for this passenger to sit down.
Once this function returns, the passenger robot will move the passenger on board the train and into a seat.
*/
void station_wait_for_train(struct station *station)
{
    lock_acquire(&station->lock);
    station->wait_p++;

    while (station->seats == 0)
    {
        cond_wait(&station->open, &station->lock);
    }

    station->upload_p++;
    station->seats--;
    station->wait_p--;

    lock_release(&station->lock);
}

/*
Once the passenger is seated, it will call the function
station_on_board(struct station *station)
to let the train know that it's on board.
*/
void station_on_board(struct station *station)
{
    lock_acquire(&station->lock);
    station->upload_p--;

    if ((station->seats == 0 || station->wait_p == 0) && station->upload_p == 0)
    {
        cond_signal(&station->closed, &station->lock);
    }

    lock_release(&station->lock);
}
