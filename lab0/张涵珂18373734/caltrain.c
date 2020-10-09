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
		// �п���λҲ��û�ϳ��ĳ˿͵�ʱ�� �������еȳ��ĳ˿� �ϳ� 
		cond_broadcast(station->isTrainArrived, station->lock);
		// ��ǰ�߳����� ���ų˿Ͷ��ϳ�������λ���� 
		cond_wait(station->isSeatsFulled, station->lock);
	}
	// ��һ��������λ���� 
	station->empty_seats = 0; 
	lock_release(station->lock);
}

void
station_wait_for_train(struct station *station)
{
	lock_acquire(station->lock);
	// ÿ����һ���߳� �˿���+1 
	station->wait_passengers++;
	while(!station->empty_seats){
		// �ϳ������Ѿ����ڿ���λ���� 
		// �����˵���һ���� 
		cond_wait(station->isTrainArrived, station->lock);
	}
	// �ȴ��ĳ˿��ϳ� 
	station->train_passengers++;
	station->wait_passengers--;
	station->empty_seats--;
	lock_release(station->lock);
}

void
station_on_board(struct station *station)
{
	lock_acquire(station->lock);
	// �ϳ��� �ϳ����˾���
	station->train_passengers--;
	
	// �����˻��߳˿Ͷ��ϳ��� �𳵷��� 
	if (!station->train_passengers && (!station->empty_seats || !station->wait_passengers)){
		cond_signal(station->isSeatsFulled, station->lock);
	}
	lock_release(station->lock);
}
