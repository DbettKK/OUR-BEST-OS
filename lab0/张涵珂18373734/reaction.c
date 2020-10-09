#include "pintos_thread.h"

// Forward declaration. This function is implemented in reaction-runner.c,
// but you needn't care what it does. Just be sure it's called when
// appropriate within reaction_o()/reaction_h().
void make_water();

struct reaction {
	int h_num;
	struct lock *lock;
	struct condition *h;
	struct condition *o;
};

void
reaction_init(struct reaction *reaction)
{
	reaction->h = malloc(sizeof(struct condition));
	reaction->o = malloc(sizeof(struct condition));
	reaction->lock = malloc(sizeof(struct lock));
	reaction->h_num = 0;
	lock_init(reaction->lock);
	cond_init(reaction->h);
	cond_init(reaction->o);
}

void
reaction_h(struct reaction *reaction)
{
	lock_acquire(reaction->lock);
	// ����Hԭ�� 
	reaction->h_num++;
	// ���������߳̽��в�ˮ 
	cond_signal(reaction->o, reaction->lock);
	// ������� �������� 
	cond_wait(reaction->h, reaction->lock);
	lock_release(reaction->lock);
}

void
reaction_o(struct reaction *reaction)
{
	lock_acquire(reaction->lock);
	// ���Hԭ�Ӳ��� �������߳����� 
	while(reaction->h_num < 2){
		cond_wait(reaction->o, reaction->lock);
	}
	// ����ˮ 
	reaction->h_num -= 2;
	make_water();
	// ��������Hԭ�������߳� ��Ϊ��Ҫ����H�̲߳ź�һ��O�����߳�һ�𷵻� 
	cond_signal(reaction->h, reaction->lock);
	cond_signal(reaction->h, reaction->lock);
	lock_release(reaction->lock);
}
