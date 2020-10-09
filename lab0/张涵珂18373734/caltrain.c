#include "pintos_thread.h"

struct station {
	int empty_seats;
	int train_passengers;
	int wait_passengers;
	struct lock *lock;
	struct condition *isSeatsFulled;
	struct condition *isTrainArrived;
};

void
station_init(struct station *station)
{	
	station->isSeatsFulled = malloc(sizeof(struct condition));
	station->isTrainArrived = malloc(sizeof(struct condition));
	station->lock = malloc(sizeof(struct lock));
	station->wait_passengers = 0;
	station->train_passengers = 0;
	lock_init(station->lock);
	cond_init(station->isSeatsFulled);
	cond_init(station->isTrainArrived);
}

void
station_load_train(struct station *station, int count)
{
	lock_acquire(station->lock);
	station->empty_seats = count;
	while (station->train_passengers || (station->wait_passengers && station->empty_seats)) {
		// 有空座位也有没上车的乘客的时候 唤醒所有等车的乘客 上车 
		cond_broadcast(station->isTrainArrived, station->lock);
		// 当前线程阻塞 等着乘客都上车或者座位坐满 
		cond_wait(station->isSeatsFulled, station->lock);
	}
	// 下一辆车的座位重置 
	station->empty_seats = 0; 
	lock_release(station->lock);
}

void
station_wait_for_train(struct station *station)
{
	lock_acquire(station->lock);
	// 每进来一个线程 乘客数+1 
	station->wait_passengers++;
	while(!station->empty_seats){
		// 上车的人已经等于空座位数了 
		// 其他人等下一辆车 
		cond_wait(station->isTrainArrived, station->lock);
	}
	// 等待的乘客上车 
	station->train_passengers++;
	station->wait_passengers--;
	station->empty_seats--;
	lock_release(station->lock);
}

void
station_on_board(struct station *station)
{
	lock_acquire(station->lock);
	// 上车后 上车的人就座
	station->train_passengers--;
	
	// 坐满了或者乘客都上车了 火车发车 
	if (!station->train_passengers && (!station->empty_seats || !station->wait_passengers)){
		cond_signal(station->isSeatsFulled, station->lock);
	}
	lock_release(station->lock);
}
