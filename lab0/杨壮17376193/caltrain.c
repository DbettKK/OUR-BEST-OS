#include "pintos_thread.h"

struct station {
	// FILL ME IN
	struct lock threadLock;
	struct condition canTrainGo;
	struct condition isTrainFree;
	int remainedPassengers;
	int remainedSeat;
	int boardingPassengers;
};

void station_init(struct station *station){
	// FILL ME IN
	lock_init(&station->threadLock);
	cond_init(&station->canTrainGo);
	cond_init(&station->isTrainFree);
	station->remainedPassengers=0;
	station->remainedSeat=0;
    station->boardingPassengers=0;
}

void station_load_train(struct station *station, int count){
	// FILL ME IN
	lock_acquire(&station->threadLock);
	station->remainedSeat=count;
	//station->boardingPassengers=0;

	if(count==0){
		lock_release(&station->threadLock);
		return;
	}
	if((station->remainedSeat > 0 && station->remainedPassengers > 0) || station->boardingPassengers != 0)
		cond_broadcast(&station->isTrainFree,&station->threadLock);

	while((station->remainedSeat > 0 && station->remainedPassengers > 0) || station->boardingPassengers != 0)
		cond_wait(&station->canTrainGo,&station->threadLock);

	station->remainedSeat=0;
	lock_release(&station->threadLock);
}

void station_wait_for_train(struct station *station){
	// FILL ME IN
	lock_acquire(&station->threadLock);
	station->remainedPassengers++;
	while(station->remainedSeat==0)
		cond_wait(&station->isTrainFree,&station->threadLock);
    station->remainedPassengers--;
	station->remainedSeat--;
	station->boardingPassengers++;
	cond_broadcast(&station->isTrainFree,&station->threadLock);
	lock_release(&station->threadLock);
}

void station_on_board(struct station *station){
	// FILL ME IN
	lock_acquire(&station->threadLock);
	station->boardingPassengers--;
	if(station->remainedSeat==0 || station->remainedPassengers==0){
		cond_broadcast(&station->canTrainGo,&station->threadLock);
	}
	lock_release(&station->threadLock);
}
